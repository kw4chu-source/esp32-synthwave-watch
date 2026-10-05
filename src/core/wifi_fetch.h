#pragma once
// Wi-Fi tylko na czas zadania: polacz -> zadanie -> rozlacz, radio zostaje
// wlaczone na kanale ESP-NOW (wroom_link). Zadnego stalego polaczenia,
// zadnego auto-reconnect, nic nie zapisuje sie we flashu.

#include <functional>

// Blokuje do WIFI_CONNECT_TIMEOUT_MS + czas zadania. Wolac poza petla renderu.
bool wifiFetch(const std::function<bool()>& job);

// Zadanie FreeRTOS (rdzen 0): co 15 min Wi-Fi -> pogoda z bramki (+ NTP co NTP_INTERVAL_MS).
void ntpTaskStart();

// Czy zegar systemowy ma juz prawdziwy czas
bool timeValid();
