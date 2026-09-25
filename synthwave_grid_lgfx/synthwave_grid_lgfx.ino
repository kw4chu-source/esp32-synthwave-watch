/*
 * ESP32 Synthwave Grid - wersja na LovyanGFX (zamiast TFT_eSPI)
 *
 * Powod migracji: TFT_eSPI jest uznawane przez spolecznosc za porzucone
 * (Bodmer/TFT_eSPI#3770), a jego pushImageDMA spowodowalo uszkodzenie
 * flash na tym sprzecie (znany problem - createSprite() nie gwarantuje
 * pamieci DMA-capable, dajac silnikowi DMA nieprawidlowy bufor zrodlowy).
 * LovyanGFX ma DMA jako jawna, pierwszorzedna opcje konfiguracji
 * (cfg.dma_channel), wiec powinien alokowac bufory poprawnie.
 *
 * Uklad ekranu i logika animacji identyczne jak w wersji TFT_eSPI
 * (sketches/synthwave_timer) - gorna 1/3 czarna (zegar z NTP),
 * dolna 2/3 animacja siatki, komponowana pasami po 64px w RAM.
 *
 * Pinout (ten sam co w TFT_eSPI/User_Setup.h - domyslny dla tego LCD):
 * CS15/DC2/RST4/MOSI23/SCLK18/MISO19
 */

// FS.h/LittleFS.h MUSZA byc wlaczone PRZED LovyanGFX.hpp - LovyanGFX
// wykrywa dostepnosc LittleFS po strazniku naglowka (_LITTLEFS_H_) w
// czasie preprocesowania siebie samego i dopiero wtedy kompiluje
// specjalizacje DataWrapperT<fs::LittleFSFS> potrzebna przez loadFont().
// Odwrotna kolejnosc daje blad kompilacji "abstract class" (brakujaca
// specjalizacja).
#include <FS.h>
#include <LittleFS.h>
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <WiFi.h>
#include <time.h>

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9488 _panel_instance;
  lgfx::Bus_SPI _bus_instance;

public:
  LGFX(void) {
    {
      auto cfg = _bus_instance.config();
      cfg.spi_host = VSPI_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 27000000;
      cfg.freq_read = 16000000;
      cfg.spi_3wire = false; // MISO fizycznie podlaczone (GPIO19)
      cfg.use_lock = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO; // <- to jest to, czego brakowalo w TFT_eSPI
      cfg.pin_sclk = 18;
      cfg.pin_mosi = 23;
      cfg.pin_miso = 19;
      cfg.pin_dc = 2;
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }
    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs = 15;
      cfg.pin_rst = 4;
      cfg.pin_busy = -1;
      cfg.panel_width = 320;
      cfg.panel_height = 480;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;
      cfg.readable = true;
      cfg.invert = false;
      cfg.rgb_order = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = true; // magistrala dzielona z (nieuzywanym tu) dotykiem/SD
      _panel_instance.config(cfg);
    }
    setPanel(&_panel_instance);
  }
};

LGFX lcd;
LGFX_Sprite bandSprite(&lcd);
LGFX_Sprite clockSprite(&lcd);

#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 320

// Gorna 1/3 ekranu zostaje czarna (zegar z NTP) - tutaj nic nie rysujemy.
const int zoneTop = SCREEN_HEIGHT / 3;          // 106
const int zoneHeight = SCREEN_HEIGHT - zoneTop; // 214

// Rozmiar sprite'a zegara - prawie cala wysokosc gornej strefy (3px
// marginesu od gory ekranu), dol sprite'a zakotwiczony dokladnie na
// linii horyzontu (patrz pushY w updateUptimeClock).
const int clockSpriteW = 400;
const int clockSpriteH = zoneTop - 3;

// Punkt zbiegu linii - ustawiony na SAM POCZATEK strefy animacji (0),
// zeby wizualny horyzont pokrywal sie dokladnie z zoneTop (granica z
// czarna strefa zegara). Wczesniej byl 15% w glab strefy, co zostawialo
// pusty odstep miedzy dolem zegara a miejscem zbiegu linii.
const int horizonY = 0;
const int roadSpacing = 20;

