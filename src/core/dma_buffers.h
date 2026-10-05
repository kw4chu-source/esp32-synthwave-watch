#pragma once
// Para buforow DMA uzywanych na zmiane: w jednym skladamy piksele, drugi
// w tym czasie leci po SPI. Wspolne dla siatki i dirty rectow.

#include <stdint.h>
#include "core/lgfx_config.h"

class DmaBuffers {
public:
  static constexpr int CAPACITY_PX = 480 * 25;  // 24 000 B na bufor

  bool begin(LGFX* lcd);

  // Nastepny wolny bufor (czeka, az DMA skonczy go czytac). Piksele w
  // kolejnosci swap565 - pisac przez px::swap().
  uint16_t* acquire();

  // Wyslij w ostatnio pobranym buforze prostokat w x h (w*h <= CAPACITY_PX)
  void push(int x, int y, int w, int h);

  // Pomiar (mikrosekundy, sumy od ostatniego resetStats):
  //  compose = skladanie pikseli, wait = czekanie na koniec poprzedniego DMA,
  //  push = wywolanie pushImageDMA (konwersja 565 -> 666 robi CPU)
  struct Stats {
    uint32_t composeUs, waitUs, pushUs, pixels;
  };
  Stats stats = {};
  void resetStats() { stats = {}; }

private:
  LGFX* _lcd = nullptr;
  uint16_t* _buf[2] = {nullptr, nullptr};
  int _cur = 0;
};

extern DmaBuffers dmaBuffers;
