#pragma once
// Jedna klatka zegarka: aktualizacja stanu sceny i wysylka tylko
// zmienionych prostokatow (dirty rects) + siatka.

#include <stdint.h>
#include <time.h>

namespace animator {

void begin();

// Pelne przerysowanie ekranu (start)
void drawAll();

// Krok klatki. now = czas lokalny lub nullptr, gdy jeszcze nie zsynchronizowany.
void frame(uint32_t nowMs, const struct tm* now);

}  // namespace animator
