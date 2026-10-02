#pragma once
// Siatka perspektywiczna pod horyzontem (y 220-320), rysowana co klatke
// pasami po 25 wierszy prosto do buforow DMA.

#include <stdint.h>

namespace grid {

void begin();

// scroll: pozycja w jednostkach glebokosci (rosnie z czasem)
void render(float scroll);

}  // namespace grid
