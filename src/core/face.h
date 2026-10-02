#pragma once
// Interfejs tarczy zegara. Szkielet (core/) robi reszte: wyswietlacz, bufory
// DMA, tempo klatek, NTP przez wifiFetch(), wroom_link. Kazda tarcza to
// katalog src/faces/<nazwa>/ i osobne srodowisko w platformio.ini.

#include <stdint.h>
#include <time.h>

namespace face {

// Raz w setup(), po inicjalizacji wyswietlacza i buforow DMA
void begin();

// Pelne przerysowanie ekranu (start)
void drawAll();

// Jedna klatka co FRAME_MS. now = czas lokalny albo nullptr przed
// pierwsza synchronizacja NTP. Tarcza wysyla tylko zmienione prostokaty.
void frame(uint32_t nowMs, const struct tm* now);

}  // namespace face
