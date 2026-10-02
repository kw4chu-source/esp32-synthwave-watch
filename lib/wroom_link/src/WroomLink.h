#pragma once
// wroom_link - szkielet (etap 1).
// Nasluch ESP-NOW na DISCOVER i CMD_LOADER; obsluga komend na razie pusta
// (tylko log). Pelna wersja: odpowiedz na DISCOVER, weryfikacja HMAC, ACK,
// przelaczenie na factory, skan kanalow 1-13 z zapisem w NVS.

#include <Arduino.h>
#include "wroom_link_proto.h"

class WroomLink {
public:
  // Wolac w setup(), po WiFi.mode(). Ustawia WiFi.setSleep(false) i kanal.
  static bool begin(const char* appName, const char* appVersion, uint8_t channel);

  // Wolac w loop() - obsluga ramek odebranych w callbacku ESP-NOW.
  static void poll();

  // Kanal, na ktorym radio czeka na Cardputera (po wifiFetch() trzeba wrocic).
  static uint8_t channel() { return s_channel; }
  static void restoreChannel();

private:
  static uint8_t s_channel;
  static const char* s_name;
  static const char* s_version;
};
