// Tarcza "vhs_hallway": nagranie VHS z korytarza.
//
// Postac na koncu korytarza zbliza sie z kazda minuta (klatka = minuta),
// przeskakujac zawsze w chwili zaklocenia tasmy - nigdy nie widac, jak idzie.
// O pelnej godzinie: mocny glitch, czarny ekran i postac znow na koncu.
// Do tego: migajace REC, migoczaca swietlowka, przewijajacy sie pas
// zaklocen i losowe drobne glitche.

#include "core/face.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

#include "core/pixel.h"
#include "core/push.h"
#include "scene.h"

using namespace assets;
using scene::st;

namespace face {
namespace {

constexpr int MAX_STRIPS = 8;
struct Strip {
  int16_t y, h, dx;
};
Strip strips[MAX_STRIPS];
int stripCount = 0;

char dateShown[48] = "";
int8_t lastMinute = -1;

uint32_t nextFlickerMs = 0, nextBandMs = 0, nextGlitchMs = 0;
uint8_t flickerStep = 0;
constexpr bool FLICKER_SEQ[] = {true, false, true, true, false, true, false, false, true, false};

// Przeskok postaci / skok o pelnej godzinie
uint8_t moveFrames = 0;      // ile klatek glitchu przy przeskoku zostalo
int moveY0 = 0, moveY1 = 0;  // zakres wierszy, w ktorym sypie sie obraz
uint8_t jumpStep = 0;        // 0 = brak; 1..N sekwencja godzinowa
uint8_t pendingFigure = 0;

uint32_t rnd(uint32_t n) { return esp_random() % n; }

void pushScene(int x, int y, int w, int h) { pushComposed(x, y, w, h, scene::compose); }
void pushAll() { pushScene(0, 0, SCREEN_W, SCREEN_H); }

void pushFigureRect(uint8_t f) {
  const FigFrame& fr = FIGURE[f];
  pushScene(fr.x, fr.y, fr.w, fr.h);
}

// Pasek "rozjechanej" tasmy: przesuniecie + odklejony kanal czerwony
void pushStrip(const Strip& s) {
  pushComposed(0, s.y, SCREEN_W, s.h, [&](int x0, int y0, int w, int h, uint16_t* buf) {
    scene::compose(x0, y0, w, h, buf);
    static uint16_t row[SCREEN_W];
    for (int r = 0; r < h; r++) {
      uint16_t* line = buf + r * SCREEN_W;
      memcpy(row, line, sizeof(row));
      for (int x = 0; x < SCREEN_W; x++) {
        const uint16_t base = px::swap(row[(x - s.dx + 2 * SCREEN_W) % SCREEN_W]);
        const uint16_t red = px::swap(row[(x - s.dx - 6 + 2 * SCREEN_W) % SCREEN_W]);
        line[x] = px::swap((base & 0x07FF) | (red & 0xF800));
      }
    }
  });
}

void restoreStrips() {
  for (int i = 0; i < stripCount; i++) pushScene(0, strips[i].y, SCREEN_W, strips[i].h);
  stripCount = 0;
}

void glitchStrips(int count, int yMin, int yMax, int maxShift) {
  restoreStrips();
  stripCount = count > MAX_STRIPS ? MAX_STRIPS : count;
  for (int i = 0; i < stripCount; i++) {
    Strip& s = strips[i];
    s.h = 3 + rnd(12);
    s.y = yMin + rnd(yMax - yMin - s.h > 1 ? yMax - yMin - s.h : 1);
    const int mag = 8 + rnd(maxShift);
    s.dx = (esp_random() & 1) ? mag : -mag;
    pushStrip(s);
  }
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
    int x, y, w, h;
    scene::dateRect(x, y, w, h);
    pushScene(x, y, w, h);
  }