const int bandHeight = 64; // 480*64*2 = 61440 B - ta sama sprawdzona wielkosc co w TFT_eSPI

// WiFi + NTP - ta plytka (po hot-swapie z antena SMD) sama pobiera czas
// z internetu zamiast liczyc uptime. Polaczenie jest NIEBLOKUJACE, zeby
// animacja nie przycinala sie czekaniem na WiFi.
//
// Siec domowa (tvskmp_*) miala u tego urzadzenia problemy z polaczeniem
// (podejrzewane filtrowanie MAC na routerze ISP - patrz [[project-rf-relay-system]]),
// wiec docelowo uzywamy sieci testowej "esp32base" (SoftAP na tvboxie,
// 2.4GHz, otwarta - bez hasla), ktora jest juz potwierdzona jako dzialajaca
// z tym dokladnie sprzetem.
const char* wifiSsid1 = "esp32base";
const char* wifiSsid2 = "esp32base"; // jedna siec - failover na ta sama, nieszkodliwe
const unsigned long wifiRetryInterval = 15000;

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;      // UTC+1 (Polska, strefa bazowa)
const int daylightOffset_sec = 3600;  // +1h czasu letniego (wrzesien = nadal CEST)

uint8_t wifiSsidIndex = 0;
unsigned long lastWifiAttempt = 0;
bool ntpRequested = false;

struct GridLine {
  int16_t x1, y1, x2, y2;
  uint8_t alpha;
};

#define MAX_GRID_LINES 40
GridLine horizLines[MAX_GRID_LINES];
GridLine vertLines[MAX_GRID_LINES];
uint16_t horizLineCount = 0;
uint16_t vertLineCount = 0;

float scrollPos = 0.0f;
const float scrollSpeed = 54.0f;

uint32_t lastFrameMs = 0;
uint32_t frameCount = 0;
uint32_t fpsWindowStart = 0;

void preCalculateGrid() {
  int centerX = SCREEN_WIDTH / 2;

  horizLineCount = 0;
  for (int y = zoneHeight; y > horizonY && horizLineCount < MAX_GRID_LINES; y -= roadSpacing) {
    int depthFactor = (y - horizonY) * 256 / (zoneHeight - horizonY);
    // Podniesione minimum (bylo 39) i maksimum (bylo 218) - siatka byla
    // za blada, zwlaszcza blisko horyzontu gdzie stare min. dawalo ~15%
    // nasycenia koloru.
    uint8_t alpha = (150 + (depthFactor * 105) / 256) & 0xFF;

    horizLines[horizLineCount].x1 = 0;
    horizLines[horizLineCount].y1 = (int16_t)y;
    horizLines[horizLineCount].x2 = SCREEN_WIDTH;
    horizLines[horizLineCount].y2 = (int16_t)y;
    horizLines[horizLineCount].alpha = alpha;
    horizLineCount++;
  }

  vertLineCount = 0;
  const int numLanes = 6;

  for (int lane = -numLanes; lane <= numLanes; lane++) {
    if (lane == 0 || vertLineCount >= MAX_GRID_LINES) continue;

    int x1 = centerX + (lane * 15);
    int x2 = centerX + (lane * 130);

    vertLines[vertLineCount].x1 = (int16_t)x1;
    vertLines[vertLineCount].y1 = (int16_t)horizonY;
    vertLines[vertLineCount].x2 = (int16_t)x2;
    vertLines[vertLineCount].y2 = (int16_t)zoneHeight;
    vertLines[vertLineCount].alpha = 255; // pelne nasycenie - bylo 200, przygaszone
    vertLineCount++;
  }
}

