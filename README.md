# ESP32 Synthwave Watch

Zegar w stylu cyberpunk/retrosynth na ESP32 + 3.5" TFT ILI9488, z animowaną
perspektywiczną siatką (jak z grafiki lat 80.) i zegarem pobierającym czas z NTP.

![status](https://img.shields.io/badge/status-działa-brightgreen)

## Jak to wygląda

Górna 1/3 ekranu: godzina (`HH:MM`) czcionką Orbitron Bold, sam obrys (bez
wypełnienia) w kolorze magenta, dokładnie takim samym jak pionowe linie
siatki poniżej. Dolne 2/3: animowana, przewijająca się perspektywiczna
siatka z gradientem tła (czerń przy horyzoncie → fiolet bliżej "kamery").

## Sprzęt

- ESP32 (WROOM-32, dowolny klon)
- Wyświetlacz dotykowy TFT LCD 3.5" **ILI9488** SPI 320×480

Podłączenie:

| Sygnał | GPIO |
|---|---|
| CS   | 15 |
| DC   | 2  |
| RST  | 4  |
| MOSI | 23 |
| SCLK | 18 |
| MISO | 19 |
| BL (podświetlenie) | 32 |

## Biblioteki

- [LovyanGFX](https://github.com/lovyan03/LovyanGFX) — sterownik wyświetlacza
  (TFT_eSPI był testowany jako pierwszy, ale jego `pushImageDMA()` nie
  gwarantuje pamięci DMA-capable i realnie uszkodził flash na tym sprzęcie;
  LovyanGFX ma `dma_channel` jako jawną opcję konfiguracji i działa stabilnie)
- WiFi + LittleFS — wbudowane w rdzeń `esp32` dla Arduino

## Wgrywanie

1. Otwórz `synthwave_grid_lgfx/synthwave_grid_lgfx.ino` w Arduino IDE.
2. Ustaw płytkę na ESP32 Dev Module, **Flash Mode: DIO** (tryb QIO powoduje
   crash/boot-loop na części tanich klonów ESP32).
3. Wgraj plik `synthwave_grid_lgfx/data/Orbitron90.vlw` do LittleFS (np.
   wtyczką "ESP32 Sketch Data Upload" albo `esptool.py`/`mklittlefs`
   ręcznie pod partycję `spiffs`) — to własna, antyaliasowana czcionka
   zegara, wygenerowana przez [fonts.atomic14.com](https://fonts.atomic14.com/).
   Bez tego zegar spadnie na wbudowaną czcionkę Font7 (nadal działa, tylko
   mniej ładnie).
4. Wgraj szkic.
5. W kodzie podmień SSID w `wifiSsid1`/`wifiSsid2` na swoją sieć WiFi.

## Licencja

MIT — rób z tym co chcesz.
