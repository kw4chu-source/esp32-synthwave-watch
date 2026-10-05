#include "core/weather.h"

#include <Arduino.h>
#include <WiFiUdp.h>
#include <stdio.h>

#include "config.h"

namespace weather {
namespace {

constexpr uint16_t WX_PORT = 4210;
constexpr uint32_t STALE_MS = 2UL * 3600UL * 1000UL;

Data cur;
uint32_t storedMs = 0;  // millis() odbioru, skorygowane o wiek danych w bramce
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

void classify(Data& d) {
  const int c = d.code;
  const int mmh = d.rain10 > d.snow10 ? d.rain10 : d.snow10;  // x10
  if (c >= 200 && c < 300) d.kind = Kind::Storm;
  else if (c >= 300 && c < 600) d.kind = Kind::Rain;
  else if (c >= 600 && c < 700) d.kind = Kind::Snow;
  else if (c >= 700 && c < 800) d.kind = Kind::Fog;
  else if (c == 800 || c == 801) d.kind = Kind::Clear;
  else if (c > 801) d.kind = Kind::Clouds;
  else d.kind = Kind::None;

  if (d.kind == Kind::Rain || d.kind == Kind::Snow || d.kind == Kind::Storm) {
    if (mmh > 0) d.intensity = mmh < 10 ? 1 : (mmh < 40 ? 2 : 3);
    else {
      const int sub = c % 100;  // bez pomiaru opadu: z kodu (x00 slabo, x01 umiark., x02+ mocno)
      d.intensity = (c < 400) ? 1 : (sub == 0 ? 1 : (sub == 1 ? 2 : 3));
    }
    if (d.kind == Kind::Storm && d.intensity < 2) d.intensity = 2;
  } else {
    d.intensity = 0;
  }
}

#ifdef WEATHER_DEMO_S
Data demo() {
  static const struct {
    uint16_t code;
    int16_t temp10;
    uint8_t clouds;
    uint16_t rain10, snow10;
  } STEPS[] = {{800, 182, 0, 0, 0},   {803, 121, 80, 0, 0}, {500, 93, 90, 5, 0},  {502, 88, 100, 60, 0},
               {211, 164, 100, 30, 0}, {601, -31, 100, 0, 15}, {741, 42, 100, 0, 0}};
  const int n = sizeof(STEPS) / sizeof(STEPS[0]);
  const int i = (millis() / (WEATHER_DEMO_S * 1000UL)) % n;
  Data d;
  d.valid = true;
  d.code = STEPS[i].code;
  d.temp10 = d.feels10 = STEPS[i].temp10;
  d.clouds = STEPS[i].clouds;
  d.rain10 = STEPS[i].rain10;
  d.snow10 = STEPS[i].snow10;
  d.day = true;
  d.serial = i + 1;
  classify(d);
  return d;
}
#endif

}  // namespace

Data get() {
#ifdef WEATHER_DEMO_S
  return demo();
#else
  portENTER_CRITICAL(&mux);
  Data d = cur;
  const uint32_t at = storedMs;
  portEXIT_CRITICAL(&mux);
  if (d.valid && millis() - at > STALE_MS) d.valid = false;
  return d;
#endif
}

bool query() {
  WiFiUDP udp;
  if (!udp.begin(0)) return false;
  const IPAddress gw(192, 168, 50, 1);
  bool ok = false;
  for (int attempt = 0; attempt < 3 && !ok; attempt++) {
    udp.beginPacket(gw, WX_PORT);
    udp.print("WX?");
    udp.endPacket();
    const uint32_t t0 = millis();
    while (millis() - t0 < 1000) {
      if (udp.parsePacket() > 0) {
        char buf[96];
        const int n = udp.read(buf, sizeof(buf) - 1);
        buf[n > 0 ? n : 0] = 0;
        int age, t, f, code, cl, r, s, wd, day, mn, mx;
        if (sscanf(buf, "WX1 %d %d %d %d %d %d %d %d %d %d %d", &age, &t, &f, &code, &cl, &r, &s, &wd, &day, &mn,
                   &mx) == 11) {
          ok = true;
          if (age < 0) {
            Serial.println("[POGODA] bramka nie ma jeszcze danych");
            break;
          }
          Data d;
          d.valid = true;
          d.temp10 = t, d.feels10 = f, d.min10 = mn, d.max10 = mx;
          d.code = code, d.clouds = cl, d.rain10 = r, d.snow10 = s, d.wind10 = wd, d.day = day;
          classify(d);
          portENTER_CRITICAL(&mux);
          const bool changed = !cur.valid || cur.code != d.code || cur.temp10 / 5 != d.temp10 / 5 ||
                               cur.intensity != d.intensity || cur.day != d.day;
          d.serial = cur.serial + (changed ? 1 : 0);
          cur = d;
          storedMs = millis() - uint32_t(age) * 1000UL;
          portEXIT_CRITICAL(&mux);
          Serial.printf("[POGODA] %.1f C, kod %d, opad %.1f/%.1f mm/h (dane sprzed %d s)\n", t / 10.0f, code,
                        r / 10.0f, s / 10.0f, age);
        }
        break;
      }
      delay(20);
    }
  }
  udp.stop();
  return ok;
}

void formatTemp(const Data& d, char* out, int len) {
  const int t = (d.temp10 >= 0 ? d.temp10 + 5 : d.temp10 - 5) / 10;
  snprintf(out, len, "%d", t);
}

}  // namespace weather
