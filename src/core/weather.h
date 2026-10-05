#pragma once
// Pogoda z bramki ESP-NET (UDP 4210 na 192.168.50.1). Bramka pobiera
// OpenWeather co 15 min; zegarek pyta ja przy kazdym polaczeniu Wi-Fi
// (zadanie sieci w wifi_fetch, co 15 min). Dane uznawane za aktualne 2 h.
//
// Test wygladu bez bramki: -DWEATHER_DEMO_S=20 (co 20 s kolejna pogoda).

#include <stdint.h>

namespace weather {

enum class Kind : uint8_t { None, Clear, Clouds, Fog, Rain, Snow, Storm };

struct Data {
  bool valid = false;
  int16_t temp10 = 0, feels10 = 0, min10 = 0, max10 = 0;  // st. C x10
  uint16_t code = 0;                                       // kod OpenWeather
  uint8_t clouds = 0;                                      // %
  uint16_t rain10 = 0, snow10 = 0, wind10 = 0;             // mm/h x10, m/s x10
  bool day = true;
  Kind kind = Kind::None;
  uint8_t intensity = 0;  // 0 brak, 1 slabo, 2 umiarkowanie, 3 mocno
  uint32_t serial = 0;    // rosnie przy kazdej zmianie danych
};

// Biezace dane (kopia; bezpieczne z innego rdzenia)
Data get();

// Zapytanie bramki - wolac w zadaniu sieci przy polaczonym Wi-Fi
bool query();

// Temperatura zaokraglona do stopni, np. "-3" / "12"
void formatTemp(const Data& d, char* out, int len);

}  // namespace weather