inline uint16_t alphaBlend(uint16_t bg, uint16_t fg, uint8_t alpha) {
  uint16_t bgR = (bg >> 11) & 0x1F;
  uint16_t bgG = (bg >> 5) & 0x3F;
  uint16_t bgB = bg & 0x1F;

  uint16_t fgR = (fg >> 11) & 0x1F;
  uint16_t fgG = (fg >> 5) & 0x3F;
  uint16_t fgB = fg & 0x1F;

  uint16_t r = (bgR * (256 - alpha) + fgR * alpha) / 256;
  uint16_t g = (bgG * (256 - alpha) + fgG * alpha) / 256;
  uint16_t b = (bgB * (256 - alpha) + fgB * alpha) / 256;

  return (r << 11) | (g << 5) | b;
}

// Wspolny kolor pionowych linii siatki I obrysu zegara - jedno miejsce,
// zeby oba na pewno byly identyczne (user chcial zegar w kolorze linii
// animacji).
const uint16_t GRID_MAGENTA = lgfx::color565(255, 100, 200);

// Kolejnosc od gory strefy (horyzont) do dolu (blizej "kamery"):
// czarny przy horyzoncie, coraz jasniejszy fiolet w strone widza.
// Wspolna dla malowania tla ORAZ blendowania linii siatki - linie byly
// blendowane zawsze wzgledem czerni, mimo ze realne tlo w dolnych pasach
// jest jasniejsze, co dodatkowo je przygaszalo.
uint16_t backgroundColorAt(int localY) {
  static const uint16_t colors[4] = {
    lgfx::color565(0, 0, 0),
    lgfx::color565(15, 8, 30),
    lgfx::color565(30, 15, 55),
    lgfx::color565(45, 27, 78)
  };
  int segH = zoneHeight / 4;
  int idx = localY / segH;
  if (idx < 0) idx = 0;
  if (idx > 3) idx = 3;
  return colors[idx];
}

void drawBackgroundBand(int bandTop, int bandH) {
  int segH = zoneHeight / 4;
  int bandBottom = bandTop + bandH;

  for (int i = 0; i < 4; i++) {
    int segTop = i * segH;
    int segBottom = segTop + segH;
    int drawTop = segTop > bandTop ? segTop : bandTop;
    int drawBottom = segBottom < bandBottom ? segBottom : bandBottom;
    if (drawBottom <= drawTop) continue;
    bandSprite.fillRect(0, drawTop - bandTop, SCREEN_WIDTH, drawBottom - drawTop, backgroundColorAt(segTop));
  }
}

// Proba polaczenia z WiFi - nieblokujaca, wywolywana co klatke ale
// faktycznie dziala tylko raz na wifiRetryInterval. Failover na przemian
// miedzy dwiema siecami, tak jak w reszcie projektu.
void connectWifiStep() {
  if (WiFi.status() == WL_CONNECTED) return;

  unsigned long now = millis();
  if (now - lastWifiAttempt < wifiRetryInterval) return;
  lastWifiAttempt = now;

  // Rozlacz i odczekaj przed kolejnym WiFi.begin() - bez tego sterownik
  // ESP-IDF (jesli wciaz w trakcie poprzedniej proby) odrzuca nowa
  // konfiguracje bledem "sta is connecting, cannot set config".
  WiFi.disconnect(true);
  delay(50);

  const char* ssid = (wifiSsidIndex == 0) ? wifiSsid1 : wifiSsid2;
  Serial.printf("[WIFI] Probuje polaczyc z: %s (siec otwarta, bez hasla)\n", ssid);
  WiFi.begin(ssid); // brak hasla - siec bez zabezpieczen
  wifiSsidIndex = 1 - wifiSsidIndex;
}

