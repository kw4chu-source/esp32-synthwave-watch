// ILI9488 Custom Chip for Wokwi Simulator
// SPDX-License-Identifier: MIT
//
// ═══════════════════════════════════════════════════════════════════════════
// WHY PREVIOUS VERSIONS FAILED
// ═══════════════════════════════════════════════════════════════════════════
//
// TFT_eSPI drives CS in two distinct modes:
//
// WINDOW MODE  (standalone call, no startWrite/endWrite)
//   CS toggles around every logical group. DC is set BEFORE CS drops and
//   is stable for the entire CS window:
//     DC=0 → CS↓  [CMD byte]     CS↑
//     DC=1 → CS↓  [DATA bytes]   CS↑
//
// STREAM MODE  (inside startWrite/endWrite, or inTransaction=true)
//   CS is held low continuously; DC changes BETWEEN bytes mid-burst:
//     CS↓  DC=0 [CMD] DC=1 [DATA…] DC=0 [CMD] DC=1 [DATA/pixels…] CS↑
//
// v3 sampled DC once at CS↓ and applied it to the entire burst — correct for
// window mode but broken for stream mode (all bytes misclassified as data).
//
// v2 reset parser state to STATE_EXPECT_CMD on every CS↓ — correct for
// stream mode but broken for window mode (data windows misclassified as
// commands because state was wiped between the CMD and DATA CS windows).
//
// ═══════════════════════════════════════════════════════════════════════════
// SOLUTION: DC-EDGE FLUSH
// ═══════════════════════════════════════════════════════════════════════════
//
// Key: spi_stop() causes Wokwi to deliver all buffered-but-not-yet-reported
// SPI bytes synchronously via on_spi_done() before spi_stop() returns.
//
// We register a pin_watch on DC. When DC changes WHILE CS is asserted:
//   1. dc_current holds the OLD DC level (updated only AFTER spi_stop).
//   2. spi_stop() fires on_spi_done() synchronously — bytes are processed
//      under the OLD dc_current. ✓
//   3. dc_current is updated to the new level.
//   4. spi_start() resumes; subsequent bytes classified under new level. ✓
//
// This creates a clean byte boundary at every DC transition. on_spi_done
// always sees a buffer where every byte had the same DC level.
//
// The structural parser state persists across CS edges (NOT reset on CS↓),
// so window-mode CMD/DATA pairs across separate CS pulses work correctly.
//
// ═══════════════════════════════════════════════════════════════════════════
// PARTIAL-PIXEL FLUSH
// ═══════════════════════════════════════════════════════════════════════════
//
// drawPixel() without a WOKWI_SIMULATION guard sends 2-byte RGB565 after
// RAMWR instead of 3-byte RGB666. On DC 1→0 or CS↑ with pixel_idx==2 we
// recover the pixel by interpreting those 2 bytes as RGB565.

#include "wokwi-api.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#define DBG_INFO(fmt,...) //printf("[ILI9488][INFO] " fmt "\n", ##__VA_ARGS__)
#define DBG_VERB(fmt,...) //printf("[ILI9488][VERB] " fmt "\n", ##__VA_ARGS__)

#define DISPLAY_WIDTH   320
#define DISPLAY_HEIGHT  480
#define SPI_BUF_SIZE    4096

// ── Commands ──────────────────────────────────────────────────────────────────
#define CMD_NOP        0x00
#define CMD_SWRESET    0x01
#define CMD_SLPIN      0x10
#define CMD_SLPOUT     0x11
#define CMD_NORON      0x13
#define CMD_INVOFF     0x20
#define CMD_INVON      0x21
#define CMD_DISPOFF    0x28
#define CMD_DISPON     0x29
#define CMD_CASET      0x2A
#define CMD_PASET      0x2B
#define CMD_RAMWR      0x2C
#define CMD_MADCTL     0x36
#define CMD_PIXFMT     0x3A
#define CMD_WRMEMCONT  0x3C

// ── Data sub-state ────────────────────────────────────────────────────────────
typedef enum {
  DATA_IDLE,    // no data expected; next DC=0 burst brings a new command
  DATA_ARGS,    // consuming fixed-count argument bytes for current command
  DATA_PIXELS,  // pixel stream (RAMWR/WRMEMCONT); runs until DC or CS edge
} data_state_t;

