// ESP32 Synthwave Watch v2 - szkielet PlatformIO (etap 1, krok 1)
// Na tym etapie: start wyswietlacza, diagnostyka partycji/app_desc, wroom_link.
// Scena (niebo, slonce, siatka, cyfry) dochodzi w kolejnych krokach.

#include <Arduino.h>
#include <WiFi.h>
#include <esp_app_desc.h>
#include <esp_ota_ops.h>

#include "config.h"
#include "display/lgfx_config.h"
#include <WroomLink.h>

LGFX lcd;

static void logBootInfo() {
  const esp_partition_t* running = esp_ota_get_running_partition();
  const esp_partition_t* boot = esp_ota_get_boot_partition();
  const esp_app_desc_t* desc = esp_app_get_description();

  Serial.printf("[BOOT] %s %s (app_desc: %s %s)\n", APP_NAME, APP_VERSION,
                desc->project_name, desc->version);
  Serial.printf("[BOOT] partycja: %s @0x%06lX, boot: %s\n", running->label,
                (unsigned long)running->address, boot ? boot->label : "?");
  Serial.printf("[HEAP] wolne %u B, max blok %u B\n", ESP.getFreeHeap(),
                ESP.getMaxAllocHeap());
}

static void drawSkeletonScreen() {
  const esp_partition_t* running = esp_ota_get_running_partition();
  lcd.fillScreen(TFT_BLACK);
  lcd.setTextDatum(lgfx::middle_center);
  lcd.setFont(&fonts::FreeSansBold18pt7b);
  lcd.setTextColor(lgfx::color565(255, 100, 200));
  lcd.drawString(APP_NAME, SCREEN_W / 2, 120);
  lcd.setFont(&fonts::FreeSans12pt7b);
  lcd.setTextColor(lgfx::color565(0, 240, 255));
  lcd.drawString(APP_VERSION, SCREEN_W / 2, 165);
  lcd.setTextColor(lgfx::color565(133, 119, 173));
  lcd.drawString(String("partycja: ") + running->label, SCREEN_W / 2, 205);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  logBootInfo();

  lcd.init();
  lcd.setRotation(1);
  pinMode(PIN_TFT_BL, OUTPUT);
  digitalWrite(PIN_TFT_BL, HIGH);
  drawSkeletonScreen();

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WroomLink::begin(APP_NAME, APP_VERSION, LINK_DEFAULT_CHANNEL);

  Serial.println("[READY]");
}

void loop() {
  WroomLink::poll();
  delay(10);
}
