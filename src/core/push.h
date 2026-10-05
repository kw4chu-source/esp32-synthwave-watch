#pragma once
// Wysylka dowolnego prostokata sceny przez bufory DMA, kawalkami
// mieszczacymi sie w jednym buforze. compose(x, y, w, h, out) wypelnia
// w*h pikseli w kolejnosci swap565 (px::swap).

#include "config.h"
#include <esp_timer.h>

#include "core/dma_buffers.h"

template <class Compose>
void pushComposed(int x, int y, int w, int h, Compose&& compose) {
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > SCREEN_W) w = SCREEN_W - x;
  if (y + h > SCREEN_H) h = SCREEN_H - y;
  if (w <= 0 || h <= 0) return;
  const int rowsPerChunk = DmaBuffers::CAPACITY_PX / w;
  for (int y0 = y; y0 < y + h; y0 += rowsPerChunk) {
    const int rows = (y0 + rowsPerChunk <= y + h) ? rowsPerChunk : (y + h - y0);
    uint16_t* buf = dmaBuffers.acquire();
    const int64_t t0 = esp_timer_get_time();
    compose(x, y0, w, rows, buf);
    dmaBuffers.stats.composeUs += uint32_t(esp_timer_get_time() - t0);
    dmaBuffers.push(x, y0, w, rows);
  }
}
