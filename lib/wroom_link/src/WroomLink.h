#pragma once
// wroom_link - strona projektu WROOM.
// Nasluch ESP-NOW: DISCOVER -> odpowiedz nazwa/wersja; CMD_LOADER z
// poprawnym HMAC -> ACK -> start z factory (loader). Radio zostaje na
// kanale Cardputera (wifiFetch() wraca na niego przez restoreChannel()).
// TODO etap 4: skan kanalow 1-13 i zapis kanalu Cardputera w NVS.

#include <Arduino.h>
#include "wroom_link_proto.h"

class WroomLink {
public:
  // Wolac w setup(), po WiFi.mode(). key = WROOM_LINK_KEY (wspolny z Cardputerem).
  static bool begin(const char* appName, const char* appVersion, uint8_t channel,
                    const char* key);

  // Wolac w loop() - obsluga ramek odebranych w callbacku ESP-NOW.
  static void poll();

  static uint8_t channel() { return s_channel; }
  static void restoreChannel();

private:
  static uint8_t s_channel;
  static const char* s_name;
  static const char* s_version;
  static const char* s_key;
};