// ── Chip state ────────────────────────────────────────────────────────────────
typedef struct {
  spi_dev_t spi;
  buffer_t  framebuffer;
  uint32_t  fb_width, fb_height;

  pin_t pin_cs, pin_dc, pin_rst, pin_bl;
  bool  cs_active;
  bool  spi_running;

  // DC level tracked by us — updated AFTER spi_stop() so on_spi_done always
  // sees the level that was active while the bytes were being clocked in.
  uint8_t dc_current;

  // Structural parser state — persists across CS edges (window mode)
  uint8_t      cmd;
  data_state_t data_state;
  int32_t      data_remain;   // bytes left in DATA_ARGS; -1 = unlimited pixels
  uint32_t     data_index;    // bytes consumed in current arg block

  // Display registers
  bool    display_on, sleep_mode, invert;
  uint8_t madctl, pixel_bytes;

  uint16_t col_start, col_end;
  uint16_t row_start, row_end;
  uint16_t cursor_x,  cursor_y;

  // Pixel assembly
  uint8_t pixel_buf[3];
  uint8_t pixel_idx;

  uint8_t spi_buf[SPI_BUF_SIZE];
} chip_state_t;

static chip_state_t chip;

// ── Data byte count per command ───────────────────────────────────────────────
static int32_t cmd_data_len(uint8_t cmd) {
  switch (cmd) {
    case CMD_NOP: case CMD_SWRESET: case CMD_SLPIN: case CMD_SLPOUT:
    case CMD_NORON: case CMD_INVOFF: case CMD_INVON:
    case CMD_DISPOFF: case CMD_DISPON:
      return 0;
    case CMD_CASET:     return 4;
    case CMD_PASET:     return 4;
    case CMD_MADCTL:    return 1;
    case CMD_PIXFMT:    return 1;
    case CMD_RAMWR:
    case CMD_WRMEMCONT: return -1;   // unlimited pixel stream
    default:            return 0;
  }
}

// ── Colour conversion ─────────────────────────────────────────────────────────
static inline uint32_t pack_rgba(uint8_t r, uint8_t g, uint8_t b) {
  // Wokwi framebuffer: 0xAA_BB_GG_RR little-endian
  return (0xFFu << 24) | ((uint32_t)b << 16) | ((uint32_t)g << 8) | r;
}
static inline uint32_t rgb565_to_rgba(uint16_t c) {
  uint8_t r = ((c >> 11) & 0x1F) << 3; r |= r >> 5;
  uint8_t g = ((c >>  5) & 0x3F) << 2; g |= g >> 6;
  uint8_t b = ( c        & 0x1F) << 3; b |= b >> 5;
  return pack_rgba(r, g, b);
}
static inline uint32_t rgb666_to_rgba(uint8_t r6, uint8_t g6, uint8_t b6) {
  // ILI9488: colour in bits [7:2]; bits [1:0] are don't-care
  uint8_t r = r6 & 0xFC; r |= r >> 6;
  uint8_t g = g6 & 0xFC; g |= g >> 6;
  uint8_t b = b6 & 0xFC; b |= b >> 6;
  return pack_rgba(r, g, b);
}

// ── Display helpers ───────────────────────────────────────────────────────────
static void reset_display(void) {
  chip.display_on  = false;
  chip.sleep_mode  = true;
  chip.invert      = false;
  chip.madctl      = 0;
  chip.pixel_bytes = 3;    // ILI9488 default: 18-bit / 3 bytes per pixel

  chip.col_start = 0;  chip.col_end  = DISPLAY_WIDTH  - 1;
  chip.row_start = 0;  chip.row_end  = DISPLAY_HEIGHT - 1;
  chip.cursor_x  = 0;  chip.cursor_y = 0;

  chip.cmd         = CMD_NOP;
  chip.data_state  = DATA_IDLE;
  chip.data_remain = 0;
  chip.data_index  = 0;
  chip.pixel_idx   = 0;

  DBG_INFO("RESET");
}

static void write_pixel(uint32_t rgba) {
  if (chip.display_on) {
    if (chip.invert) rgba ^= 0x00FFFFFFu;
    if (chip.cursor_x < chip.fb_width && chip.cursor_y < chip.fb_height) {
      uint32_t off = (chip.cursor_y * chip.fb_width + chip.cursor_x) * 4;
      buffer_write(chip.framebuffer, off, &rgba, 4);
    }
  }
  if (++chip.cursor_x > chip.col_end) {
    chip.cursor_x = chip.col_start;
    if (++chip.cursor_y > chip.row_end)
      chip.cursor_y = chip.row_start;
  }
}

// ── Partial-pixel flush ───────────────────────────────────────────────────────
static void flush_partial_pixel(void) {
  if (chip.data_state != DATA_PIXELS || chip.pixel_idx == 0) return;
  if (chip.pixel_idx == 2) {
    uint32_t color = rgb565_to_rgba(
        ((uint16_t)chip.pixel_buf[0] << 8) | chip.pixel_buf[1]);
    write_pixel(color);
    DBG_VERB("Partial pixel (2B→RGB565) flushed");
  }
  chip.pixel_idx = 0;
}

