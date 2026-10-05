#include "core/face.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include "core/dma_buffers.h"
#include "core/pixel.h"
#include "core/weather.h"
#include "face_config.h"
#include "grid.h"
#include "scene.h"

using namespace assets;

namespace face {
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

// ---- pogoda ----
uint32_t wxSerial = 0xFFFFFFFF;
bool wxValid = false, storm = false;
uint32_t frameNo = 0, nextBoltMs = 0;
uint8_t boltFrames = 0;
int boltMinX = 0, boltMaxX = 0;

void respawnDrop(Scene::Drop& d, bool anywhere) {
  if (scene.snow) {
    d.len = 2;
    d.vy = 1.0f + (esp_random() % 12) / 10.0f;
  } else {
    d.len = 8 + esp_random() % 7;
    d.vy = storm ? 13.0f + esp_random() % 4 : 9.0f + esp_random() % 4;
  }
  d.x = float(esp_random() % (SCREEN_W + 60));
  d.y = anywhere ? float(esp_random() % SCREEN_H) - 10 : -float(d.len + esp_random() % 30);
}

void applyWeather(const weather::Data& d) {
  using weather::Kind;
  const Kind k = d.valid ? d.kind : Kind::None;
  storm = k == Kind::Storm;
  const bool cloudy = k == Kind::Clouds || k == Kind::Rain || k == Kind::Snow || k == Kind::Storm || k == Kind::Fog;
  scene.skyDim = !cloudy ? 255 : (k == Kind::Clouds || k == Kind::Fog ? 200 : 150);
  scene.fog = k == Kind::Fog;
  scene.cloudCount = cloudy && k != Kind::Fog ? Scene::MAX_CLOUDS : 0;
  static const int16_t CX[] = {77, 207, 345, -10}, CY[] = {120, 102, 137, 112};
  static const uint8_t SH[] = {0, 0, 1, 1};
  for (int i = 0; i < Scene::MAX_CLOUDS; i++) scene.clouds[i] = {CX[i], CY[i], SH[i]};
  scene.snow = k == Kind::Snow;
  scene.slope = scene.snow ? 0.0f : (storm ? 0.5f : 0.3f);
  if (k == Kind::Rain) scene.dropCount = d.intensity == 1 ? 30 : (d.intensity == 2 ? 55 : 80);
  else if (k == Kind::Storm) scene.dropCount = Scene::MAX_DROPS;
  else if (k == Kind::Snow) scene.dropCount = d.intensity == 1 ? 35 : (d.intensity == 2 ? 60 : 85);
  else scene.dropCount = 0;
  for (int i = 0; i < scene.dropCount; i++) respawnDrop(scene.drops[i], true);
  switch (k) {
    case Kind::Clear: scene.wxIcon = d.day ? 0 : 1; break;
    case Kind::Clouds: scene.wxIcon = 2; break;
    case Kind::Rain: scene.wxIcon = 3; break;
    case Kind::Snow: scene.wxIcon = 4; break;
    case Kind::Storm: scene.wxIcon = 5; break;
    case Kind::Fog: scene.wxIcon = 6; break;
    default: scene.wxIcon = -1; break;
  }
  char t[16] = "";
  if (d.valid) {
    weather::formatTemp(d, t, sizeof(t));
    strlcat(t, "\xC2\xB0" "C", sizeof(t));
  }
  scene.setWeatherText(t);
  pushRect(0, 0, SCREEN_W, HORIZON_Y);  // cale niebo (kolor, chmury, pasek)
}

void stepWeather(uint32_t nowMs) {
  frameNo++;
  if (frameNo % 50 == 1) {
    const weather::Data d = weather::get();
    if (d.valid != wxValid || (d.valid && d.serial != wxSerial)) {
      wxValid = d.valid;
      wxSerial = d.serial;
      applyWeather(d);
    }
  }
  // chmury dryfuja w lewo (1 px co 3 klatki)
  if (scene.cloudCount && frameNo % 3 == 0)
    for (int i = 0; i < scene.cloudCount; i++) {
      Scene::Cloud& c = scene.clouds[i];
      c.x--;
      if (c.x < -CLOUD_W[c.shape]) c.x = SCREEN_W + int(esp_random() % 40);
      pushRect(c.x < 0 ? 0 : c.x, c.y, CLOUD_W[c.shape] + 1, CLOUD_H[c.shape]);
    }
  // opad nad horyzontem (ponizej rysuje go siatka)
  for (int i = 0; i < scene.dropCount; i++) {
    Scene::Drop& d = scene.drops[i];
    int ox0, oy0, ox1, oy1, nx0, ny0, nx1, ny1;
    scene.dropBounds(d, ox0, oy0, ox1, oy1);
    d.y += d.vy;
    d.x -= d.vy * scene.slope;
    if (scene.snow && (esp_random() & 3) == 0) d.x += float(int(esp_random() % 3) - 1);
    if (d.y > SCREEN_H) respawnDrop(d, false);
    scene.dropBounds(d, nx0, ny0, nx1, ny1);
    auto push = [](int x0, int y0, int x1, int y1) {
      if (y0 >= HORIZON_Y || y1 < 0) return;
      if (y1 >= HORIZON_Y) y1 = HORIZON_Y - 1;
      if (y0 < 0) y0 = 0;
      if (x0 < 0) x0 = 0;
      if (x1 >= SCREEN_W) x1 = SCREEN_W - 1;
      if (x1 >= x0) pushRect(x0, y0, x1 - x0 + 1, y1 - y0 + 1);
    };
    if (ny0 > oy1 + 30 || ny1 < oy0 - 30) {
      push(ox0, oy0, ox1, oy1);
      push(nx0, ny0, nx1, ny1);
    } else {
      push(ox0 < nx0 ? ox0 : nx0, oy0 < ny0 ? oy0 : ny0, ox1 > nx1 ? ox1 : nx1, oy1 > ny1 ? oy1 : ny1);
    }
  }
  // burza: piorun przez 3 klatki co 15-40 s
  if (boltFrames) {
    if (--boltFrames == 0) {
      scene.bolt = false;
      pushRect(boltMinX, 0, boltMaxX - boltMinX, HORIZON_Y);
    }
  } else if (storm && int32_t(nowMs - nextBoltMs) >= 0) {
    nextBoltMs = nowMs + 15000 + esp_random() % 25000;
    int x = 40 + esp_random() % (SCREEN_W - 80);
    boltMinX = x;
    boltMaxX = x;
    for (auto& bx : scene.boltX) {
      bx = int16_t(x);
      if (x < boltMinX) boltMinX = x;
      if (x > boltMaxX) boltMaxX = x;
      x += int(esp_random() % 25) - 12;
    }
    boltMinX = boltMinX - 3 < 0 ? 0 : boltMinX - 3;
    boltMaxX = boltMaxX + 4 > SCREEN_W ? SCREEN_W : boltMaxX + 4;
    scene.bolt = true;
    boltFrames = 3;
    pushRect(boltMinX, 0, boltMaxX - boltMinX, HORIZON_Y);
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
  stepWeather(nowMs);
  grid::render(gridScroll);
}

}  // namespace face
