// Tarcza "fireworks" (8-bit): nocne miasto, duze biale cyfry z fioletowym
// konturem, rakiety wybuchaja kolorowymi kulami iskier, w budynkach zapalaja
// sie i gasna okna. O pelnej godzinie - wielki pokaz.
// Tlo (gwiazdy, miasto, cyfry, data) w osobnym buforze; iskry rysowane tylko
// na niebie i zmazywane przez przywrocenie piksela tla.

#include "core/face.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "faces/common_8bit/pix8.h"

using namespace pix8;

namespace face {
namespace {

constexpr int DIGIT_Y = 34, DIGIT_SCALE = 4;
constexpr int DATE_Y = LH - 9;

uint8_t bg[LH][LW];       // tlo bez iskier
bool sky[LH][LW];         // gdzie wolno rysowac iskry (nie miasto, cyfry, data)

struct Window {
  uint8_t x, y;
  bool lit;
};
Window windows[160];
int windowCount = 0;

struct Spark {
  float x, y, vx, vy;
  uint8_t life, color;
  int16_t px, py;  // gdzie narysowano w poprzedniej klatce
};
constexpr int MAX_SPARKS = 220;
Spark sparks[MAX_SPARKS];

struct Rocket {
  bool on;
  float x, y, vy;
  uint8_t color;
  int16_t px, py;
};
Rocket rockets[4];

uint32_t frameNo = 0, nextRocket = 20;
int showLeft = 0;  // rakiety pokazu godzinowego
char shownTime[6] = "", shownDate[32] = "";
int lastHour = -1;

const uint8_t BURST_COLORS[] = {M, C, Y, G, R, P, O, L};

// ---------------------------------------------------------------- tlo
void bgSet(int x, int y, uint8_t c) {
  if (x < 0 || x >= LW || y < 0 || y >= LH) return;
  bg[y][x] = c;
  set(x, y, c);
}

void bgFill(int x, int y, int w, int h, uint8_t c, bool isSky) {
  for (int yy = y; yy < y + h; yy++)
    for (int xx = x; xx < x + w; xx++)
      if (xx >= 0 && xx < LW && yy >= 0 && yy < LH) {
        bg[yy][xx] = c;
        sky[yy][xx] = isSky;
        fb[yy][xx] = c;
      }
  markDirty(x, y, w, h);
}

void restore(int x, int y) {
  if (x >= 0 && x < LW && y >= 0 && y < LH) set(x, y, bg[y][x]);
}

void drawDigits(const char* hhmm) {
  const int w = bigTimeWidth(DIGIT_SCALE) + 4;
  const int x0 = (LW - w) / 2;
  // obszar cyfr wraca do nieba (gwiazdy pozostaja tylko poza nim)
  bgFill(x0, DIGIT_Y - 2, w, 7 * DIGIT_SCALE + 4, K, true);
  if (!hhmm[0]) return;
  bigTime(hhmm, DIGIT_Y, DIGIT_SCALE, [](int x, int y, int) { bgFill(x - 1, y - 1, DIGIT_SCALE + 2, DIGIT_SCALE + 2, P, false); });
  bigTime(hhmm, DIGIT_Y, DIGIT_SCALE, [](int x, int y, int) { bgFill(x, y, DIGIT_SCALE, DIGIT_SCALE, W, false); });
}

void drawDate(const char* date) {
  bgFill(0, DATE_Y - 1, LW, 10, K, false);
  if (!date[0]) return;
  const int x = (LW - textWidth(date)) / 2;
  text(x, DATE_Y, date, W);
  for (int y = DATE_Y - 1; y < LH; y++)
    for (int xx = 0; xx < LW; xx++) bg[y][xx] = fb[y][xx];
}

void buildCity() {
  int x = 0;
  while (x < LW) {
    const int w = 10 + rnd(13), h = 14 + rnd(19);
    bgFill(x, DATE_Y - 1 - h, w, h, D, false);
    for (int wy = DATE_Y + 1 - h; wy < DATE_Y - 3; wy += 4)
      for (int wx = x + 2; wx < x + w - 2 && wx < LW; wx += 3)
        if (windowCount < 160 && rnd(100) < 60) {
          const bool lit = rnd(100) < 40;
          windows[windowCount++] = {uint8_t(wx), uint8_t(wy), lit};
          bgSet(wx, wy, lit ? (rnd(3) ? Y : O) : N);
        }
    if (x + w < LW) bgFill(x + w, DATE_Y - 1 - h, 1, h, K, false);  // szczelina miedzy blokami
    x += w + 1;
  }
}

// ---------------------------------------------------------------- iskry i rakiety
void launch() {
  for (auto& r : rockets)
    if (!r.on) {
      r = {true, float(15 + rnd(LW - 30)), float(DATE_Y - 20), -(1.3f + rnd(60) / 100.0f),
           BURST_COLORS[rnd(sizeof(BURST_COLORS))], -1, -1};
      return;
    }
}

void burst(float x, float y, uint8_t color) {
  const int n = 20 + rnd(10);
  const float speed = 0.7f + rnd(40) / 100.0f;
  int made = 0;
  for (auto& s : sparks) {
    if (s.life) continue;
    const float a = made * 2 * M_PI / n;
    s = {x, y, cosf(a) * speed, sinf(a) * speed * 0.9f, uint8_t(32 + rnd(10)), color, -1, -1};
    if (++made >= n) break;
  }
}

void eraseAt(int16_t& px, int16_t& py) {
  if (px >= 0) restore(px, py);
  px = -1;
}

void stepRockets() {
  for (auto& r : rockets) {
    if (!r.on) continue;
    eraseAt(r.px, r.py);
    r.y += r.vy;
    r.vy += 0.02f;  // grawitacja hamuje wznoszenie
    if (r.vy >= -0.25f || r.y < 8) {
      burst(r.x, r.y, r.color);
      r.on = false;
      continue;
    }
    const int x = int(r.x), y = int(r.y);
    if (x >= 0 && x < LW && y >= 0 && y < LH && sky[y][x]) {
      set(x, y, W);
      r.px = x;
      r.py = y;
    }
  }
}

void stepSparks() {
  for (auto& s : sparks) {
    if (!s.life) continue;
    eraseAt(s.px, s.py);
    s.x += s.vx;
    s.y += s.vy;
    s.vy += 0.03f;
    s.vx *= 0.985f;
    if (--s.life == 0) continue;
    const int x = int(s.x), y = int(s.y);
    if (x < 0 || x >= LW || y < 0 || y >= LH || !sky[y][x]) continue;
    // kolor: najpierw blysk, potem barwa wybuchu, na koncu przygasanie
    const uint8_t c = s.life > 28 ? W : (s.life > 8 ? s.color : (s.life > 4 ? S : D));
    set(x, y, c);
    s.px = x;
    s.py = y;
  }
}

void stepWindows() {
  if (!windowCount || rnd(100) >= 25) return;
  Window& w = windows[rnd(windowCount)];
  w.lit = !w.lit;
  bgSet(w.x, w.y, w.lit ? (rnd(3) ? Y : O) : N);
}

void updateClock(const struct tm* now) {
  char hhmm[6] = "";
  if (now) snprintf(hhmm, sizeof(hhmm), "%02d:%02d", now->tm_hour, now->tm_min);
  if (strcmp(hhmm, shownTime) != 0) {
    strcpy(shownTime, hhmm);
    drawDigits(hhmm);
    if (now) showLeft += 2;  // zmiana minuty: dwie rakiety na powitanie
  }
  char date[32] = "";
  if (now) formatDate(now, date, sizeof(date));
  if (strcmp(date, shownDate) != 0) {
    strcpy(shownDate, date);
    drawDate(date);
  }
  if (now) {
    if (lastHour >= 0 && now->tm_hour != lastHour) showLeft = 14;  // pokaz o pelnej godzinie
    lastHour = now->tm_hour;
  }
}

}  // namespace

void begin() {
  pix8::begin(K);
  for (int y = 0; y < LH; y++)
    for (int x = 0; x < LW; x++) {
      bg[y][x] = K;
      sky[y][x] = true;
    }
  for (int i = 0; i < 45; i++) {
    const int x = rnd(LW), y = rnd(DATE_Y - 34);
    bgSet(x, y, (i % 4 == 0) ? W : (i % 2 ? S : D));
  }
  buildCity();
  drawDigits("");
  drawDate("");
}

void drawAll() {
  markAll();
  flush();
}

void frame(uint32_t nowMs, const struct tm* now) {
  frameNo++;
  updateClock(now);
  if (frameNo >= nextRocket) {
    launch();
    if (showLeft > 0) {
      showLeft--;
      nextRocket = frameNo + 4 + rnd(6);
    } else {
      nextRocket = frameNo + 10 + rnd(25);
    }
  }
  stepRockets();
  stepSparks();
  stepWindows();
  flush();
}

}  // namespace face