// ── Command decoder ───────────────────────────────────────────────────────────
static void execute_command(uint8_t cmd) {
  chip.cmd        = cmd;
  chip.data_index = 0;
  chip.pixel_idx  = 0;
  DBG_VERB("CMD 0x%02X", cmd);

  int32_t dlen = cmd_data_len(cmd);
  if (dlen == -1) {
    chip.data_state  = DATA_PIXELS;
    chip.data_remain = -1;
  } else if (dlen == 0) {
    chip.data_state  = DATA_IDLE;
    chip.data_remain = 0;
    switch (cmd) {
      case CMD_SWRESET: reset_display();         break;
      case CMD_SLPOUT:  chip.sleep_mode = false; break;
      case CMD_SLPIN:   chip.sleep_mode = true;  break;
      case CMD_DISPON:  chip.display_on = true;  break;
      case CMD_DISPOFF: chip.display_on = false; break;
      case CMD_INVON:   chip.invert     = true;  break;
      case CMD_INVOFF:  chip.invert     = false; break;
      default: break;
    }
  } else {
    chip.data_state  = DATA_ARGS;
    chip.data_remain = dlen;
  }
}

// ── Data byte consumer ────────────────────────────────────────────────────────
static void consume_data_byte(uint8_t byte) {
  switch (chip.data_state) {

    case DATA_IDLE:
      // Silently absorb — occurs during vendor init sequences with unknown cmds
      break;

    case DATA_ARGS: {
      uint32_t idx = chip.data_index++;
      switch (chip.cmd) {
        case CMD_CASET:
          if (idx == 0) chip.col_start  = (uint16_t)byte << 8;
          if (idx == 1) chip.col_start |= byte;
          if (idx == 2) chip.col_end    = (uint16_t)byte << 8;
          if (idx == 3) {
            chip.col_end |= byte;
            if (chip.col_end >= DISPLAY_WIDTH) chip.col_end = DISPLAY_WIDTH - 1;
            chip.cursor_x = chip.col_start;
            DBG_VERB("CASET %u..%u", chip.col_start, chip.col_end);
          }
          break;
        case CMD_PASET:
          if (idx == 0) chip.row_start  = (uint16_t)byte << 8;
          if (idx == 1) chip.row_start |= byte;
          if (idx == 2) chip.row_end    = (uint16_t)byte << 8;
          if (idx == 3) {
            chip.row_end |= byte;
            if (chip.row_end >= DISPLAY_HEIGHT) chip.row_end = DISPLAY_HEIGHT - 1;
            chip.cursor_y = chip.row_start;
            DBG_VERB("PASET %u..%u", chip.row_start, chip.row_end);
          }
          break;
        case CMD_MADCTL:
          if (idx == 0) { chip.madctl = byte; DBG_INFO("MADCTL=0x%02X", byte); }
          break;
        case CMD_PIXFMT:
          if (idx == 0) {
            chip.pixel_bytes = ((byte & 0x07) == 0x05) ? 2 : 3;
            DBG_INFO("PIXFMT: %s", chip.pixel_bytes == 2 ? "RGB565" : "RGB666");
          }
          break;
        default: break;
      }
      if (chip.data_remain > 0 && --chip.data_remain == 0)
        chip.data_state = DATA_IDLE;
      break;
    }

    case DATA_PIXELS:
      chip.pixel_buf[chip.pixel_idx++] = byte;
      if (chip.pixel_idx >= chip.pixel_bytes) {
        uint32_t color = (chip.pixel_bytes == 2)
          ? rgb565_to_rgba(
              ((uint16_t)chip.pixel_buf[0] << 8) | chip.pixel_buf[1])
          : rgb666_to_rgba(
              chip.pixel_buf[0], chip.pixel_buf[1], chip.pixel_buf[2]);
        write_pixel(color);
        chip.pixel_idx = 0;
      }
      break;
  }
}

