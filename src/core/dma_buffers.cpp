#include "dma_buffers.h"

#include <esp_heap_caps.h>
#include <esp_timer.h>

DmaBuffers dmaBuffers;

bool DmaBuffers::begin(LGFX* lcd) {
  _lcd = lcd;
  for (auto& b : _buf) {
    b = static_cast<uint16_t*>(heap_caps_malloc(CAPACITY_PX * 2, MALLOC_CAP_DMA | MALLOC_CAP_8BIT));
    if (!b) return false;
  }
  return true;
}

uint16_t* DmaBuffers::acquire() {
  // Ten bufor byl wysylany dwa push() temu, a push() czeka na koniec
  // poprzedniego transferu - wiec jest juz wolny. Skladanie w nim idzie
  // rownolegle z transferem drugiego bufora.
  _cur ^= 1;
  return _buf[_cur];
}

void DmaBuffers::push(int x, int y, int w, int h) {
  const int64_t t0 = esp_timer_get_time();
  _lcd->waitDMA();
  const int64_t t1 = esp_timer_get_time();
  _lcd->pushImageDMA(x, y, w, h, reinterpret_cast<const lgfx::swap565_t*>(_buf[_cur]));
  stats.waitUs += uint32_t(t1 - t0);
  stats.pushUs += uint32_t(esp_timer_get_time() - t1);
  stats.pixels += uint32_t(w * h);
}
