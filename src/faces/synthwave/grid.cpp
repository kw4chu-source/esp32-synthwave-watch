#include "grid.h"

#include <math.h>
#include "assets_gen.h"
#include "core/dma_buffers.h"
#include "core/pixel.h"
#include "face_config.h"
#include "scene.h"

namespace grid {
namespace {

constexpr int BAND_H = 25;               // 480*25 = pojemnosc bufora DMA
constexpr float FOCAL = GRID_H - 1;      // glebokosc z=1 -> dolna krawedz ekranu
constexpr float LINE_SPACING = 0.5f;     // odstep linii poziomych w glebokosci
constexpr float Z_FAR = 22.0f;           // dalej linie zlewaja sie z horyzontem
constexpr int LANES = 12;                // linie pionowe po kazdej stronie srodka
constexpr int LANE_W = 56;               // rozstaw linii pionowych przy dolnej krawedzi

uint16_t hColor, vColor, horizon0, horizon1;

// Dla wiersza siatki: 0..255 jak mocno widac linie (zanik przy horyzoncie)
inline uint8_t fadeForRow(int gy) {
  int v = 60 + gy * 195 / (GRID_H - 1);
  return uint8_t(v > 255 ? 255 : v);
}

void fillBackground(uint16_t* buf, int bandTop, int rows) {
  for (int r = 0; r < rows; r++) {
    const int gy = bandTop + r;
    uint16_t bg[4];
    px::ditherRow(assets::GROUND_RGB[gy], GRID_TOP + gy, bg);
    uint16_t* line = buf + r * SCREEN_W;
    for (int x = 0; x < SCREEN_W; x++) line[x] = bg[x & 3];
  }
}

void drawHorizontal(uint16_t* buf, int bandTop, int rows, float scroll) {
  const float phase = fmodf(scroll, LINE_SPACING);
  for (float z = 1.0f - phase; z < Z_FAR; z += LINE_SPACING) {
    if (z < 0.98f) continue;
    const int gy = int(lroundf(FOCAL / z));
    if (gy < bandTop || gy >= bandTop + rows || gy < 2) continue;
    const uint8_t a = uint8_t(fadeForRow(gy) * (1.0f - z / Z_FAR));
    uint16_t* line = buf + (gy - bandTop) * SCREEN_W;
    for (int x = 0; x < SCREEN_W; x++) line[x] = px::blend(line[x], hColor, a);
  }
}

void drawVertical(uint16_t* buf, int bandTop, int rows) {
  for (int r = 0; r < rows; r++) {
    const int gy = bandTop + r;
    if (gy < 2) continue;
    const uint8_t a = fadeForRow(gy);
    uint16_t* line = buf + r * SCREEN_W;
    for (int lane = -LANES; lane <= LANES; lane++) {
      // x w tym i nastepnym wierszu - rysujemy caly odcinek, zeby strome
      // linie przy krawedziach nie mialy dziur
      const int xa = SCREEN_W / 2 + lane * LANE_W * gy / (GRID_H - 1);
      const int xb = SCREEN_W / 2 + lane * LANE_W * (gy + 1) / (GRID_H - 1);
      int lo = xa < xb ? xa : xb, hi = xa < xb ? xb : xa;
      if (hi > lo) hi--;  // bez nakladania na kolejny wiersz
      if (hi < 0 || lo >= SCREEN_W) continue;
      if (lo < 0) lo = 0;
      if (hi >= SCREEN_W) hi = SCREEN_W - 1;
      for (int x = lo; x <= hi; x++) line[x] = px::blend(line[x], vColor, a);
    }
  }
}

}  // namespace

void begin() {
  hColor = px::rgb565(100, 100, 255);
  vColor = px::rgb565(255, 70, 200);
  horizon0 = px::rgb565(255, 255, 255);
  horizon1 = px::rgb565(180, 200, 255);
}

void render(float scroll) {
  for (int bandTop = 0; bandTop < GRID_H; bandTop += BAND_H) {
    const int rows = (bandTop + BAND_H <= GRID_H) ? BAND_H : GRID_H - bandTop;
    uint16_t* buf = dmaBuffers.acquire();
    fillBackground(buf, bandTop, rows);
    drawHorizontal(buf, bandTop, rows, scroll);
    drawVertical(buf, bandTop, rows);
    if (bandTop == 0) {
      for (int x = 0; x < SCREEN_W; x++) {
        buf[x] = horizon0;
        buf[SCREEN_W + x] = horizon1;
      }
    }
    scene.drawPrecip(0, GRID_TOP + bandTop, SCREEN_W, rows, buf);
    for (int i = 0; i < rows * SCREEN_W; i++) buf[i] = px::swap(buf[i]);
    dmaBuffers.push(0, GRID_TOP + bandTop, SCREEN_W, rows);
  }
}

}  // namespace grid
