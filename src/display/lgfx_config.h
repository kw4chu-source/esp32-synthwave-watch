#pragma once
// Konfiguracja LovyanGFX dla ILI9488 3.5" (SPI, VSPI, DMA).
// DMA przez LovyanGFX jest bezpieczne; TFT_eSPI+DMA psulo flash na tym sprzecie.

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "config.h"

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9488 _panel;
  lgfx::Bus_SPI _bus;

public:
  LGFX() {
    {
      auto cfg = _bus.config();
      cfg.spi_host = VSPI_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = TFT_SPI_WRITE_HZ;
      cfg.freq_read = TFT_SPI_READ_HZ;
      cfg.spi_3wire = false;
      cfg.use_lock = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk = PIN_TFT_SCLK;
      cfg.pin_mosi = PIN_TFT_MOSI;
      cfg.pin_miso = PIN_TFT_MISO;
      cfg.pin_dc = PIN_TFT_DC;
      _bus.config(cfg);
      _panel.setBus(&_bus);
    }
    {
      auto cfg = _panel.config();
      cfg.pin_cs = PIN_TFT_CS;
      cfg.pin_rst = PIN_TFT_RST;
      cfg.pin_busy = -1;
      cfg.panel_width = 320;
      cfg.panel_height = 480;
      cfg.readable = true;
      cfg.invert = false;
      cfg.rgb_order = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = true;  // magistrala dzielona z dotykiem/SD (nieuzywane)
      _panel.config(cfg);
    }
    setPanel(&_panel);
  }
};
