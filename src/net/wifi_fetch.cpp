#include "wifi_fetch.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WroomLink.h>
#include <esp_sntp.h>
#include <time.h>

#include "config.h"
#include "secrets.h"

namespace {

constexpr uint32_t NTP_RETRY_MS = 10UL * 60UL * 1000UL;  // po porazce: 10 min
constexpr uint32_t NTP_SYNC_TIMEOUT_MS = 10000;

bool syncNtp() {
  sntp_set_sync_status(SNTP_SYNC_STATUS_RESET);
  configTzTime(TZ_POLAND, NTP_SERVER);
  const uint32_t start = millis();
  while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED) {
    if (millis() - start > NTP_SYNC_TIMEOUT_MS) {
      esp_sntp_stop();
      return false;
    }
    delay(100);
  }
  // Bez tego SNTP probowaloby co godzine po odlaczonym Wi-Fi
  esp_sntp_stop();
  return true;
}

void ntpTask(void*) {
  for (;;) {
    const uint32_t t0 = millis();
    const bool ok = wifiFetch(syncNtp);
    Serial.printf("[NTP] %s (%lu ms)\n", ok ? "zsynchronizowano" : "nieudane",
                  (unsigned long)(millis() - t0));
    vTaskDelay(pdMS_TO_TICKS(ok ? NTP_INTERVAL_MS : NTP_RETRY_MS));
  }
}

}  // namespace

bool wifiFetch(const std::function<bool()>& job) {
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS) delay(100);

  bool ok = false;
  if (WiFi.status() == WL_CONNECTED) {
    ok = job();
  } else {
    Serial.printf("[WIFI] brak polaczenia (status %d)\n", WiFi.status());
  }

  WiFi.disconnect(false);  // false = radio zostaje wlaczone
  delay(50);
  WroomLink::restoreChannel();
  return ok;
}

void ntpTaskStart() {
  setenv("TZ", TZ_POLAND, 1);
  tzset();
  xTaskCreatePinnedToCore(ntpTask, "ntp", 4096, nullptr, 1, nullptr, 0);
}

bool timeValid() {
  return time(nullptr) > 1700000000;  // > 2023 = NTP juz zadzialal
}
