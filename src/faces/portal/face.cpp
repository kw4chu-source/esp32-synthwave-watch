// Tarcza "portal" (klimat Rick i Morty): zielony portal z wirujacymi
// ramionami spirali i falujaca krawedzia, wylatuja z niego kostki i galki
// oczne, dookola krazy spodek. Cyfry "glutowe" z obrysem.
// Zmiana minuty = seria rzeczy z portalu, pelna godzina = wielki wyrzut
// i szybszy wir.
//
// Piksele portalu z mapy biegunowej (r8, kat) z flasha; co klatke wysylany
// prostokat portalu + prostokaty latajacych obiektow (hd::Tiles).

#include "core/face.h"

#include <esp_random.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "assets_gen.h"
#include "faces/common_hd/hd.h"

using namespace assets;

namespace face {
namespace {

hd::Tiles tiles;
hd::TextLine dateLine;
char dateShown[48] = "";
int8_t digits[4] = {-1, -1, -1, -1};
int lastMinute = -1, lastHour = -1;

const hd::Style CLOCK_STYLE = {hd::Ink::Outline, nullptr, px::rgb565(190, 255, 60), px::rgb565(10, 40, 0)};
const hd::Style DATE_STYLE = {hd::Ink::Outline, nullptr, px::rgb565(190, 255, 60), px::rgb565(0, 0, 0)};
const uint16_t BG = px::rgb565(4, 6, 12);
const uint16_t ARM = px::rgb565(225, 255, 150);
const uint16_t EDGE = px::rgb565(235, 255, 175);

uint16_t grad[256];         // wnetrze portalu wg promienia
uint16_t armWidth[256];     // polowa szerokosci ramienia (kat x5)
uint8_t glowLut[64][3];     // poswiata na zewnatrz krawedzi
uint8_t edge8[256];         // promien krawedzi wg kata (faluje)
uint16_t rot = 0;           // obrot spirali
uint32_t frameNo = 0;
int spinBoost = 0;

// ---------------------------------------------------------------- obiekty
enum Kind : uint8_t { CUBE, EYE };
struct Obj {
  bool on;
  Kind kind;
  float u, ang, speed, spin;
  int16_t x, y, s;  // biezaca pozycja i polowa rozmiaru
};
constexpr int MAX_OBJ = 16;
Obj objs[MAX_OBJ];
int queued = 0;  // obiekty czekajace na wyrzut (seria)
uint32_t nextSpawn = 10;

struct Saucer {
  float a;
  int16_t x, y;
} saucer = {0, 0, 0};

uint32_t rnd(uint32_t n) { return esp_random() % n; }
float frnd() { return (esp_random() & 0xFFFF) / 65536.0f; }

void markObj(const Obj& o) { tiles.mark(o.x - o.s - 3, o.y - o.s - 3, 2 * o.s + 7, 2 * o.s + 7); }
void markSaucer() { tiles.mark(saucer.x - 28, saucer.y - 17, 57, 27); }

void spawn() {
  for (auto& o : objs)
    if (!o.on) {
      o = {true, rnd(3) ? CUBE : EYE, 0, frnd() * 2 * float(M_PI), 0.009f + frnd() * 0.008f, (frnd() - 0.5f) * 0.25f,
           CX, CY, 4};
      return;
    }
}

void placeObj(Obj& o) {
  const float r = 20 + o.u * 260;
  o.x = int16_t(CX + cosf(o.ang) * r);
  o.y = int16_t(CY + sinf(o.ang) * r * 0.75f);
  o.s = int16_t(4 + o.u * 22);
}

// ---------------------------------------------------------------- rysowanie
void drawObj(const Obj& o, int x0, int y0, int w, int h, uint16_t* out) {
  const int bx0 = o.x - o.s - 2, by0 = o.y - o.s - 2, bx1 = o.x + o.s + 2, by1 = o.y + o.s + 2;
  if (bx1 < x0 || bx0 >= x0 + w || by1 < y0 || by0 >= y0 + h) return;
  const float ca = cosf(o.u * 6 * (1 + o.spin * 8)), sa = sinf(o.u * 6 * (1 + o.spin * 8));
  const int s = o.s, s2 = s * s;
  for (int y = (by0 > y0 ? by0 : y0); y <= by1 && y < y0 + h; y++)
    for (int x = (bx0 > x0 ? bx0 : x0); x <= bx1 && x < x0 + w; x++) {
      const int dx = x - o.x, dy = y - o.y;
      uint16_t& p = out[(y - y0) * w + x - x0];
      if (o.kind == CUBE) {
        const float lx = fabsf(dx * ca + dy * sa), ly = fabsf(-dx * sa + dy * ca);
        const float m = lx > ly ? lx : ly;
        if (m < s - 2) p = px::rgb565(255, 120, 200);
        else if (m < s) p = 0xFFFF;
      } else {
        const int d2 = dx * dx + dy * dy;
        if (d2 >= s2) continue;
        if (d2 * 100 < s2 * 4) p = 0;                                  // zrenica
        else if (d2 * 100 < s2 * 20) p = px::rgb565(40, 140, 255);     // teczowka
        else if (d2 * 100 < s2 * 72) p = px::rgb565(250, 250, 240);    // bialko
        else p = px::rgb565(40, 40, 40);                               // obwodka
      }
    }
}

void drawSaucer(int x0, int y0, int w, int h, uint16_t* out) {
  const int sx = saucer.x, sy = saucer.y;
  if (sx + 28 < x0 || sx - 28 >= x0 + w || sy + 10 < y0 || sy - 17 >= y0 + h) return;
  for (int y = (sy - 17 > y0 ? sy - 17 : y0); y < sy + 10 && y < y0 + h; y++)
    for (int x = (sx - 28 > x0 ? sx - 28 : x0); x < sx + 28 && x < x0 + w; x++) {
      const float bx = (x - sx) / 26.0f, by = (y - sy - 1) / 7.5f;
      const float dx_ = (x - sx) / 11.5f, dy_ = (y - sy + 6) / 9.0f;
      const float body = bx * bx + by * by, dome = dx_ * dx_ + dy_ * dy_;
      uint16_t& p = out[(y - y0) * w + x - x0];
      if (dome < 1 && y < sy - 1) p = dome > 0.7f ? px::rgb565(40, 40, 50) : px::rgb565(120, 230, 255);
      else if (body < 1) {
        p = body > 0.72f ? px::rgb565(40, 40, 50) : px::rgb565(180, 190, 200);
        // swiatelka
        for (int q = 0; q < 3; q++) {
          const int lx = sx - 15 + q * 15, ly = sy + 3;
          if ((x - lx) * (x - lx) + (y - ly) * (y - ly) <= 6)
            p = ((frameNo / 3 + q) % 3 == 0) ? px::rgb565(255, 240, 60) : px::rgb565(90, 90, 60);
        }
      }
    }
}

inline uint16_t portalPixel(int x, int y) {
  int r8, e8;
  uint16_t ang = 0;
  const bool inMap = x >= MAP_X && x < MAP_X + MAP_W && y >= MAP_Y && y < MAP_Y + MAP_H;
  if (inMap) {
    const uint16_t* m = PORTAL_MAP[(y - MAP_Y) * MAP_W + (x - MAP_X)];
    r8 = m[0];
    ang = m[1];
    e8 = edge8[ang >> 8];
  } else {
    const float dx = (x + 0.5f - CX) / assets::RX, dy = (y + 0.5f - CY) / assets::RY;
    const float rn = sqrtf(dx * dx + dy * dy) * R_ONE;
    r8 = rn > 255 ? 255 : int(rn);
    e8 = R_ONE;
  }
  const int diff = r8 - e8;
  if (diff < -5) {
    uint16_t p = grad[r8];
    const uint16_t phi = uint16_t((ang - uint16_t(r8 * 261) - rot) * 5);
    const uint16_t d = phi < 32768 ? phi : uint16_t(65536 - phi);
    const uint16_t aw = armWidth[r8];
    if (d < aw) p = px::blend32(p, ARM, 32 - d * 32 / aw);
    return p;
  }
  if (diff <= 2) return (diff == -5 || diff == 2) ? px::blend32(grad[r8 < 255 ? r8 : 255], EDGE, 18) : EDGE;
  const int g = r8 - R_ONE;
  return g < 64 ? hd::addLight(BG, glowLut[g < 0 ? 0 : g], x, y) : BG;
}

void compose(int x0, int y0, int w, int h, uint16_t* out) {
  for (int r = 0; r < h; r++)
    for (int i = 0; i < w; i++) out[r * w + i] = portalPixel(x0 + i, y0 + r);
  for (const auto& s : STARS)
    if (s.x >= x0 && s.x < x0 + w && s.y >= y0 && s.y < y0 + h) out[(s.y - y0) * w + s.x - x0] = px::rgb565(s.b, s.b, s.b);
  for (const auto& o : objs)
    if (o.on) drawObj(o, x0, y0, w, h, out);
  drawSaucer(x0, y0, w, h, out);
  if (y0 < DIGIT_TOP + DIGIT_CELL_H && y0 + h > DIGIT_TOP) {
    for (int i = 0; i < 4; i++)
      if (digits[i] >= 0)
        hd::drawGlyph(DIGIT_DATA[digits[i]], DIGIT_CELL_W, DIGIT_CELL_H, DIGIT_X[i], DIGIT_TOP, CLOCK_STYLE, x0, y0, w,
                      h, out);
    if (digits[0] >= 0)
      hd::drawGlyph(COLON_DATA, COLON_W, DIGIT_CELL_H, COLON_X, DIGIT_TOP, CLOCK_STYLE, x0, y0, w, h, out);
  }
  if (y0 < DATE_BOTTOM && y0 + h > DATE_TOP) dateLine.draw(DATE_DATA, DATE_BASELINE, DATE_STYLE, x0, y0, w, h, out);
  for (int i = 0; i < w * h; i++) out[i] = px::swap(out[i]);
}

// ---------------------------------------------------------------- logika
void updateEdge() {
  const float t = frameNo * (2 * float(M_PI) / 45);
  for (int a = 0; a < 256; a++) {
    const float ang = a * (2 * float(M_PI) / 256);
    edge8[a] = uint8_t(R_ONE * (1 + 0.05f * sinf(ang * 9 + t * 2) + 0.03f * sinf(ang * 23 - t * 3)));
  }
}

void updateClock(const struct tm* now) {
  int8_t nd[4] = {-1, -1, -1, -1};
  char date[48] = "";
  if (now) {
    nd[0] = now->tm_hour / 10;
    nd[1] = now->tm_hour % 10;
    nd[2] = now->tm_min / 10;
    nd[3] = now->tm_min % 10;
    snprintf(date, sizeof(date), "%s  %02d.%02d.%04d", WEEKDAYS[now->tm_wday], now->tm_mday, now->tm_mon + 1,
             now->tm_year + 1900);
    if (lastMinute >= 0 && now->tm_min != lastMinute) queued += 6;
    if (lastHour >= 0 && now->tm_hour != lastHour) {
      queued += 14;
      spinBoost = 90;
    }
    lastMinute = now->tm_min;
    lastHour = now->tm_hour;
  }
  for (int i = 0; i < 4; i++)
    if (nd[i] != digits[i]) {
      if (digits[i] < 0 || nd[i] < 0) tiles.mark(COLON_X, DIGIT_TOP, COLON_W, DIGIT_CELL_H);
      digits[i] = nd[i];
      tiles.mark(DIGIT_X[i], DIGIT_TOP, DIGIT_CELL_W, DIGIT_CELL_H);
    }
  if (strcmp(date, dateShown) != 0) {
    strcpy(dateShown, date);
    dateLine.set(date, DATE_GLYPHS, DATE_GLYPH_COUNT, SCREEN_W / 2);
    tiles.mark(0, DATE_TOP, SCREEN_W, DATE_BOTTOM - DATE_TOP);
  }
}

}  // namespace

void begin() {
  for (int r = 0; r < 256; r++) {
    float f = r / float(R_ONE);
    if (f > 1) f = 1;
    const float k = 1.45f;
    auto c = [&](float v) { return uint8_t(v * k > 255 ? 255 : v * k); };
    grad[r] = px::rgb565(c(30 + 150 * (1 - f)), c(140 + 115 * sqrtf(1 - f)), c(20 + 40 * (1 - f)));
    const uint32_t aw = r ? 177000u / r : 16000u;
    armWidth[r] = uint16_t(aw > 16000 ? 16000 : aw);
  }
  for (int d = 0; d < 64; d++) {
    const float g = 0.75f * expf(-d / 13.0f);
    glowLut[d][0] = uint8_t(60 * g);
    glowLut[d][1] = uint8_t(255 * g);
    glowLut[d][2] = uint8_t(80 * g);
  }
  updateEdge();
  saucer.x = CX + 205;
  saucer.y = CY + 20;
}

void drawAll() {
  tiles.markAll();
  tiles.flush(compose);
}

void frame(uint32_t nowMs, const struct tm* now) {
  frameNo++;
  updateClock(now);

  // stare pozycje
  tiles.mark(MAP_X, MAP_Y, MAP_W, MAP_H);
  for (const auto& o : objs)
    if (o.on) markObj(o);
  markSaucer();

  // wir i krawedz
  rot += spinBoost ? 2600 : 1000;
  if (spinBoost) spinBoost--;
  updateEdge();

  // obiekty
  if (queued > 0 && frameNo % 3 == 0) {
    spawn();
    queued--;
  } else if (frameNo >= nextSpawn) {
    spawn();
    nextSpawn = frameNo + 25 + rnd(40);
  }
  for (auto& o : objs) {
    if (!o.on) continue;
    o.u += o.speed;
    if (o.u > 1.05f) {
      o.on = false;
      continue;
    }
    placeObj(o);
    markObj(o);
  }

  // spodek po elipsie (okrazenie ~12 s)
  saucer.a += 2 * float(M_PI) / 130;
  saucer.x = int16_t(CX + cosf(saucer.a) * 205);
  saucer.y = int16_t(CY + 20 + sinf(saucer.a) * 110);
  markSaucer();

  tiles.flush(compose);
}

}  // namespace face
