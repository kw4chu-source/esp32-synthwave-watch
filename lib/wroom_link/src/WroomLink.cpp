#include "WroomLink.h"

#include <WiFi.h>
#include <esp_mac.h>
#include <esp_now.h>
#include <esp_ota_ops.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <string.h>

using namespace wroom_link;

uint8_t WroomLink::s_channel = 1;
const char* WroomLink::s_name = "";
const char* WroomLink::s_version = "";
const char* WroomLink::s_key = "";

namespace {

constexpr size_t MAX_FRAME = 250;  // limit ESP-NOW v1

struct RxFrame {
  uint8_t mac[6];
  uint8_t len;
  uint8_t data[MAX_FRAME];
};

QueueHandle_t s_rxQueue = nullptr;
uint8_t s_selfMac[6];
uint32_t s_txSeq = 0;
uint32_t s_usedNonces[8];
int s_usedNonceCount = 0;

// Callback dziala w zadaniu Wi-Fi - tylko kopiujemy ramke do kolejki.
void onRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  if (len < (int)sizeof(Header) || len > (int)MAX_FRAME) return;
  RxFrame f;
  memcpy(f.mac, info->src_addr, 6);
  f.len = (uint8_t)len;
  memcpy(f.data, data, len);
  xQueueSend(s_rxQueue, &f, 0);  // pelna kolejka = ramka gubiona, Cardputer ponowi
}

void sendTo(const uint8_t mac[6], const void* msg, size_t len) {
  if (!esp_now_is_peer_exist(mac)) {
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, mac, 6);
    peer.channel = 0;  // biezacy kanal radia
    peer.ifidx = WIFI_IF_STA;
    esp_now_add_peer(&peer);
  }
  esp_now_send(mac, static_cast<const uint8_t*>(msg), len);
}

bool nonceUsed(uint32_t nonce) {
  for (int i = 0; i < s_usedNonceCount; i++)
    if (s_usedNonces[i] == nonce) return true;
  s_usedNonces[s_usedNonceCount++ % 8] = nonce;
  if (s_usedNonceCount > 8) s_usedNonceCount = 8;
  return false;
}

void enterLoader() {
  const esp_partition_t* factory =
      esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, nullptr);
  if (!factory || esp_ota_set_boot_partition(factory) != ESP_OK) {
    Serial.println("[LINK] brak poprawnego loadera w factory!");
    return;
  }
  Serial.println("[LINK] restart do loadera");
  Serial.flush();
  esp_restart();
}

}  // namespace

bool WroomLink::begin(const char* appName, const char* appVersion, uint8_t channel,
                      const char* key) {
  s_name = appName;
  s_version = appVersion;
  s_channel = channel;
  s_key = key;
  s_txSeq = esp_random();

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

  esp_read_mac(s_selfMac, ESP_MAC_WIFI_STA);
  Serial.printf("[LINK] ESP-NOW nasluch, kanal %u, %s %s, MAC %02X:%02X:%02X:%02X:%02X:%02X\n",
                s_channel, s_name, s_version, s_selfMac[0], s_selfMac[1], s_selfMac[2],
                s_selfMac[3], s_selfMac[4], s_selfMac[5]);
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

    if (h.type == MsgType::Discover) {
      DiscoverReplyMsg r = {};
      r.h = makeHeader(MsgType::DiscoverReply, ++s_txSeq);
      strlcpy(r.name, s_name, sizeof(r.name));
      strlcpy(r.version, s_version, sizeof(r.version));
      sendTo(f.mac, &r, sizeof(r));
      Serial.printf("[LINK] DISCOVER od %02X:%02X:%02X:%02X:%02X:%02X -> odpowiedz\n", f.mac[0],
                    f.mac[1], f.mac[2], f.mac[3], f.mac[4], f.mac[5]);

    } else if (h.type == MsgType::CmdLoader && f.len == sizeof(CmdLoaderMsg)) {
      const CmdLoaderMsg& m = *reinterpret_cast<const CmdLoaderMsg*>(f.data);
      uint8_t want[32];
      cmdLoaderHmac(s_key, m, s_selfMac, want);

      AckMsg ack = {};
      ack.h = makeHeader(MsgType::Ack, ++s_txSeq);
      ack.ackSeq = h.seq;
      if (!hmacEqual(want, m.hmac)) ack.code = ACK_BAD_HMAC;
      else if (nonceUsed(m.nonce)) ack.code = ACK_REPLAY;
      else ack.code = ACK_OK;
      sendTo(f.mac, &ack, sizeof(ack));
      Serial.printf("[LINK] CMD_LOADER seq=%lu -> %s\n", (unsigned long)h.seq,
                    ack.code == ACK_OK ? "OK" : (ack.code == ACK_BAD_HMAC ? "zly HMAC" : "powtorka"));

      if (ack.code == ACK_OK) {
        delay(100);  // ACK musi wyjsc przed restartem
        enterLoader();
      }
    }
  }
}