// Zegar z czasem internetowym (godziny:minuty, NTP) - rysowany w gornej,
// zarezerwowanej strefie. Ta sama czcionka/obrys/pozycja co poprzednio,
// zmienilo sie tylko zrodlo tekstu (NTP zamiast millis()-owego uptime).
// Redraw tylko przy zmianie tekstu - bezpieczne, bo nic innego nie dotyka
// tej czesci ekranu co klatke (w przeciwienstwie do strefy siatki).
void updateInternetClock() {
  static char cachedTime[6] = "";
  static unsigned long lastCheck = 0;

  // Throttle do 1x/s - getLocalTime() nie musi byc odpytywane co klatke.
  if (millis() - lastCheck < 1000) return;
  lastCheck = millis();

  char timeStr[6] = "--:--";
  wl_status_t wifiStatus = WiFi.status();
  bool haveRealTime = false;

  if (wifiStatus == WL_CONNECTED) {
    if (!ntpRequested) {
      ntpRequested = true;
      configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
      Serial.println("[NTP] Zadanie synchronizacji czasu wyslane");
    }

    struct tm timeinfo;
    // tm_year > 120 (czyli rok > 2020) odrzuca "sukces" getLocalTime()
    // zwracajacy epoke (1 sty 1970) zanim NTP naprawde zdazylo zsynchronizowac -
    // bez tego zegar potrafi utknac na "00:00" mimo pozornego polaczenia.
    if (getLocalTime(&timeinfo, 50) && timeinfo.tm_year > 120) {
      strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
      haveRealTime = true;
    }
  }

  static unsigned long lastDiagPrint = 0;
  if (millis() - lastDiagPrint >= 5000) {
    lastDiagPrint = millis();
    Serial.printf("[WIFI] status=%d  [NTP] czas_ok=%d  [CLOCK] %s\n",
                  wifiStatus, haveRealTime, timeStr);
  }

  if (strcmp(timeStr, cachedTime) == 0) return;
  strcpy(cachedTime, timeStr);

  lcd.waitDMA();

  clockSprite.fillSprite(TFT_BLACK);

  // Czcionka (Orbitron90.vlw albo fallback Font7) zaladowana raz w setup() -
  // uzywana tu w skali natywnej (1.0), bez skalowania, zeby anti-aliasing
  // wgranej czcionki VLW nie zostal rozmyty przez skalowanie najblizszego
  // sasiada.
  clockSprite.setTextSize(1.0f);

  // bottom_left zamiast bottom_center/datum automatycznego centrowania -
  // wbudowane centrowanie w poziomie potrafilo zle liczyc srodek dla tej
  // czcionki (widoczne przesuniecie w prawo). Liczymy srodek recznie z
  // textWidth(), co jest przewidywalne i latwe do zdiagnozowania.
  clockSprite.setTextDatum(lgfx::bottom_left);

  int textW = clockSprite.textWidth(timeStr);
  int cx = (clockSpriteW - textW) / 2;
  int cy = clockSpriteH;

  Serial.printf("[CLOCK] fontH=%d textW=%d cx=%d (sprite %dx%d)\n",
                clockSprite.fontHeight(), textW, cx, clockSpriteW, clockSpriteH);

  // Sam obrys (bez wypelnienia) w kolorze pionowych linii siatki (magenta) -
  // ten sam trik co poprzednia poswiata: dylatacja KOLKIEM (dx*dx+dy*dy <=
  // r*r, nie kwadrat) daje gladki, rowny obrys zamiast postrzepionego.
  // Grubosc 2px - przy 3px na Font7 zamykaly sie waskie przerwy miedzy
  // segmentami cyfr (np. "0" zamienialo sie w wypelniona "pigulke");
  // Orbitron Bold 90px ma grubsze kreski wiec 2px jest bezpieczne i tu.
  const int outlineThickness = 2;
  clockSprite.setTextColor(GRID_MAGENTA);
  for (int dx = -outlineThickness; dx <= outlineThickness; dx++) {
    for (int dy = -outlineThickness; dy <= outlineThickness; dy++) {
      if (dx * dx + dy * dy > outlineThickness * outlineThickness) continue;
      clockSprite.drawString(timeStr, cx + dx, cy + dy);
    }
  }

  // Wycinamy srodek z powrotem do czerni (tla sprite'a) - zostaje sam obrys.
  clockSprite.setTextColor(TFT_BLACK);
  clockSprite.drawString(timeStr, cx, cy);

  // Dol sprite'a zakotwiczony dokladnie na linii horyzontu (zoneTop),
  // gora ma tylko 3px marginesu (wliczone w clockSpriteH).
  int pushX = (SCREEN_WIDTH - clockSpriteW) / 2;
  int pushY = zoneTop - clockSpriteH;
  clockSprite.pushSprite(pushX, pushY);
}

