// ESP32 Synthwave Watch v2 (PlatformIO)
// Render na rdzeniu 1 (loop), NTP/Wi-Fi w osobnym zadaniu na rdzeniu 0.

#include <Arduino.h>
#include <WiFi.h>
#include <esp_app_desc.h>
#include <esp_ota_ops.h>
#include <time.h>

#include "config.h"
#include "display/lgfx_config.h"
#include "net/wifi_fetch.h"
#include "render/animator.h"
#include "render/dma_buffers.h"
#include "secrets.h"
#include <WroomLink.h>

LGFX lcd;

static void logBootInfo() {
  const esp_partition_t* running = esp_ota_get_running_partition();
  const esp_app_desc_t* desc = esp_app_get_description();
  Serial.printf("[BOOT] %s %s (app_desc: %s %s), partycja %s @0x%06lX\n", APP_NAME,
                APP_VERSION, desc->project_name, desc->version, running->label,
                (unsigned long)running->address);
}

void setup() {
  Serial.begin(115200);
  delay(200);
  logBootInfo();

  lcd.init();
  lcd.setRotation(1);
  pinMode(PIN_TFT_BL, OUTPUT);
  digitalWrite(PIN_TFT_BL, HIGH);

  if (!dmaBuffers.begin(&lcd)) {
    Serial.println("[BLAD] brak pamieci DMA na bufory");
    for (;;) delay(1000);
  }

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WroomLink::begin(APP_NAME, APP_VERSION, LINK_DEFAULT_CHANNEL, WROOM_LINK_KEY);

  animator::begin();
  lcd.startWrite();
  animator::drawAll();
  lcd.endWrite();

  ntpTaskStart();
  Serial.printf("[READY] heap %u B, max blok %u B\n", ESP.getFreeHeap(), ESP.getMaxAllocHeap());
}

void loop() {
  static uint32_t nextFrame = millis();
  static uint32_t frames = 0, worstMs = 0, statsStart = millis();

  const uint32_t t0 = millis();
  struct tm now;
  const bool valid = timeValid();
  if (valid) {
    time_t t = time(nullptr);
    localtime_r(&t, &now);
  }

  lcd.startWrite();
  animator::frame(t0, valid ? &now : nullptr);
  lcd.endWrite();

  WroomLink::poll();

  const uint32_t took = millis() - t0;
  if (took > worstMs) worstMs = took;
  frames++;
  if (t0 - statsStart >= 10000) {
    Serial.printf("[FPS] %.1f, najdluzsza klatka %lu ms, heap %u B\n",
                  frames * 1000.0f / (t0 - statsStart), (unsigned long)worstMs, ESP.getFreeHeap());
    frames = 0;
    worstMs = 0;
    statsStart = t0;
  }

  // Stale tempo klatek (FRAME_MS); przy przekroczeniu nie nadrabiamy
  nextFrame += FRAME_MS;
  const int32_t wait = int32_t(nextFrame - millis());
  if (wait > 0) delay(wait);
  else nextFrame = millis();
}
