// Tarcza "invaders" (8-bit): godzina to zielone oslony, nad nimi maszeruja
// kosmici, statek na dole strzela. Bomby kosmitow wybijaja dziury w cyfrach;
// przy zmianie minuty oslony odbudowuja sie jako nowa godzina. Gora czasem
// przelatuje UFO, o pelnej godzinie wybucha cala flota.
// Wspolrzedne w pikselach 8-bit (160x107), ekran = x3.

#include "core/face.h"

#include <stdio.h>
#include <string.h>

#include "faces/common_8bit/pix8.h"

using namespace pix8;

namespace face {
namespace {

// ---- sprite'y (wlasne projekty)
const char* const ALIEN_A[2][7] = {
    {"..X...X..", "...XXX...", "..XXXXX..", ".XX.X.XX.", "XXXXXXXXX", "X.X...X.X", "..X...X.."},
    {"..X...X..", "...XXX...", "..XXXXX..", ".XX.X.XX.", "XXXXXXXXX", ".XX...XX.", "X.......X"}};
const char* const ALIEN_B[2][7] = {
    {"...XX....", "..XXXX...", ".XXXXXX..", "XX.XX.XX.", "XXXXXXXX.", ".X.XX.X..", "X......X."},
    {"...XX....", "..XXXX...", ".XXXXXX..", "XX.XX.XX.", "XXXXXXXX.", "..X..X...", ".X....X.."}};
const char* const BOOM[7] = {"X...X...X", ".X..X..X.", "..X...X..", "XX.....XX", "..X...X..", ".X..X..X.", "X...X...X"};
const char* const SHIP[5] = {"....XXX....", "...XXXXX...", ".XXXXXXXXX.", "XXXXXXXXXXX", "XXX.XXX.XXX"};
const char* const UFO[5] = {"....XXXXX....", "..XXXXXXXXX..", ".XX.XX.XX.XX.", "XXXXXXXXXXXXX", "..XXX...XXX.."};

constexpr int COLS = 8, ROWS = 3;
constexpr int ROW_Y[ROWS] = {16, 25, 34};
constexpr uint8_t ROW_COLOR[ROWS] = {M, C, G};
constexpr int BAND_X = 8, BAND_Y = 15, BAND_W = 146, BAND_H = 27;
constexpr int DIGIT_Y = 45, DIGIT_SCALE = 4;
constexpr int SHIP_Y = 88, GROUND_Y = 97, DATE_Y = 99;

struct Alien {
  uint8_t state;     // 0 zywy, 1..6 wybuch, 7 martwy
  uint16_t respawn;  // klatki do odrodzenia
};
Alien aliens[ROWS][COLS];
int marchOff = 0, marchDir = 1, animFrame = 0;
uint32_t frameNo = 0;
bool bandDirty = true;

int shipX = 74;
struct Shot {
  bool on;
  int x, y;
  uint8_t drawn;  // ktore z 3 pikseli narysowalismy (zeby nie zmazac gwiazd)
};
Shot bullet = {};
Shot bombs[3] = {};
int fireCooldown = 0, bombCooldown = 60;
int ufoX = -100;
uint32_t nextUfo = 400;

char shownTime[6] = "";
char shownDate[32] = "";
int lastHour = -1;

int alienX(int col) { return 22 + col * 15 + marchOff; }

// ---------------------------------------------------------------- rysowanie
void drawBand() {
  fill(BAND_X, BAND_Y, BAND_W, BAND_H, K);
  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++) {
      const Alien& a = aliens[r][c];
      if (a.state == 0)
        sprite(alienX(c), ROW_Y[r], r == 1 ? ALIEN_B[animFrame] : ALIEN_A[animFrame], 7, ROW_COLOR[r]);
      else if (a.state <= 6)
        sprite(alienX(c), ROW_Y[r], BOOM, 7, (a.state & 1) ? Y : O);
    }
  bandDirty = false;
}

void drawDigits(const char* hhmm) {
  fill(26, DIGIT_Y - 1, 108, 7 * DIGIT_SCALE + 2, K);
  if (hhmm[0]) bigTime(hhmm, DIGIT_Y, DIGIT_SCALE, [](int x, int y, int r) { fill(x, y, DIGIT_SCALE, DIGIT_SCALE, r < 5 ? G : T); });
}

void drawHud(const char* hhmm) {
  fill(0, 0, 96, 11, K);
  char s[16];
  snprintf(s, sizeof(s), "SCORE %c%c%c%c", hhmm[0] ? hhmm[0] : '-', hhmm[0] ? hhmm[1] : '-',
           hhmm[0] ? hhmm[3] : '-', hhmm[0] ? hhmm[4] : '-');
  text(4, 2, s, W);
}

// pociski: 1x3, rysowane tylko na czarnym tle i zmazywane tylko tam, gdzie je narysowano
void shotErase(Shot& s) {
  for (int i = 0; i < 3; i++)
    if (s.drawn & (1 << i)) set(s.x, s.y + i, K);
  s.drawn = 0;
}
void shotDraw(Shot& s, uint8_t color) {
  for (int i = 0; i < 3; i++)
    if (get(s.x, s.y + i) == K) {
      set(s.x, s.y + i, color);
      s.drawn |= 1 << i;
    }
}

// ---------------------------------------------------------------- logika
void stepAliens() {
  if (frameNo % 6 == 0) {  // marsz
    marchOff += marchDir;
    if (marchOff >= 6 || marchOff <= -6) marchDir = -marchDir;
    animFrame ^= 1;
    bandDirty = true;
  }
  if (frameNo % 2 == 0)
    for (auto& row : aliens)
      for (auto& a : row) {
        if (a.state >= 1 && a.state <= 6) {
          a.state++;
          bandDirty = true;
          if (a.state == 7) a.respawn = 150 + rnd(250);
        } else if (a.state == 7 && a.respawn && --a.respawn == 0) {
          a.state = 0;
          bandDirty = true;
        }
      }
}

int targetColumn() {
  int best = -1, bestDist = 999;
  for (int c = 0; c < COLS; c++)
    for (int r = 0; r < ROWS; r++)
      if (aliens[r][c].state == 0) {
        const int d = abs(alienX(c) + 4 - (shipX + 5));
        if (d < bestDist) bestDist = d, best = c;
        break;
      }
  return best;
}

void stepShip() {
  const int col = targetColumn();
  if (col < 0) return;
  const int tx = alienX(col) + 4 - 5;
  if (tx != shipX) {
    fill(shipX, SHIP_Y, 11, 5, K);
    shipX += tx > shipX ? 1 : -1;
    sprite(shipX, SHIP_Y, SHIP, 5, C);
  }
  if (fireCooldown) fireCooldown--;
  if (!bullet.on && !fireCooldown && abs(tx - shipX) <= 1) {
    bullet = {true, shipX + 5, SHIP_Y - 4, 0};
    fireCooldown = 20 + rnd(25);
  }
}

void stepBullet() {
  if (!bullet.on) return;
  shotErase(bullet);
  bullet.y -= 3;
  // trafienie kosmity
  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++) {
      Alien& a = aliens[r][c];
      const int ax = alienX(c);
      if (a.state == 0 && bullet.x >= ax && bullet.x < ax + 9 && bullet.y <= ROW_Y[r] + 6 && bullet.y + 2 >= ROW_Y[r]) {
        a.state = 1;
        bullet.on = false;
        bandDirty = true;
        return;
      }
    }
  if (ufoX > -13 && bullet.x >= ufoX && bullet.x < ufoX + 13 && bullet.y <= 14) {
    fill(ufoX, 10, 13, 5, K);
    ufoX = -100;
    bullet.on = false;
    return;
  }
  if (bullet.y < 12) {
    bullet.on = false;
    return;
  }
  shotDraw(bullet, W);
}

