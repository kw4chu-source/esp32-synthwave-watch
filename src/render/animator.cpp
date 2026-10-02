#include "animator.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "dma_buffers.h"
#include "grid.h"
#include "pixel.h"
#include "scene.h"

using namespace assets;

namespace animator {
namespace {

constexpr float GRID_SPEED = 0.9f;            // jednostki glebokosci na sekunde
constexpr uint32_t GLITCH_PERIOD_MS = 30000;  // srednio co ~30 s (+/- 6 s)
constexpr uint8_t GLITCH_FRAMES = 4;
constexpr int GLITCH_MAX_STRIPS = 5;

float gridScroll = 0;
uint32_t lastMs = 0;

uint8_t starShown[STAR_COUNT];
char dateShown[48] = "";

struct Strip {
  int16_t y, h, dx;
};
Strip strips[GLITCH_MAX_STRIPS];
int stripCount = 0;
uint8_t glitchFramesLeft = 0;
uint32_t nextGlitchMs = 0;

// Sklada i wysyla prostokat sceny w kawalkach mieszczacych sie w buforze DMA
void pushRect(int x, int y, int w, int h) {
  if (w <= 0 || h <= 0) return;
  const int rowsPerChunk = DmaBuffers::CAPACITY_PX / w;
  for (int y0 = y; y0 < y + h; y0 += rowsPerChunk) {
    const int rows = (y0 + rowsPerChunk <= y + h) ? rowsPerChunk : (y + h - y0);
    uint16_t* buf = dmaBuffers.acquire();
    scene.compose(x, y0, w, rows, buf);
    dmaBuffers.push(x, y0, w, rows);
  }
}

void pushDigitSlot(int slot) {
  pushRect(Scene::slotX(slot), DIGIT_TOP, DIGIT_CELL_W, DIGIT_CELL_H);
}

// ---- slonce: tylko wiersze, w ktorych maska przeciec sie zmienila ----
void stepSun() {
  const uint8_t prev = scene.sunFrame;
  scene.sunFrame = (scene.sunFrame + 1) % SUN_FRAMES;
  const int sunTop = SUN_CY - SUN_R;
  int run = -1;
  for (int i = SUN_SLICE_ZONE_TOP; i <= SUN_R; i++) {
    const bool changed = i < SUN_R && SUN_GAP[prev][i] != SUN_GAP[scene.sunFrame][i];
    if (changed && run < 0) run = i;
    if (!changed && run >= 0) {
      const int hw = SUN_HALFW[i - 1] > SUN_HALFW[run] ? SUN_HALFW[i - 1] : SUN_HALFW[run];
      pushRect(SUN_CX - hw, sunTop + run, 2 * hw, i - run);
      run = -1;
    }
  }
}

void stepStars() {
  scene.starTick++;
  for (int i = 0; i < STAR_COUNT; i++) {
    const uint8_t l = scene.starLevel(i);
    if (l == starShown[i]) continue;
    starShown[i] = l;
    pushRect(STARS[i].x, STARS[i].y, STARS[i].size, STARS[i].size);
  }
}

// ---- glitch VHS: poziome paski przesuniete z rozjechanym kanalem R ----
void pushGlitchStrip(const Strip& s) {
  uint16_t* buf = dmaBuffers.acquire();
  scene.compose(0, s.y, SCREEN_W, s.h, buf);
  static uint16_t row[SCREEN_W];
  for (int r = 0; r < s.h; r++) {
    uint16_t* line = buf + r * SCREEN_W;
    memcpy(row, line, sizeof(row));
    for (int x = 0; x < SCREEN_W; x++) {
      const uint16_t base = px::swap(row[(x - s.dx + SCREEN_W) % SCREEN_W]);
      const uint16_t red = px::swap(row[(x - s.dx - 4 + 2 * SCREEN_W) % SCREEN_W]);
      line[x] = px::swap((base & 0x07FF) | (red & 0xF800));
    }
  }
  dmaBuffers.push(0, s.y, SCREEN_W, s.h);
}

void restoreStrips() {
  for (int i = 0; i < stripCount; i++) pushRect(0, strips[i].y, SCREEN_W, strips[i].h);
  stripCount = 0;
}

void stepGlitch(uint32_t nowMs) {
  if (!glitchFramesLeft) {
    if (int32_t(nowMs - nextGlitchMs) < 0) return;
    glitchFramesLeft = GLITCH_FRAMES;
    nextGlitchMs = nowMs + GLITCH_PERIOD_MS - 6000 + (esp_random() % 12000);
  }
  restoreStrips();
  if (--glitchFramesLeft == 0) return;  // ostatnia klatka = czysty obraz

  stripCount = 2 + esp_random() % (GLITCH_MAX_STRIPS - 1);
  for (int i = 0; i < stripCount; i++) {
    Strip& s = strips[i];
    s.h = 3 + esp_random() % 9;
    s.y = DATE_BOTTOM + esp_random() % (HORIZON_Y - DATE_BOTTOM - s.h);
    const int mag = 6 + esp_random() % 20;
    s.dx = (esp_random() & 1) ? mag : -mag;
    pushGlitchStrip(s);
  }
}

// ---- czas: cyfry i data ----
void updateClock(const struct tm* now) {
  int8_t want[Scene::DIGIT_SLOTS] = {-1, -1, -1, -1};
  if (now) {
    want[0] = now->tm_hour / 10;
    want[1] = now->tm_hour % 10;
    want[2] = now->tm_min / 10;
    want[3] = now->tm_min % 10;
  }
  for (int i = 0; i < Scene::DIGIT_SLOTS; i++) {
    if (scene.slotAnimating(i)) {
      scene.advanceTransition(i);
      pushDigitSlot(i);
    } else if (scene.digit(i) != want[i]) {
      scene.setDigit(i, want[i], true);
      pushDigitSlot(i);
    }
  }

  char date[48] = "";
  if (now)
    snprintf(date, sizeof(date), "%s  %02d.%02d.%04d", WEEKDAYS[now->tm_wday], now->tm_mday,
             now->tm_mon + 1, now->tm_year + 1900);
  if (strcmp(date, dateShown) != 0) {
    strcpy(dateShown, date);
    scene.setDate(date);
    pushRect(0, 0, SCREEN_W, DATE_BOTTOM);
  }
}

}  // namespace

void begin() {
  scene.begin();
  grid::begin();
  for (int i = 0; i < STAR_COUNT; i++) starShown[i] = scene.starLevel(i);
  nextGlitchMs = millis() + GLITCH_PERIOD_MS;
}

void drawAll() {
  pushRect(0, 0, SCREEN_W, HORIZON_Y);
  grid::render(gridScroll);
}

void frame(uint32_t nowMs, const struct tm* now) {
  const uint32_t dt = lastMs ? nowMs - lastMs : 0;
  lastMs = nowMs;
  gridScroll += GRID_SPEED * dt / 1000.0f;
  if (gridScroll > 1000.0f) gridScroll -= 1000.0f;

  updateClock(now);
  stepSun();
  stepStars();
  stepGlitch(nowMs);
  grid::render(gridScroll);
}

}  // namespace animator
