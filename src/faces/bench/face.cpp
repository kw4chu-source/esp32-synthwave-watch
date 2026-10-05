// "Tarcza" testowa: benchmark wyswietlacza i ESP32 dla planu przyspieszenia.
//
// Przy starcie (ok. 30 s, ekran miga wzorami):
//  1. SPI 26.7 / 40 / 80 MHz: pelny ekran przez nasza sciezke (565 -> konwersja
//     666 na CPU -> DMA), rozbicie czasu: skladanie / czekanie SPI / konwersja.
//     Poprawnosc: odczyt pikseli z ekranu (MISO, 16 MHz) i porownanie ze wzorem.
//  2. Sciezka "surowa": skladanie od razu w 3 bajtach RGB (bez konwersji).
//  3. Przeplot: tylko parzyste wiersze.
//  4. Flash (mapy z flasha) i RAM: MB/s.
//  5. CPU: koszt typowego piksela (jak zorza), jeden vs dwa rdzenie;
//     sinf / sqrtf / dzielenie.
// Potem: tabela wynikow na gorze ekranu, na dole ciagly test obciazeniowy
// (plazma) na najszybszym poprawnym zegarze + licznik bledow odczytu.
// Wszystko leci tez na Serial ([BENCH] ...).

#include "core/face.h"

#include <Arduino.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <math.h>
#include <string.h>

#include "core/dma_buffers.h"
#include "core/lgfx_config.h"
#include "core/pixel.h"
#include "core/push.h"

extern LGFX lcd;