  // postac: klatka = minuta; o pelnej godzinie sekwencja "skoku"
  if (!now || jumpStep || moveFrames) return;
  const int8_t minute = now->tm_min;
  if (minute == lastMinute) return;
  const bool hourTurn = lastMinute > 0 && minute == 0;
  const bool first = lastMinute < 0;
  lastMinute = minute;
  if (first) {  // pierwszy czas po starcie - bez efektow
    st.figure = minute;
    pushAll();
    return;
  }
  if (hourTurn) {
    jumpStep = 1;
    pendingFigure = 0;
    return;
  }
  const FigFrame& a = FIGURE[st.figure];
  const FigFrame& b = FIGURE[minute];
  moveY0 = min(a.y, b.y);
  moveY1 = max(a.y + a.h, b.y + b.h);
  pendingFigure = minute;
  moveFrames = 3;
}

void stepMove() {
  if (!moveFrames) return;
  if (moveFrames == 3) {  // przeskok w trakcie zaklocen
    const uint8_t old = st.figure;
    st.figure = pendingFigure;
    pushFigureRect(old);
    pushFigureRect(st.figure);
  }
  if (--moveFrames) glitchStrips(3 + rnd(3), moveY0, moveY1, 24);
  else restoreStrips();
}

// o pelnej godzinie: 4 klatki mocnego glitchu, ~0,8 s czerni, powrot na koniec korytarza
void stepJump() {
  if (!jumpStep) return;
  if (jumpStep <= 4) {
    st.figure = 59;  // tuz przed kamera
    if (jumpStep == 1) pushFigureRect(59);
    glitchStrips(MAX_STRIPS, 0, SCREEN_H, 60);
  } else if (jumpStep == 5) {
    stripCount = 0;
    st.black = true;
    pushAll();
  } else if (jumpStep == 14) {
    st.black = false;
    st.figure = pendingFigure;
    pushAll();
    jumpStep = 0;
    return;
  }
  jumpStep++;
}

// ---- tlo
void stepRec(uint32_t nowMs) {
  const bool on = (nowMs / 500) % 2 == 0;
  if (on == st.recOn) return;
  st.recOn = on;
  pushScene(REC_DOT.x, REC_DOT.y, REC_DOT.w, REC_DOT.h);
}

void stepFlicker(uint32_t nowMs) {
  if (!flickerStep) {
    if (int32_t(nowMs - nextFlickerMs) < 0) return;
    flickerStep = 1;
  }
  const bool off = FLICKER_SEQ[flickerStep - 1];
  if (off != st.lampOff) {
    st.lampOff = off;
    pushScene(LAMP_X, LAMP_Y, LAMP_W, LAMP_H);
  }
  if (++flickerStep > sizeof(FLICKER_SEQ)) {
    flickerStep = 0;
    nextFlickerMs = nowMs + 6000 + rnd(12000);
    if (st.lampOff) {
      st.lampOff = false;
      pushScene(LAMP_X, LAMP_Y, LAMP_W, LAMP_H);
    }
  }
}

void stepBand(uint32_t nowMs) {
  if (st.bandY <= -scene::BAND_H || st.bandY >= SCREEN_H + 50) {
    if (int32_t(nowMs - nextBandMs) < 0) return;
    nextBandMs = nowMs + 8000 + rnd(9000);
    st.bandY = SCREEN_H;
  }
  const int old = st.bandY;
  st.bandY -= 8;
  const int top = st.bandY;
  pushScene(0, top, SCREEN_W, old + scene::BAND_H - top);
  if (st.bandY <= -scene::BAND_H) st.bandY = -100;
}

void stepRandomGlitch(uint32_t nowMs) {
  if (moveFrames || jumpStep) return;
  if (stripCount) {
    restoreStrips();
    return;
  }
  if (int32_t(nowMs - nextGlitchMs) < 0) return;
  nextGlitchMs = nowMs + 20000 + rnd(15000);
  glitchStrips(2 + rnd(2), 40, SCREEN_H - 40, 16);
}

}  // namespace

void begin() {
  const uint32_t now = millis();
  nextFlickerMs = now + 4000;
  nextBandMs = now + 2000;
  nextGlitchMs = now + 15000;
}

void drawAll() { pushAll(); }

void frame(uint32_t nowMs, const struct tm* now) {
  st.noiseSeed++;
  updateClock(now);
  stepJump();
  stepMove();
  stepRec(nowMs);
  stepFlicker(nowMs);
  stepBand(nowMs);
  stepRandomGlitch(nowMs);
}

}  // namespace face
