// Tarcza "neon_rain": neonowy szyld z godzina nad miastem w deszczu.
//
// Animacje (tylko zmieniane prostokaty):
//  - deszcz: ~70 smug przesuwanych co klatke
//  - zmiana minuty: cyfra gasnie i zapala sie z zajaknieciem jak neon
//  - co kilka minut losowa cyfra "brzeczy" (przygasa na chwile)
//  - okna gasna/zapalaja sie, pas skanowania hologramu, falujace odbicia
//  - co 40-70 s przelatuje auto; o pelnej godzinie blyska burza

#include "core/face.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

#include "core/push.h"
#include "scene.h"

using namespace assets;
using scene::Lit;
using scene::st;

namespace face {
namespace {

// Kroki animacji cyfry: -1 w polu digit = poprzednia cyfra, -2 = nowa
struct Step {
  int8_t which;
  Lit lit;
};
constexpr Step TURN_ON[] = {{-1, Lit::Dim}, {-1, Lit::Off}, {-2, Lit::Dim},
                            {-2, Lit::On},  {-2, Lit::Dim}, {-2, Lit::On}};
constexpr Step BUZZ[] = {{-2, Lit::Dim}, {-2, Lit::On}, {-2, Lit::Dim}, {-2, Lit::Dim}, {-2, Lit::On}};

struct SlotAnim {
  const Step* steps = nullptr;
  uint8_t count = 0, pos = 0;
  int8_t from = -1, to = -1;
};
SlotAnim anim[4];

char dateShown[48] = "";
int lastHour = -1;
uint32_t nextBuzzMs = 0;
uint32_t nextCarMs = 0;
uint32_t frameNo = 0;
uint8_t flashStep = 0;

constexpr uint8_t FLASH_SEQ[] = {200, 0, 0, 140, 0};

// ---- prostokaty
void pushScene(int x, int y, int w, int h) { pushComposed(x, y, w, h, scene::compose); }
void pushSlot(int i) { pushScene(DIGIT_X[i], DIGIT_TOP, DIGIT_CELL_W, DIGIT_CELL_H); }
void pushDate() { pushScene(70, DATE_BAND_TOP, SCREEN_W - 140, DATE_BAND_BOTTOM - DATE_BAND_TOP); }

uint32_t rnd(uint32_t n) { return esp_random() % n; }

void respawnDrop(scene::Drop& d, bool anywhere) {
  d.len = 9 + rnd(9);
  d.vy = 13 + rnd(5);
  d.x = float(rnd(SCREEN_W + 40));
  d.y = anywhere ? float(rnd(SCREEN_H)) - 20 : -float(d.len + rnd(40));
}

// ---- cyfry
void startAnim(int i, const Step* steps, uint8_t count, int8_t from, int8_t to) {
  anim[i] = {steps, count, 0, from, to};
}

void stepSlot(int i) {
  SlotAnim& a = anim[i];
  if (!a.steps) return;
  const Step& s = a.steps[a.pos];
  st.slots[i].digit = s.which == -1 ? a.from : a.to;
  st.slots[i].lit = s.lit;
  if (++a.pos >= a.count) a.steps = nullptr;
  pushSlot(i);
}

void updateClock(uint32_t nowMs, const struct tm* now) {
  int8_t want[4] = {-1, -1, -1, -1};
  if (now) {
    want[0] = now->tm_hour / 10;
    want[1] = now->tm_hour % 10;
    want[2] = now->tm_min / 10;
    want[3] = now->tm_min % 10;
  }
  for (int i = 0; i < 4; i++) {
    if (anim[i].steps) continue;
    if (st.slots[i].digit != want[i])
      startAnim(i, TURN_ON, sizeof(TURN_ON) / sizeof(Step), st.slots[i].digit, want[i]);
  }

  // "brzeczacy" neon co 3-6 min
  if (now && int32_t(nowMs - nextBuzzMs) >= 0) {
    nextBuzzMs = nowMs + 180000 + rnd(180000);
    const int i = rnd(4);
    if (!anim[i].steps && st.slots[i].digit >= 0)
      startAnim(i, BUZZ, sizeof(BUZZ) / sizeof(Step), st.slots[i].digit, st.slots[i].digit);
  }
  for (int i = 0; i < 4; i++) stepSlot(i);

  char date[48] = "";
  if (now)
    snprintf(date, sizeof(date), "%s  %02d.%02d.%04d", WEEKDAYS[now->tm_wday], now->tm_mday,
             now->tm_mon + 1, now->tm_year + 1900);
  if (strcmp(date, dateShown) != 0) {
    strcpy(dateShown, date);
    scene::setDate(date);
    pushDate();
  }

  // burza o pelnej godzinie (nie przy pierwszym ustawieniu czasu)
  if (now) {
    if (lastHour >= 0 && now->tm_hour != lastHour) flashStep = 1;
    lastHour = now->tm_hour;
  }
}

// ---- tlo
void stepRain() {
  int ox0[scene::DROP_COUNT], oy0[scene::DROP_COUNT], ox1[scene::DROP_COUNT],
      oy1[scene::DROP_COUNT];
  for (int i = 0; i < scene::DROP_COUNT; i++) {
    scene::Drop& d = st.drops[i];
    scene::dropBounds(d, ox0[i], oy0[i], ox1[i], oy1[i]);
    d.y += d.vy;
    d.x -= d.vy * scene::DROP_SLOPE;
    if (d.y > SCREEN_H) respawnDrop(d, false);
  }
  // najpierw caly nowy stan, potem wysylka (stary + nowy obszar kazdej smugi)
  for (int i = 0; i < scene::DROP_COUNT; i++) {
    int x0, y0, x1, y1;
    scene::dropBounds(st.drops[i], x0, y0, x1, y1);
    if (oy0[i] > y1 + 40 || oy1[i] < y0 - 40) {  // respawn: dwa osobne prostokaty
      pushScene(ox0[i], oy0[i], ox1[i] - ox0[i] + 1, oy1[i] - oy0[i] + 1);
      pushScene(x0, y0, x1 - x0 + 1, y1 - y0 + 1);
    } else {
      const int ux0 = min(x0, ox0[i]), uy0 = min(y0, oy0[i]);
      pushScene(ux0, uy0, max(x1, ox1[i]) - ux0 + 1, max(y1, oy1[i]) - uy0 + 1);
    }
  }
}

void stepWindows() {
  if (rnd(100) >= 18) return;
  const int i = rnd(WINDOW_COUNT);
  st.windowDark[i] = !st.windowDark[i];
  pushScene(WINDOWS[i].x, WINDOWS[i].y, WINDOW_W, WINDOW_H);
}

void stepHologram() {
  if (frameNo % 2) return;
  const int old = st.holoBand;
  st.holoBand = old < 0 ? HOLO_Y0 : old + 2;
  if (st.holoBand + 3 > HOLO_Y1) st.holoBand = HOLO_Y0;
  if (old >= 0) pushScene(HOLO_X0, old, HOLO_X1 - HOLO_X0, 3);
  pushScene(HOLO_X0, st.holoBand, HOLO_X1 - HOLO_X0, 3);
}

void stepCar(uint32_t nowMs) {
  if (st.carX <= -CAR_W || st.carX >= SCREEN_W + 10) {
    if (int32_t(nowMs - nextCarMs) < 0) return;
    nextCarMs = nowMs + 40000 + rnd(30000);
    st.carX = SCREEN_W + 4;
  }
  const int old = st.carX;
  st.carX -= 5;
  pushScene(st.carX, CAR_Y, old - st.carX + CAR_W, CAR_H);
  if (st.carX <= -CAR_W) st.carX = -1000;
}

void stepRipple() {
  if (frameNo % 3) return;
  st.ripple++;
  pushScene(0, STREET_Y + 1, SCREEN_W, SCREEN_H - STREET_Y - 1);
}

void stepFlash() {
  if (!flashStep) return;
  st.flash = FLASH_SEQ[flashStep - 1];
  pushScene(0, 0, SCREEN_W, SCREEN_H);
  if (++flashStep > sizeof(FLASH_SEQ)) flashStep = 0;
}

}  // namespace

void begin() {
  for (auto& d : st.drops) respawnDrop(d, true);
  nextCarMs = millis() + 8000;  // pierwszy przelot zaraz po starcie
  nextBuzzMs = millis() + 120000;
}

void drawAll() { pushScene(0, 0, SCREEN_W, SCREEN_H); }

void frame(uint32_t nowMs, const struct tm* now) {
  frameNo++;
  updateClock(nowMs, now);
  stepWindows();
  stepHologram();
  stepCar(nowMs);
  stepRipple();
  stepRain();
  stepFlash();
}

}  // namespace face