// Bufor jest jeden, wspoldzielony miedzy pasami - waitDMA() upewnia sie,
// ze poprzedni transfer skonczyl czytac z niego, zanim go nadpiszemy.
void renderAndPushBand(float offset, int bandTop, int bandH) {
  lcd.waitDMA();

  int bandBottom = bandTop + bandH;

  drawBackgroundBand(bandTop, bandH);

  // Swiecaca linia horyzontu na pelna szerokosc ekranu - tylko pas 0
  // dotyka gornej krawedzi strefy animacji (horizonY=0), wiec rysujemy
  // ja tylko tam. Odswieza sie co klatke razem z siatka.
  if (bandTop == 0) {
    bandSprite.drawFastHLine(0, 2, SCREEN_WIDTH, bandSprite.color565(60, 120, 180));
    bandSprite.drawFastHLine(0, 1, SCREEN_WIDTH, bandSprite.color565(180, 220, 255));
    bandSprite.drawFastHLine(0, 0, SCREEN_WIDTH, TFT_WHITE);
  }

  uint16_t blueGrid = bandSprite.color565(100, 100, 255);
  uint16_t magentaGrid = GRID_MAGENTA;

  int intOffset = ((int)offset) % roadSpacing;
  for (int i = 0; i < horizLineCount; i++) {
    int y = horizLines[i].y1 - intOffset;
    // y >= horizonY jest kluczowe - bez tego animowana linia potrafi
    // "wjechac" ponad horyzont przy zawijaniu offsetu (horyzont ma
    // zostac czysty, czarny, bez zadnych linii).
    if (y >= bandTop && y < bandBottom && y >= horizonY) {
      // Blendujemy wzgledem PRAWDZIWEGO tla w tym miejscu (nie zawsze
      // czerni) - w dolnych pasach tlo jest jasniejszym fioletem, wiec
      // linie tam byly przygaszone wzgledem zalozonej czarnej bazy.
      uint16_t lineColor = alphaBlend(backgroundColorAt(y), blueGrid, horizLines[i].alpha);
      bandSprite.drawLine(horizLines[i].x1, y - bandTop, horizLines[i].x2, y - bandTop, lineColor);
    }
  }

  int clipTop = horizonY > bandTop ? horizonY : bandTop;
  int clipBottom = zoneHeight < bandBottom ? zoneHeight : bandBottom;
  if (clipTop < clipBottom) {
    float span = (float)(zoneHeight - horizonY);
    float t1 = (clipTop - horizonY) / span;
    float t2 = (clipBottom - horizonY) / span;
    uint16_t vertBg = backgroundColorAt(clipTop);

    for (int i = 0; i < vertLineCount; i++) {
      float dx = vertLines[i].x2 - vertLines[i].x1;
      int16_t xAtTop = vertLines[i].x1 + (int16_t)(t1 * dx);
      int16_t xAtBottom = vertLines[i].x1 + (int16_t)(t2 * dx);

      uint16_t lineColor = alphaBlend(vertBg, magentaGrid, vertLines[i].alpha);
      bandSprite.drawLine(xAtTop, clipTop - bandTop, xAtBottom, clipBottom - bandTop, lineColor);
    }
  }

  // Brak osobnej funkcji "*DMA" - DMA jest uzywane automatycznie przez
  // pushSprite(), bo skonfigurowalismy dma_channel na magistrali.
  bandSprite.pushSprite(0, zoneTop + bandTop);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n[BOOT] ESP32 Synthwave Grid (LovyanGFX)");

  lcd.init();
  lcd.setRotation(1);
  lcd.fillScreen(TFT_BLACK);

  pinMode(32, OUTPUT);
  digitalWrite(32, HIGH); // podswietlenie na stale wlaczone (bez PWM)

  Serial.printf("[TFT] Screen: %dx%d, strefa animacji: %dx%d (od y=%d), pas: %dx%d\n",
                SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_WIDTH, zoneHeight, zoneTop, SCREEN_WIDTH, bandHeight);

  preCalculateGrid();
  Serial.printf("[GRID] Precomputed: %d horiz (animowane), %d vert (statyczne)\n",
                horizLineCount, vertLineCount);

  Serial.printf("[HEAP] Wolne: %u B, najwiekszy ciagly blok: %u B\n",
                ESP.getFreeHeap(), ESP.getMaxAllocHeap());

  bandSprite.setColorDepth(16);
  void* spriteBuf = bandSprite.createSprite(SCREEN_WIDTH, bandHeight);
  if (spriteBuf == nullptr) {
    Serial.println("[BLAD] Nie udalo sie zaalokowac sprite'a pasa (za malo RAM)!");
    pinMode(2, OUTPUT);
    while (true) {
      digitalWrite(2, HIGH);
      delay(100);
      digitalWrite(2, LOW);
      delay(100);
    }
  }
  Serial.printf("[SPRITE] Zaalokowano %d bajtow w RAM (jeden pas, reuzywany)\n", SCREEN_WIDTH * bandHeight * 2);

  clockSprite.setColorDepth(16);
  void* clockBuf = clockSprite.createSprite(clockSpriteW, clockSpriteH);
  if (clockBuf == nullptr) {
    Serial.println("[BLAD] Nie udalo sie zaalokowac sprite'a zegara (za malo RAM)!");
  } else {
      Serial.printf("[SPRITE] Zegar: %d bajtow w RAM\n", clockSpriteW * clockSpriteH * 2);
  }

  // Wlasna czcionka (Orbitron Bold 90px, anti-aliased VLW) wgrana osobno
  // do LittleFS (data/Orbitron90.vlw) - patrz [[project-cyberpunk-dashboard]].
  // Jesli montowanie/wczytanie sie nie uda (np. obraz LittleFS nie zostal
  // jeszcze wgrany), spadamy z powrotem na wbudowana Font7, zeby zegar
  // dalej dzialal zamiast pokazywac puste/losowe znaki.
  if (!LittleFS.begin()) {
    Serial.println("[FONT] BLAD: montowanie LittleFS nie powiodlo sie - fallback na Font7");
    clockSprite.setFont(&fonts::Font7);
  } else if (!clockSprite.loadFont(LittleFS, "/Orbitron90.vlw")) {
    Serial.println("[FONT] BLAD: nie udalo sie zaladowac /Orbitron90.vlw - fallback na Font7");
    clockSprite.setFont(&fonts::Font7);
  } else {
    Serial.println("[FONT] Zaladowano Orbitron90.vlw z LittleFS");
  }

  WiFi.mode(WIFI_STA);
  connectWifiStep(); // od razu pierwsza proba, bez czekania na wifiRetryInterval

  lastFrameMs = millis();
  fpsWindowStart = lastFrameMs;

  Serial.println("[READY] Animacja siatki wystartowala");
}

void loop() {
  uint32_t now = millis();
  uint32_t dt = now - lastFrameMs;
  lastFrameMs = now;

  scrollPos += (scrollSpeed * dt) / 1000.0f;

  connectWifiStep();
  updateInternetClock();

  for (int bandTop = 0; bandTop < zoneHeight; bandTop += bandHeight) {
    int thisBandH = bandHeight;
    if (bandTop + thisBandH > zoneHeight) thisBandH = zoneHeight - bandTop;
    renderAndPushBand(scrollPos, bandTop, thisBandH);
  }
  lcd.waitDMA();

  frameCount++;
  if (now - fpsWindowStart >= 1000) {
    Serial.printf("[FPS] %lu\n", frameCount);
    frameCount = 0;
    fpsWindowStart = now;
  }
}
