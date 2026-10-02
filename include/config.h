#pragma once
// Wspolna konfiguracja zegarka: piny, geometria ekranu, czasy.

#include <stdint.h>

// ---- Wyswietlacz ILI9488 (pinout z plikow projektu, NIE z szablonow) ----
// GPIO13 celowo nieuzywany.
constexpr int PIN_TFT_CS   = 15;
constexpr int PIN_TFT_DC   = 2;
constexpr int PIN_TFT_RST  = 4;
constexpr int PIN_TFT_MOSI = 23;
constexpr int PIN_TFT_SCLK = 18;
constexpr int PIN_TFT_MISO = 19;
constexpr int PIN_TFT_BL   = 32;   // aktywne HIGH, bez PWM

constexpr uint32_t TFT_SPI_WRITE_HZ = 27000000;  // zweryfikowane na sprzecie
constexpr uint32_t TFT_SPI_READ_HZ  = 16000000;

// ---- Geometria (landscape 480x320) ----
constexpr int SCREEN_W = 480;
constexpr int SCREEN_H = 320;

constexpr int DATE_TOP    = 0;     // pas daty: y 0-40
constexpr int DATE_BOTTOM = 40;
constexpr int HORIZON_Y   = 220;   // horyzont, srodek slonca
constexpr int GRID_TOP    = HORIZON_Y;
constexpr int GRID_H      = SCREEN_H - GRID_TOP;  // siatka y 220-320
constexpr int SIDE_W      = 75;    // boki na animacje (poza pasem cyfr)

// ---- Klatki ----
// Obecny zegarek: 11 FPS (limit SPI). Trzymamy ten poziom, nie podnosimy.
constexpr uint32_t FRAME_MS = 90;

// ---- Czas / NTP ----
constexpr const char* NTP_SERVER = "pool.ntp.org";
constexpr const char* TZ_POLAND  = "CET-1CEST,M3.5.0,M10.5.0/3";
constexpr uint32_t NTP_INTERVAL_MS    = 6UL * 3600UL * 1000UL;  // 6 h
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 10000;

// ---- wroom_link ----
// Na razie staly kanal; skan 1-13 + zapis w NVS dochodzi w etapie 4.
constexpr uint8_t LINK_DEFAULT_CHANNEL = 1;
