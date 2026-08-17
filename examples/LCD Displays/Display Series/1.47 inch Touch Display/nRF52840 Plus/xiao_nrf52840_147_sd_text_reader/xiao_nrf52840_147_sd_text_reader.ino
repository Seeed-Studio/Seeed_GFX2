/**
 * Product: XIAO 1.47 inch Touch Display (JD9853A 172x320 + AXS5106L touch)
 * Display: JD9853A 172x320, BGR, capacitive touch
 * Target:  XIAO nRF52840 Plus (RST=38, BL=37). For ESP32-S3 Plus use the sibling
 *          ESP32-S3 Plus folder (RST=13, BL=12).
 * Wiring:  CS=D2, DC=D3, MOSI=D10, SCLK=D8; RST=38, BL=37. (Touch unused on this build.)
 * Demo:    Scans SD root for TXT/LOG/CSV and paginates each file as text on the 172x320 LCD.
 *
 * Ported from XIAO-Display-Board-main (xiao_nrf52840_147_sd_text_reader, TFT_eSPI). The Seeed_GFX
 * Board/Config templates replace the original bus+panel setup, manual MADCTL/invert/
 * swapbytes, and (for TFT_eSPI) the driver.h User_Setup. SdFat (SdSpiConfig(D6, SHARED_SPI,
 * freq, &SPI)) and the acquireForLcd/acquireForSd shared-SPI bus arbitration are retained
 * (LCD CS=D2 / SD CS=D6 -- Seeed_GFX does not manage SD CS).
 */

#include <Seeed_GFX.h>
#include "board/boards/XIAO_LCD_Board.h"
#include "driver/tft/Driver_JD9853A.h"
#include "panel/Panel_TFT.h"
#include <Adafruit_TinyUSB.h>
#include <SPI.h>
#include <SdFat.h>

Seeed_GFX display;

static constexpr int8_t  LCD_RST_PIN = 38;  // XIAO nRF52840 Plus
static constexpr int8_t  LCD_BL_PIN  = 37;
static constexpr uint8_t LCD_CS_PIN  = D2;
static constexpr uint8_t SD_CS_PIN   = D6;

static constexpr int LCD_W        = 172;
static constexpr int LCD_H        = 320;
static constexpr int MAX_FILES    = 24;
static constexpr int MAX_PATH_LEN = 64;
static constexpr int TEXT_COLUMNS = 27;
static constexpr int TEXT_LINES   = 24;

SdFat sdCard;

char textPaths[MAX_FILES][MAX_PATH_LEN];
int textCount = 0;
int textIndex = 0;

static void acquireForLcd() {
  // Bus_SPI owns LCD_CS (D2) and parks it HIGH; only SD_CS is idled here to keep
  // the SD card off the shared SPI bus before an LCD op.
  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH);
  delayMicroseconds(2);
}

static void acquireForSd() {
  // Bus_SPI parks LCD_CS HIGH after each LCD transaction; only SD_CS needs idling.
  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH);
  delayMicroseconds(2);
}

static void initLcd() {
  acquireForLcd();
  if (!display.begin<Board_XIAO_1inch47_Touch_Display<LCD_RST_PIN, LCD_BL_PIN>,
                    Config_XIAO_1inch47_Touch_JD9853A>()) {
    Serial.println(display.lastResult().message);
    return;
  }
  display.fillScreen(TFT_BLACK);
}

static void drawHeader(const char *title, uint16_t color) {
  acquireForLcd();
  display.fillScreen(TFT_BLACK);
  display.setTextSize(1);
  display.setTextColor(color, TFT_BLACK);
  display.setCursor(3, 3);
  String name = title;
  if (name.length() > 26) name = name.substring(0, 26);
  display.print(name);
  display.drawFastHLine(0, 17, LCD_W, color);
}

static bool beginSd() {
  acquireForSd();
  SPI.begin();
  const uint32_t freqs[] = {8000000, 4000000, 1000000, 400000};
  for (size_t i = 0; i < sizeof(freqs) / sizeof(freqs[0]); ++i) {
    SdSpiConfig cfg(SD_CS_PIN, SHARED_SPI, freqs[i], &SPI);
    if (sdCard.begin(cfg)) return true;
    delay(100);
  }
  return false;
}

static bool endsWithNoCase(const char *name, const char *ext) {
  size_t n = strlen(name);
  size_t e = strlen(ext);
  if (n < e) return false;
  for (size_t i = 0; i < e; ++i) {
    if (tolower(name[n - e + i]) != tolower(ext[i])) return false;
  }
  return true;
}

static bool isTextFile(const char *name) {
  return endsWithNoCase(name, ".txt") ||
         endsWithNoCase(name, ".log") ||
         endsWithNoCase(name, ".csv");
}

static void scanTextFiles() {
  textCount = 0;
  File32 root;
  if (!root.open("/")) return;

  File32 entry;
  while (textCount < MAX_FILES && entry.openNext(&root, O_RDONLY)) {
    if (!entry.isDir()) {
      char name[MAX_PATH_LEN];
      entry.getName(name, sizeof(name));
      if (isTextFile(name)) {
        snprintf(textPaths[textCount], MAX_PATH_LEN, "/%s", name);
        Serial.print("[TEXT] ");
        Serial.println(textPaths[textCount]);
        textCount++;
      }
    }
    entry.close();
  }
  root.close();
}

static void drawLine(int lineNumber, const char *text) {
  acquireForLcd();
  display.setTextSize(1);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setCursor(3, 23 + lineNumber * 12);
  display.print(text);
}

static void showTextFile(const char *path) {
  File32 f;
  if (!f.open(path, O_RDONLY)) {
    drawHeader(path, TFT_RED);
    drawLine(0, "Open failed");
    return;
  }

  uint32_t page = 1;
  bool done = false;
  while (!done) {
    drawHeader(path, TFT_CYAN);
    acquireForLcd();
    display.setTextColor(TFT_YELLOW, TFT_BLACK);
    display.setCursor(132, 3);
    display.print("P");
    display.print(page);

    char line[TEXT_COLUMNS + 1];
    int column = 0;
    int row = 0;

    while (row < TEXT_LINES) {
      int c = f.read();
      if (c < 0) {
        done = true;
        if (column > 0) {
          line[column] = 0;
          drawLine(row++, line);
        }
        break;
      }

      if (c == '\r') continue;
      if (c == '\n' || column >= TEXT_COLUMNS) {
        line[column] = 0;
        drawLine(row++, line);
        Serial.println(line);
        column = 0;
        if (c != '\n' && c >= 32 && c <= 126) line[column++] = (char)c;
      } else if (c == '\t') {
        do {
          line[column++] = ' ';
        } while (column < TEXT_COLUMNS && (column % 4) != 0);
      } else if (c >= 32 && c <= 126) {
        line[column++] = (char)c;
      }
    }

    if (!done) {
      delay(2500);
      page++;
    }
  }

  f.close();
  delay(1800);
}

void setup() {
  Serial.begin(115200);
  delay(800);
  initLcd();
  drawHeader("SD text reader", TFT_YELLOW);
  drawLine(0, "Mounting SD...");

  if (!beginSd()) {
    drawHeader("SD text reader", TFT_RED);
    drawLine(0, "SD mount failed");
    while (1) delay(1000);
  }

  scanTextFiles();
  if (textCount == 0) {
    drawHeader("SD text reader", TFT_YELLOW);
    drawLine(0, "No TXT/LOG/CSV found");
  }
}

void loop() {
  if (textCount == 0) {
    delay(1000);
    return;
  }

  showTextFile(textPaths[textIndex]);
  textIndex = (textIndex + 1) % textCount;
}
