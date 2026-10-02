#include "WroomLink.h"

#include <WiFi.h>
#include <esp_now.h>
#include <esp_mac.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <string.h>

using namespace wroom_link;

uint8_t WroomLink::s_channel = 1;
const char* WroomLink::s_name = "";
const char* WroomLink::s_version = "";

namespace {

constexpr size_t MAX_FRAME = 250;  // limit ESP-NOW v1

struct RxFrame {
  uint8_t mac[6];
  uint8_t len;
  uint8_t data[MAX_FRAME];
};

QueueHandle_t s_rxQueue = nullptr;

// Callback dziala w zadaniu Wi-Fi - tylko kopiujemy ramke do kolejki.
void onRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  if (len < (int)sizeof(Header) || len > (int)MAX_FRAME) return;
  RxFrame f;
  memcpy(f.mac, info->src_addr, 6);
  f.len = (uint8_t)len;
  memcpy(f.data, data, len);
  xQueueSend(s_rxQueue, &f, 0);  // pelna kolejka = ramka gubiona, Cardputer ponowi
}

}  // namespace

bool WroomLink::begin(const char* appName, const char* appVersion, uint8_t channel) {
  s_name = appName;
  s_version = appVersion;
  s_channel = channel;

  if (WiFi.getMode() == WIFI_OFF) WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);  // bez modem-sleep ESP-NOW gubi ramki
  restoreChannel();

  s_rxQueue = xQueueCreate(4, sizeof(RxFrame));
  if (!s_rxQueue) return false;

  if (esp_now_init() != ESP_OK) {
    log_e("esp_now_init nieudane");
    return false;
  }
  esp_now_register_recv_cb(onRecv);

  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  Serial.printf("[LINK] ESP-NOW nasluch, kanal %u, %s %s, MAC %02X:%02X:%02X:%02X:%02X:%02X\n",
                s_channel, s_name, s_version, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return true;
}

void WroomLink::restoreChannel() {
  esp_wifi_set_channel(s_channel, WIFI_SECOND_CHAN_NONE);
}

void WroomLink::poll() {
  if (!s_rxQueue) return;
  RxFrame f;
  while (xQueueReceive(s_rxQueue, &f, 0) == pdTRUE) {
    const Header& h = *reinterpret_cast<const Header*>(f.data);
    if (!headerValid(h)) continue;

    switch (h.type) {
      case MsgType::Discover:
        // TODO etap 4: odpowiedz DiscoverReplyMsg (nazwa/wersja)
        Serial.printf("[LINK] DISCOVER od %02X:%02X:%02X:%02X:%02X:%02X seq=%lu\n",
                      f.mac[0], f.mac[1], f.mac[2], f.mac[3], f.mac[4], f.mac[5],
                      (unsigned long)h.seq);
        break;
      case MsgType::CmdLoader:
        // TODO etap 2/4: HMAC -> ACK -> esp_ota_set_boot_partition(factory) -> restart
        Serial.printf("[LINK] CMD_LOADER seq=%lu (ignorowane w szkielecie)\n",
                      (unsigned long)h.seq);
        break;
      default:
        break;
    }
  }
}