// ── SPI done callback ─────────────────────────────────────────────────────────
// Fired by spi_stop() or when SPI_BUF_SIZE bytes accumulate.
// dc_current is guaranteed to be the level that was active for every byte in
// this buffer: on_dc_change calls spi_stop() BEFORE updating dc_current, so
// the flush here runs first under the old value.
static void on_spi_done(void *user_data, uint8_t *buf, uint32_t count) {
  (void)user_data;
  chip.spi_running = false;

  if (count > 0) {
    DBG_VERB("SPI %u bytes DC=%u state=%d [%02X %02X %02X...]",
             count, chip.dc_current, chip.data_state,
             buf[0], count > 1 ? buf[1] : 0, count > 2 ? buf[2] : 0);

    if (chip.dc_current == 0) {
      // Command window: first byte is the opcode
      execute_command(buf[0]);
      for (uint32_t i = 1; i < count; i++)
        consume_data_byte(buf[i]);
    } else {
      // Data window: all bytes are args or pixel data
      for (uint32_t i = 0; i < count; i++)
        consume_data_byte(buf[i]);
    }
  }

  if (chip.cs_active) {
    spi_start(chip.spi, chip.spi_buf, SPI_BUF_SIZE);
    chip.spi_running = true;
  }
}

// ── DC pin change callback ────────────────────────────────────────────────────
static void on_dc_change(void *user_data, pin_t pin, uint32_t value) {
  (void)user_data; (void)pin;

  if (!chip.cs_active) {
    // DC changed outside a CS transaction — track it and move on.
    chip.dc_current = (uint8_t)value;
    return;
  }

  // DC changed MID-BURST (stream mode). Flush buffered bytes under OLD level.
  DBG_VERB("DC %u→%u mid-burst flush", chip.dc_current, value);

  if (chip.spi_running) {
    chip.spi_running = false;
    spi_stop(chip.spi);
    // on_spi_done() has now run synchronously using the OLD dc_current. ✓
  }

  // On DC 1→0 transition, flush any partial pixel before switching to command mode.
  if (value == 0) {
    flush_partial_pixel();
  }

  // NOW update dc_current to the new level, after on_spi_done has consumed
  // all bytes that belonged to the old level.
  chip.dc_current = (uint8_t)value;

  // Resume reception under the new DC level.
  spi_start(chip.spi, chip.spi_buf, SPI_BUF_SIZE);
  chip.spi_running = true;
}

// ── CS / RST pin change callback ──────────────────────────────────────────────
static void on_pin_change(void *user_data, pin_t pin, uint32_t value) {
  (void)user_data;

  if (pin == chip.pin_rst) {
    if (value == 0) { DBG_INFO("HW RESET"); reset_display(); }
    return;
  }

  if (pin == chip.pin_cs) {
    if (value == 0) {
      // CS asserted. Sample DC now — it was settled before CS dropped.
      chip.dc_current = pin_read(chip.pin_dc);
      chip.cs_active  = true;
      // Structural parser state is NOT reset here — it must survive across
      // the CMD/DATA CS-pulse pairs of window mode.
      DBG_VERB("CS LOW  DC=%u state=%d", chip.dc_current, chip.data_state);
      spi_start(chip.spi, chip.spi_buf, SPI_BUF_SIZE);
      chip.spi_running = true;
    } else {
      // CS deasserted.
      chip.cs_active = false;
      if (chip.spi_running) {
        chip.spi_running = false;
        spi_stop(chip.spi);
        // on_spi_done fires synchronously for remaining bytes
      }
      flush_partial_pixel();
      DBG_VERB("CS HIGH state=%d", chip.data_state);
    }
  }
}

// ── Chip initialisation ───────────────────────────────────────────────────────
void chip_init(void) {
  chip.framebuffer = framebuffer_init(&chip.fb_width, &chip.fb_height);
  DBG_INFO("Framebuffer %ux%u", chip.fb_width, chip.fb_height);

  chip.pin_cs  = pin_init("CS",  INPUT_PULLUP);
  chip.pin_dc  = pin_init("DC",  INPUT);
  chip.pin_rst = pin_init("RST", INPUT_PULLUP);
  chip.pin_bl  = pin_init("BL",  INPUT_PULLUP);

  const pin_watch_config_t cs_watch = {
    .user_data  = NULL,
    .edge       = BOTH,
    .pin_change = on_pin_change,
  };
  pin_watch(chip.pin_cs,  &cs_watch);
  pin_watch(chip.pin_rst, &cs_watch);

  // DC watcher creates flush boundaries at every DC transition mid-burst
  const pin_watch_config_t dc_watch = {
    .user_data  = NULL,
    .edge       = BOTH,
    .pin_change = on_dc_change,
  };
  pin_watch(chip.pin_dc, &dc_watch);

  const spi_config_t spi_cfg = {
    .user_data = NULL,
    .sck       = pin_init("SCK",  INPUT),
    .mosi      = pin_init("MOSI", INPUT),
    .miso      = NO_PIN,
    .mode      = 0,
    .done      = on_spi_done,
  };
  chip.spi = spi_init(&spi_cfg);

  reset_display();
  DBG_INFO("Ready (DC-edge-flush + structural parser)");
}