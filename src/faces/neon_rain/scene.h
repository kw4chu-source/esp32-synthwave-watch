#pragma once
// Scena neon_rain skladana z warstw w dowolnym prostokacie:
// tlo (z falujacymi odbiciami na ulicy) -> okna -> hologram -> auto ->
// cyfry-neony -> data -> deszcz -> blysk burzy.
// Pogoda: szyld (ikona + temperatura), deszcz/snieg wg opadu, mgla przy
// ulicy gdy sucho, burza = blyski z piorunem.

#include <stdint.h>
#include "assets_gen.h"
#include "config.h"

namespace scene {

enum class Lit : uint8_t { Off, Dim, On };

struct Slot {
  int8_t digit = -1;  // -1 = pusta (przed synchronizacja czasu)
  Lit lit = Lit::On;
};

struct Drop {
  float x, y;   // gorny koniec smugi
  float vy;
  uint8_t len;
};

constexpr int MAX_DROPS = 120;
constexpr int BOLT_POINTS = 12;
constexpr int MAX_DATE_CHARS = 32;

struct State {
  Slot slots[4];
  bool windowDark[assets::WINDOW_COUNT] = {};
  Drop drops[MAX_DROPS];
  int dropCount = 70;          // aktywne krople/platki
  float slope = 0.22f;         // przesuniecie w lewo na 1 px w dol
  bool snow = false;           // platki zamiast smug
  bool fog = false;            // mgla nad ulica (bez opadu)
  int8_t wxIcon = -1;          // ikona pogody (-1 = brak danych)
  int16_t boltX[BOLT_POINTS];  // piorun (rysowany tylko przy blysku)
  bool bolt = false;
  int carX = -1000;            // lewa krawedz auta; poza ekranem = brak
  int holoBand = -1;           // gorny wiersz jasnego pasa hologramu
  uint8_t ripple = 0;          // faza falowania odbic
  uint8_t flash = 0;           // jasnosc blysku burzy (0 = brak)
};

extern State st;

void setDate(const char* utf8);
void setTemp(const char* utf8);  // np. "12°" (pusty = brak)

// Prostokat obejmujacy smuge (uzywany do dirty rect)
void dropBounds(const Drop& d, int& x0, int& y0, int& x1, int& y1);

// Sklada prostokat do out (w*h pikseli, swap565)
void compose(int x, int y, int w, int h, uint16_t* out);

}  // namespace scene
