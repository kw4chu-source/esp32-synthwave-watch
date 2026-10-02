// Tarcza "night_window": widok przez okno w deszczowa noc.
//
// Pod latarnia stoi postac. Przy kazdej zmianie minuty latarnia mruga,
// a w ciemnosci postac podchodzi blizej (klatka = minuta: 0 pod latarnia,
// 59 tuz za szyba, z blyskiem oczu). O pelnej godzinie latarnia gasnie,
// postac znika i po chwili znow stoi pod latarnia.
// Ciagle: deszcz za szyba, krople splywajace po szybie, okno naprzeciwko
// czasem gasnie, latarnia czasem migocze sama.

#include "core/face.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

#include "core/push.h"
#include "scene.h"

using namespace assets;
using scene::st;

namespace face {
namespace {

char dateShown[48] = "";
int8_t lastMinute = -1;
uint32_t nextFlickerMs = 0, nextLitMs = 0;

// Sekwencja latarni: kazdy krok = 1 klatka; MOVE = tu (w ciemnosci) postac sie przesuwa
enum Lamp : uint8_t { ON, OFF, MOVE, HIDE, BACK };
constexpr Lamp MINUTE_SEQ[] = {OFF, OFF, MOVE, OFF, ON, OFF, ON};
constexpr Lamp FLICKER_SEQ[] = {OFF, ON, OFF, OFF, ON};
constexpr Lamp HOUR_SEQ[] = {OFF, OFF, HIDE, OFF, OFF, OFF, OFF, OFF, OFF, OFF, OFF, OFF,
                             OFF, OFF, BACK, ON, OFF, OFF, ON, OFF, ON};
const Lamp* seq = nullptr;
uint8_t seqLen = 0, seqPos = 0;
int8_t moveTo = 0;

uint32_t rnd(uint32_t n) { return esp_random() % n; }

void pushScene(int x, int y, int w, int h) { pushComposed(x, y, w, h, scene::compose); }
void pushLamp() { pushScene(LAMP_X, LAMP_Y, LAMP_W, LAMP_H); }
void pushFigure(int8_t f) {
  if (f < 0) return;
  pushScene(FIGURE[f].x, FIGURE[f].y, FIGURE[f].w, FIGURE[f].h);
}

void startSeq(const Lamp* s, uint8_t n) {
  seq = s;
  seqLen = n;
  seqPos = 0;
}

void setFigure(int8_t f) {
  const int8_t old = st.figure;
  st.figure = f;
  pushFigure(old);
  pushFigure(f);
}

void stepSeq() {
  if (!seq) return;
  const Lamp l = seq[seqPos];
  const bool off = l != ON;
  if (off != st.lampOff) {
    st.lampOff = off;
    pushLamp();
  }
  if (l == MOVE || l == BACK) setFigure(moveTo);
  if (l == HIDE) setFigure(-1);
  if (++seqPos >= seqLen) seq = nullptr;
}

void respawnRain(scene::Rain& r, bool anywhere) {
  r.len = 7 + rnd(6);
  r.vy = 9 + rnd(4);
  r.x = float(FRAME + rnd(SCREEN_W - FRAME));
  r.y = anywhere ? float(rnd(SCREEN_H)) : -float(r.len + rnd(30));
}

void respawnRunner(scene::Runner& r, bool anywhere) {
  r.r = 2 + rnd(2);
  r.x = float(FRAME + 6 + rnd(SCREEN_W - 2 * FRAME - 12));
  r.y = anywhere ? float(FRAME + rnd(SCREEN_H - 2 * FRAME)) : float(FRAME + 4);
}

// ---- czas
void updateClock(const struct tm* now) {
  int8_t want[4] = {-1, -1, -1, -1};
  if (now) {
    want[0] = now->tm_hour / 10;
    want[1] = now->tm_hour % 10;
    want[2] = now->tm_min / 10;
    want[3] = now->tm_min % 10;
  }
  for (int i = 0; i < 4; i++) {
    if (st.digits[i] == want[i]) continue;
    st.digits[i] = want[i];
    pushScene(DIGIT_X[i], DIGIT_Y, DIGIT_CELL_W, DIGIT_CELL_H);
  }

  char date[48] = "";
  if (now)
    snprintf(date, sizeof(date), "%s %02d.%02d.%04d", WEEKDAYS[now->tm_wday], now->tm_mday,
             now->tm_mon + 1, now->tm_year + 1900);
  if (strcmp(date, dateShown) != 0) {
    strcpy(dateShown, date);
    scene::setDate(date);
    pushScene(40, DATE_BASELINE - 26, SCREEN_W - 80, 34);
  }

  if (!now || seq) return;
  const int8_t minute = now->tm_min;
  if (minute == lastMinute) return;
  const bool first = lastMinute < 0;
  const bool hourTurn = lastMinute > 0 && minute == 0;
  lastMinute = minute;
  moveTo = minute;
  if (first) {
    setFigure(minute);
  } else if (hourTurn) {
    startSeq(HOUR_SEQ, sizeof(HOUR_SEQ));
  } else {
    startSeq(MINUTE_SEQ, sizeof(MINUTE_SEQ));
  }
}

// ---- tlo
void stepRain() {
  int ox0[scene::RAIN_COUNT], oy0[scene::RAIN_COUNT], ox1[scene::RAIN_COUNT], oy1[scene::RAIN_COUNT];
  for (int i = 0; i < scene::RAIN_COUNT; i++) {
    scene::Rain& r = st.rain[i];
    scene::rainBounds(r, ox0[i], oy0[i], ox1[i], oy1[i]);
    r.y += r.vy;
    r.x -= r.vy * scene::RAIN_SLOPE;
    if (r.y > SCREEN_H) respawnRain(r, false);
  }
  for (int i = 0; i < scene::RAIN_COUNT; i++) {
    int x0, y0, x1, y1;
    scene::rainBounds(st.rain[i], x0, y0, x1, y1);
    if (oy0[i] > y1 + 30 || oy1[i] < y0 - 30) {
      pushScene(ox0[i], oy0[i], ox1[i] - ox0[i] + 1, oy1[i] - oy0[i] + 1);
      pushScene(x0, y0, x1 - x0 + 1, y1 - y0 + 1);
    } else {
      const int ux0 = min(x0, ox0[i]), uy0 = min(y0, oy0[i]);
      pushScene(ux0, uy0, max(x1, ox1[i]) - ux0 + 1, max(y1, oy1[i]) - uy0 + 1);
    }
  }
}

void stepRunners() {
  for (auto& r : st.runners) {
    // krople ruszaja zrywami: czasem stoja, czasem szybko zjezdzaja
    const uint32_t m = rnd(10);
    const float dy = m < 4 ? 0.0f : (m < 8 ? 1.5f : 4.0f);
    if (dy == 0) continue;
    const int ox = int(r.x), oy = int(r.y);
    r.y += dy;
    r.x += (rnd(3) == 0) ? (rnd(2) ? 0.5f : -0.5f) : 0.0f;
    if (r.y > SCREEN_H - FRAME - 4) {
      const int rr = r.r;
      respawnRunner(r, false);
      pushScene(ox - rr - 1, oy - rr - 1, 2 * rr + 3, 2 * rr + 3);
      pushScene(int(r.x) - r.r - 1, int(r.y) - r.r - 1, 2 * r.r + 3, 2 * r.r + 3);
      continue;
    }
    const int x0 = min(ox, int(r.x)) - r.r - 1, y0 = oy - r.r - 1;
    pushScene(x0, y0, 2 * r.r + 4, int(r.y) - oy + 2 * r.r + 3);
  }
}

void stepAmbient(uint32_t nowMs) {
  if (!seq && int32_t(nowMs - nextFlickerMs) >= 0) {
    nextFlickerMs = nowMs + 25000 + rnd(25000);
    startSeq(FLICKER_SEQ, sizeof(FLICKER_SEQ));
  }
  if (int32_t(nowMs - nextLitMs) >= 0) {
    nextLitMs = nowMs + 40000 + rnd(50000);
    st.litOff = !st.litOff;
    pushScene(LIT_X0, LIT_Y0, LIT_X1 - LIT_X0, LIT_Y1 - LIT_Y0);
  }
}

}  // namespace

void begin() {
  for (auto& r : st.rain) respawnRain(r, true);
  for (auto& r : st.runners) respawnRunner(r, true);
  const uint32_t now = millis();
  nextFlickerMs = now + 12000;
  nextLitMs = now + 30000;
}

void drawAll() { pushScene(0, 0, SCREEN_W, SCREEN_H); }

void frame(uint32_t nowMs, const struct tm* now) {
  updateClock(now);
  stepSeq();
  stepAmbient(nowMs);
  stepRain();
  stepRunners();
}

}  // namespace face