namespace face {
namespace {

constexpr uint32_t FREQS[3] = {26666667, 40000000, 80000000};
constexpr int NFREQ = 3;
constexpr int REPS = 4;

struct FreqResult {
  float frameMs, composeMs, waitMs, convMs;  // nasza sciezka, na pelny ekran
  float rawMs, rawComposeMs, rawPushMs;      // sciezka 3-bajtowa
  int readErr, rawErr;                       // bledne piksele (z 3 x 480 sprawdzonych), -1 = brak testu
};
FreqResult res[NFREQ];
bool readbackOk = false;
float interlaceMs = 0, interlaceFullMs = 0;
float flashMBs = 0, ramMBs = 0;
float pxNs1 = 0, pxNs2 = 0, sinNs = 0, sqrtNs = 0, fdivNs = 0, idivNs = 0;
int bestFreq = 0;

char lines[16][64];
int lineCount = 0;

void report(const char* fmt, ...) {
  char buf[96];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  Serial.printf("[BENCH] %s\n", buf);
  if (lineCount < 16) strlcpy(lines[lineCount++], buf, sizeof(lines[0]));
}

inline float msSince(int64_t t0) { return (esp_timer_get_time() - t0) / 1000.0f; }

void setFreq(uint32_t hz) {
  lcd.waitDMA();
  lcd.getPanel()->getBus()->setClock(hz);
}

// ---------------------------------------------------------------- wzor testowy
uint32_t seed = 1;
inline void patternRgb(int x, int y, uint8_t& r, uint8_t& g, uint8_t& b) {
  r = uint8_t((x + seed * 37) & 0xF8);
  g = uint8_t(((y * 4 / 5) + seed * 11) & 0xFC);
  b = uint8_t(((x ^ y) + seed * 5) & 0xF8);
}

void patternCompose(int x0, int y0, int w, int h, uint16_t* out) {
  for (int r = 0; r < h; r++)
    for (int i = 0; i < w; i++) {
      uint8_t R, G, B;
      patternRgb(x0 + i, y0 + r, R, G, B);
      out[r * w + i] = px::swap(px::rgb565(R, G, B));
    }
}

// Odczyt 3 wierszy z ekranu i porownanie z biezacym wzorem (6 bitow/kanal)
int verifyPattern() {
  static uint8_t rgb[480 * 3];
  int bad = 0;
  lcd.waitDMA();
  for (int y : {7, 161, 313}) {
    lcd.readRectRGB(0, y, 480, 1, rgb);
    for (int x = 0; x < 480; x++) {
      uint8_t R, G, B;
      patternRgb(x, y, R, G, B);
      if ((rgb[x * 3] >> 3) != (R >> 3) || (rgb[x * 3 + 1] >> 2) != (G >> 2) || (rgb[x * 3 + 2] >> 3) != (B >> 3))
        bad++;
    }
  }
  return bad;
}

// ---------------------------------------------------------------- sciezka 3-bajtowa
uint8_t* raw[2] = {nullptr, nullptr};
constexpr int RAW_ROWS = 16;

void rawFrame(float& composeMs, float& pushMs) {
  int cur = 0;
  int64_t tc = 0, tp = 0;
  for (int y0 = 0; y0 < SCREEN_H; y0 += RAW_ROWS) {
    cur ^= 1;
    int64_t t = esp_timer_get_time();
    uint8_t* o = raw[cur];
    for (int y = y0; y < y0 + RAW_ROWS; y++)
      for (int x = 0; x < SCREEN_W; x++, o += 3) patternRgb(x, y, o[0], o[1], o[2]);
    tc += esp_timer_get_time() - t;
    t = esp_timer_get_time();
    lcd.waitDMA();
    lcd.setAddrWindow(0, y0, SCREEN_W, RAW_ROWS);
    lcd.writePixelsDMA(reinterpret_cast<const lgfx::bgr888_t*>(raw[cur]), SCREEN_W * RAW_ROWS);
    tp += esp_timer_get_time() - t;
  }
  lcd.waitDMA();
  composeMs = tc / 1000.0f;
  pushMs = tp / 1000.0f;
}

// ---------------------------------------------------------------- testy SPI
void testFreq(int fi) {
  FreqResult& r = res[fi];
  setFreq(FREQS[fi]);
  // nasza sciezka
  dmaBuffers.resetStats();
  int64_t t0 = esp_timer_get_time();
  for (int k = 0; k < REPS; k++) {
    seed++;
    pushComposed(0, 0, SCREEN_W, SCREEN_H, patternCompose);
  }
  lcd.waitDMA();
  r.frameMs = msSince(t0) / REPS;
  r.composeMs = dmaBuffers.stats.composeUs / 1000.0f / REPS;
  r.waitMs = dmaBuffers.stats.waitUs / 1000.0f / REPS;
  r.convMs = dmaBuffers.stats.pushUs / 1000.0f / REPS;
  r.readErr = verifyPattern();
  // sciezka 3-bajtowa
  if (raw[0]) {
    float c = 0, p = 0;
    t0 = esp_timer_get_time();
    for (int k = 0; k < REPS; k++) {
      seed++;
      float c1, p1;
      rawFrame(c1, p1);
      c += c1;
      p += p1;
    }
    r.rawMs = msSince(t0) / REPS;
    r.rawComposeMs = c / REPS;
    r.rawPushMs = p / REPS;
    r.rawErr = verifyPattern();
  } else {
    r.rawErr = -1;
  }
  Serial.printf("[BENCH] %.1f MHz: 565 %.1f ms (sklad %.1f, czek %.1f, konw %.1f) bl %d | 888 %.1f ms (sklad %.1f, "
                "push %.1f) bl %d\n",
                FREQS[fi] / 1e6f, r.frameMs, r.composeMs, r.waitMs, r.convMs, r.readErr, r.rawMs, r.rawComposeMs,
                r.rawPushMs, r.rawErr);
}

void testInterlace() {
  setFreq(FREQS[bestFreq]);
  seed++;
  int64_t t0 = esp_timer_get_time();
  for (int k = 0; k < REPS; k++)
    for (int y = k & 1; y < SCREEN_H; y += 2) pushComposed(0, y, SCREEN_W, 1, patternCompose);
  lcd.waitDMA();
  interlaceMs = msSince(t0) / REPS;
  t0 = esp_timer_get_time();
  for (int k = 0; k < REPS; k++) pushComposed(0, 0, SCREEN_W, SCREEN_H, patternCompose);
  lcd.waitDMA();
  interlaceFullMs = msSince(t0) / REPS;
}

// ---------------------------------------------------------------- pamiec
void testMemory() {
  const esp_partition_t* part = esp_ota_get_running_partition();
  const void* ptr = nullptr;
  esp_partition_mmap_handle_t h;
  constexpr size_t SZ = 1024 * 1024;
  if (esp_partition_mmap(part, 0, SZ, ESP_PARTITION_MMAP_DATA, &ptr, &h) == ESP_OK) {
    const volatile uint32_t* p = static_cast<const volatile uint32_t*>(ptr);
    uint32_t sum = 0;
    const int64_t t0 = esp_timer_get_time();
    for (size_t i = 0; i < SZ / 4; i++) sum += p[i];
    const float ms = msSince(t0);
    flashMBs = SZ / 1048576.0f / (ms / 1000.0f);
    esp_partition_munmap(h);
    Serial.printf("[BENCH] flash: 1 MB w %.1f ms (suma %08lx)\n", ms, (unsigned long)sum);
  }
  static uint32_t buf[8192];  // 32 KB w RAM
  for (auto& v : buf) v = esp_random();
  volatile uint32_t sum = 0;
  const int64_t t0 = esp_timer_get_time();
  for (int k = 0; k < 32; k++)
    for (const auto& v : buf) sum += v;
  ramMBs = 32 * sizeof(buf) / 1048576.0f / (msSince(t0) / 1000.0f);
}

// ---------------------------------------------------------------- CPU
// Typowy "ciezki" piksel jak w zorzy: dwie kurtyny, LUT-y, mieszanie kolorow
uint16_t colBase[2][480], colInv[2][480], colRay[2][480];
uint16_t lutA[256];
uint8_t colLut[65][3];

inline uint16_t heavyPixel(int x, int y) {
  int r = 4, g = 8, b = 20;
  for (int k = 0; k < 2; k++) {
    const int d = colBase[k][x] - y;
    if (d < 0) continue;
    int u = (d * colInv[k][x]) >> 8;
    if (u > 255) u = 255;
    const uint32_t I = (lutA[u] * colRay[k][x]) >> 8;
    const uint8_t* c = colLut[u < 64 ? u : 64];
    r += (c[0] * I) >> 8;
    g += (c[1] * I) >> 8;
    b += (c[2] * I) >> 8;
  }
  return px::dither(r > 255 ? 255 : r, g > 255 ? 255 : g, b > 255 ? 255 : b, x, y);
}

uint16_t* cpuOut = nullptr;
SemaphoreHandle_t startSem, doneSem;
volatile int halfRows = 0;

void heavyRows(int y0, int y1) {
  for (int y = y0; y < y1; y++)
    for (int x = 0; x < 480; x++) cpuOut[(y % 25) * 480 + x] = heavyPixel(x, y);
}

void core0Worker(void*) {
  for (;;) {
    xSemaphoreTake(startSem, portMAX_DELAY);
    heavyRows(halfRows, halfRows * 2);
    xSemaphoreGive(doneSem);
  }
}

void testCpu() {
  for (int k = 0; k < 2; k++)
    for (int x = 0; x < 480; x++) {
      colBase[k][x] = 120 + 40 * sinf(x / 50.0f + k);
      colInv[k][x] = 300;
      colRay[k][x] = 200;
    }
  for (int u = 0; u < 256; u++) lutA[u] = uint16_t(300 * expf(-u / 40.0f));
  for (int u = 0; u <= 64; u++) colLut[u][0] = 40 + u * 2, colLut[u][1] = 255 - u * 3, colLut[u][2] = 130 + u;
  cpuOut = static_cast<uint16_t*>(heap_caps_malloc(480 * 25 * 2, MALLOC_CAP_8BIT));
  if (!cpuOut) return;
  constexpr int ROWS = 200;
  int64_t t0 = esp_timer_get_time();
  heavyRows(0, ROWS);
  pxNs1 = (esp_timer_get_time() - t0) * 1000.0f / (ROWS * 480);

  startSem = xSemaphoreCreateBinary();
  doneSem = xSemaphoreCreateBinary();
  xTaskCreatePinnedToCore(core0Worker, "bench0", 4096, nullptr, 5, nullptr, 0);
  halfRows = ROWS / 2;
  t0 = esp_timer_get_time();
  xSemaphoreGive(startSem);
  heavyRows(0, ROWS / 2);
  xSemaphoreTake(doneSem, portMAX_DELAY);
  pxNs2 = (esp_timer_get_time() - t0) * 1000.0f / (ROWS * 480);

  constexpr int N = 20000;
  volatile float vf = 0.37f;
  volatile int vi = 123456789;
  float acc = 0;
  t0 = esp_timer_get_time();
  for (int i = 0; i < N; i++) acc += sinf(vf + i * 0.001f);
  sinNs = (esp_timer_get_time() - t0) * 1000.0f / N;
  t0 = esp_timer_get_time();
  for (int i = 0; i < N; i++) acc += sqrtf(vf + i);
  sqrtNs = (esp_timer_get_time() - t0) * 1000.0f / N;
  t0 = esp_timer_get_time();
  for (int i = 0; i < N; i++) acc += 1.0f / (vf + i);
  fdivNs = (esp_timer_get_time() - t0) * 1000.0f / N;
  int iacc = 0;
  t0 = esp_timer_get_time();
  for (int i = 1; i <= N; i++) iacc += vi / i;
  idivNs = (esp_timer_get_time() - t0) * 1000.0f / N;
  Serial.printf("[BENCH] (anty-optymalizacja: %f %d)\n", acc, iacc);
  heap_caps_free(cpuOut);
  cpuOut = nullptr;
}

// ---------------------------------------------------------------- stres (plazma)
constexpr int STRESS_Y = 196, STRESS_H = SCREEN_H - STRESS_Y;
uint16_t plasmaPal[256];
int8_t sinTab[256];
uint32_t stressT = 0, stressFrames = 0, stressChecks = 0, stressBadFrames = 0, stressBadPx = 0;
uint32_t fpsFrames = 0, fpsStart = 0;
float stressFps = 0, stressFrameMs = 0;

inline uint16_t plasma(int x, int y, uint32_t t) {
  const int v = sinTab[(x * 2 + t * 3) & 255] + sinTab[(y * 3 - t * 2) & 255] + sinTab[((x + y) + t * 4) & 255] +
                sinTab[((x >> 1) - (y << 1) + t) & 255];
  return plasmaPal[(v + 512) >> 2 & 255];
}

void stressCompose(int x0, int y0, int w, int h, uint16_t* out) {
  for (int r = 0; r < h; r++)
    for (int i = 0; i < w; i++) out[r * w + i] = px::swap(plasma(x0 + i, y0 + r, stressT));
}

int verifyStress() {
  static uint8_t rgb[480 * 3];
  lcd.waitDMA();
  const int y = STRESS_Y + 60;
  lcd.readRectRGB(0, y, 480, 1, rgb);
  int bad = 0;
  for (int x = 0; x < 480; x++) {
    const uint16_t c = plasma(x, y, stressT);
    const int R = (c >> 11) & 31, G = (c >> 5) & 63, B = c & 31;
    if ((rgb[x * 3] >> 3) != R || (rgb[x * 3 + 1] >> 2) != G || (rgb[x * 3 + 2] >> 3) != B) bad++;
  }
  return bad;
}

void drawTable() {
  lcd.fillRect(0, 0, SCREEN_W, STRESS_Y, TFT_BLACK);
  lcd.setFont(&fonts::Font2);
  lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  for (int i = 0; i < lineCount; i++) lcd.drawString(lines[i], 4, 2 + i * 15);
}

void drawStressLine() {
  char buf[96];
  snprintf(buf, sizeof(buf), "STRES %.1f MHz: %.1f fps (%.0f ms) bledne klatki %lu/%lu, px %lu   ",
           FREQS[bestFreq] / 1e6f, stressFps, stressFrameMs, (unsigned long)stressBadFrames,
           (unsigned long)stressChecks, (unsigned long)stressBadPx);
  lcd.setFont(&fonts::Font2);
  lcd.setTextColor(stressBadFrames ? TFT_RED : TFT_GREEN, TFT_BLACK);
  lcd.drawString(buf, 4, STRESS_Y - 16);
}

}  // namespace

void begin() {
  for (int i = 0; i < 256; i++) {
    sinTab[i] = int8_t(127 * sinf(i * 2 * float(M_PI) / 256));
    const float a = i / 256.0f * 2 * float(M_PI);
    plasmaPal[i] = px::rgb565(uint8_t(128 + 127 * sinf(a)), uint8_t(128 + 127 * sinf(a + 2.1f)),
                              uint8_t(128 + 127 * sinf(a + 4.2f)));
  }
  for (auto& b : raw) b = static_cast<uint8_t*>(heap_caps_malloc(SCREEN_W * RAW_ROWS * 3, MALLOC_CAP_DMA));
}

void drawAll() {
  Serial.printf("[BENCH] start: CPU %lu MHz, flash %lu MHz tryb %d, heap %u B (blok %u)\n",
                (unsigned long)getCpuFrequencyMhz(), (unsigned long)(ESP.getFlashChipSpeed() / 1000000),
                int(ESP.getFlashChipMode()), ESP.getFreeHeap(), ESP.getMaxAllocHeap());
  // 1-2: zegary SPI (80 MHz na koncu - moze rozstroic panel, potem init)
  for (int fi = 0; fi < NFREQ; fi++) testFreq(fi);
  setFreq(FREQS[0]);
  lcd.endWrite();
  lcd.init();
  lcd.setRotation(1);
  lcd.startWrite();
  readbackOk = res[0].readErr == 0;
  bestFreq = 0;
  for (int fi = 1; fi < NFREQ; fi++)
    if (readbackOk ? res[fi].readErr == 0 : fi == 1) bestFreq = fi;
  // 3-5
  testInterlace();
  testMemory();
  testCpu();
  setFreq(FREQS[bestFreq]);

  report("BENCHMARK  (odczyt z ekranu: %s)", readbackOk ? "dziala" : "NIE dziala - ocena na oko");
  for (int fi = 0; fi < NFREQ; fi++) {
    const FreqResult& r = res[fi];
    report("%4.1f MHz: %5.1f ms/ekran (sklad %4.1f czek %4.1f konw %4.1f) bl %d", FREQS[fi] / 1e6f, r.frameMs,
           r.composeMs, r.waitMs, r.convMs, r.readErr);
    report("   3-bajt: %5.1f ms/ekran (sklad %4.1f push %4.1f) bl %d", r.rawMs, r.rawComposeMs, r.rawPushMs, r.rawErr);
  }
  report("Przeplot @%.1f MHz: %.1f ms (pelny %.1f ms)", FREQS[bestFreq] / 1e6f, interlaceMs, interlaceFullMs);
  report("Flash %.1f MB/s, RAM %.0f MB/s", flashMBs, ramMBs);
  report("Piksel zorzy: %.0f ns (1 rdzen), %.0f ns (2 rdzenie)", pxNs1, pxNs2);
  report("sinf %.0f ns, sqrtf %.0f ns, 1/x %.0f ns, int / %.0f ns", sinNs, sqrtNs, fdivNs, idivNs);
  drawTable();
  fpsStart = millis();
}

void frame(uint32_t nowMs, const struct tm* now) {
  const int64_t t0 = esp_timer_get_time();
  stressT++;
  pushComposed(0, STRESS_Y, SCREEN_W, STRESS_H, stressCompose);
  stressFrames++;
  if (stressFrames % 25 == 0) {
    const int bad = verifyStress();
    stressChecks++;
    if (bad > 0) {
      stressBadFrames++;
      stressBadPx += bad;
    }
  }
  fpsFrames++;
  stressFrameMs = msSince(t0);
  if (nowMs - fpsStart >= 2000) {
    stressFps = fpsFrames * 1000.0f / (nowMs - fpsStart);
    fpsFrames = 0;
    fpsStart = nowMs;
    drawStressLine();
  }
}

}  // namespace face
