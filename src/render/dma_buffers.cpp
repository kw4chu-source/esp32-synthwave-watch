#include "dma_buffers.h"

#include <esp_heap_caps.h>

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
  _lcd->waitDMA();
  _lcd->pushImageDMA(x, y, w, h, reinterpret_cast<const lgfx::swap565_t*>(_buf[_cur]));
}