void stepBombs() {
  if (bombCooldown) bombCooldown--;
  for (auto& b : bombs) {
    if (!b.on) {
      if (bombCooldown) continue;
      const int c = rnd(COLS);
      for (int r = ROWS - 1; r >= 0; r--)
        if (aliens[r][c].state == 0) {
          b = {true, alienX(c) + 4, ROW_Y[r] + 8, 0};
          bombCooldown = 40 + rnd(60);
          break;
        }
      continue;
    }
    shotErase(b);
    b.y += 1;
    const uint8_t below = get(b.x, b.y + 2);
    if (below == G || below == T) {  // trafienie w oslone-cyfre: wybita dziura
      fill(b.x - 1, b.y + 1, 3, 3, K);
      b.on = false;
      continue;
    }
    if (b.y + 2 >= GROUND_Y) {
      b.on = false;
      continue;
    }
    shotDraw(b, M);
  }
}

void stepUfo(uint32_t now) {
  if (ufoX <= -13) {
    if (frameNo < nextUfo) return;
    ufoX = LW;
    nextUfo = frameNo + 450 + rnd(300);
  }
  fill(ufoX, 10, 13, 5, K);
  ufoX--;
  if (ufoX > -13) sprite(ufoX, 10, UFO, 5, R);
}

void updateClock(const struct tm* now) {
  char hhmm[6] = "";
  if (now) snprintf(hhmm, sizeof(hhmm), "%02d:%02d", now->tm_hour, now->tm_min);
  if (strcmp(hhmm, shownTime) != 0) {
    strcpy(shownTime, hhmm);
    drawDigits(hhmm);  // oslony odbudowane jako nowa godzina
    drawHud(hhmm);
  }
  char date[32] = "";
  if (now) formatDate(now, date, sizeof(date));
  if (strcmp(date, shownDate) != 0) {
    strcpy(shownDate, date);
    fill(0, DATE_Y - 1, LW, 9, K);
    textCentered(DATE_Y, date, W);
  }
  if (now) {  // pelna godzina: wybucha cala flota
    if (lastHour >= 0 && now->tm_hour != lastHour)
      for (auto& row : aliens)
        for (auto& a : row)
          if (a.state == 0) a.state = 1;
    lastHour = now->tm_hour;
  }
}

}  // namespace

void begin() {
  pix8::begin(K);
  // gwiazdy tylko tam, gdzie nic sie nie rusza
  for (int i = 0; i < 40; i++) {
    const int x = rnd(LW), y = rnd(LH);
    const bool busy = (y >= BAND_Y - 6 && y < BAND_Y + BAND_H) || (y >= DIGIT_Y - 2 && y < DIGIT_Y + 30) ||
                      (y >= SHIP_Y - 2 && y < LH) || y < 12;
    if (!busy) fb[y][x] = (i % 5 == 0) ? W : (i % 2 ? S : D);
  }
  text(100, 2, "HI 9999", Y);
  fill(0, GROUND_Y, LW, 1, G);
  drawHud("");
  sprite(shipX, SHIP_Y, SHIP, 5, C);
  drawBand();
}

void drawAll() {
  markAll();
  flush();
}

void frame(uint32_t nowMs, const struct tm* now) {
  frameNo++;
  updateClock(now);
  stepAliens();
  stepShip();
  stepBullet();
  stepBombs();
  stepUfo(nowMs);
  if (bandDirty) drawBand();
  flush();
}

}  // namespace face
