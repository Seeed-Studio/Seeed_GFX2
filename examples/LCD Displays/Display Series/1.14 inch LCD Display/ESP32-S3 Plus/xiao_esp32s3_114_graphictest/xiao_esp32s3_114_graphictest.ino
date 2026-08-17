/**
 * Product: XIAO 1.14 inch LCD Board (ST7789 135x240 IPS, no touch)
 * Display: ST7789 135x240, RGB, no touch
 * Target:  XIAO ESP32-S3 Plus (RST=13, BL=12). For nRF52840 Plus use the sibling
 *          nRF52840 Plus folder (RST=38, BL=37).
 * Wiring:  CS=D2, DC=D3, MOSI=D10, SCLK=D8; RST=13, BL=12.
 * Demo:    Compact graphics benchmark: color bars, lines, rects, circles, triangles, round-rects, text, pixel gradient — timing each.
 *
 * Ported from XIAO-Display-Board-main (xiao_esp32s3_114_graphictest, TFT_eSPI). The Seeed_GFX
 * Board/Config templates replace the original bus+panel setup, manual MADCTL/invert/
 * swapbytes, and (for TFT_eSPI) the driver.h User_Setup. Pure GFX: drops driver.h and the
 * manual init helpers (preparePins/forceBacklightOn/hardResetPanel/initDisplay) plus
 * tft.init()/setRotation(0)/invertDisplay(true). drawString/setTextDatum/setCursor/print/
 * setTextColor carry over 1:1; tft.color565 -> display.color565.
 */

#include <Seeed_GFX.h>
#include "board/boards/XIAO_LCD_Board.h"
#include "driver/tft/Driver_ST7789.h"
#include "panel/Panel_TFT.h"

Seeed_GFX display;

static constexpr int8_t LCD_RST_PIN = 13;  // XIAO ESP32-S3 Plus
static constexpr int8_t LCD_BL_PIN = 12;

static void showTitle(const char *title) {
  display.fillScreen(TFT_BLACK);
  display.setTextDatum(MC_DATUM);
  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.drawString(title, display.width() / 2, 92, 2);
  display.setTextColor(TFT_DARKGREY, TFT_BLACK);
  display.drawString("XIAO ESP32-S3 Plus", display.width() / 2, 118, 1);
  delay(650);
}

static uint16_t colorWheel(uint8_t pos) {
  pos = 255 - pos;
  if (pos < 85) {
    return display.color565(255 - pos * 3, 0, pos * 3);
  }
  if (pos < 170) {
    pos -= 85;
    return display.color565(0, pos * 3, 255 - pos * 3);
  }
  pos -= 170;
  return display.color565(pos * 3, 255 - pos * 3, 0);
}

static unsigned long testColorBars() {
  unsigned long start = micros();
  const uint16_t colors[] = {
    TFT_RED,
    TFT_GREEN,
    TFT_BLUE,
    TFT_CYAN,
    TFT_MAGENTA,
    TFT_YELLOW,
    TFT_WHITE,
    TFT_BLACK
  };
  int barH = display.height() / 8;
  for (int i = 0; i < 8; ++i) {
    display.fillRect(0, i * barH, display.width(), barH, colors[i]);
  }
  return micros() - start;
}

static unsigned long testLines(uint16_t color) {
  unsigned long start = micros();
  int w = display.width();
  int h = display.height();
  display.fillScreen(TFT_BLACK);

  for (int x = 0; x < w; x += 8) display.drawLine(0, 0, x, h - 1, color);
  for (int y = 0; y < h; y += 8) display.drawLine(0, 0, w - 1, y, color);
  for (int x = 0; x < w; x += 8) display.drawLine(w - 1, h - 1, x, 0, TFT_YELLOW);
  for (int y = 0; y < h; y += 8) display.drawLine(w - 1, h - 1, 0, y, TFT_YELLOW);

  return micros() - start;
}

static unsigned long testFastLines() {
  unsigned long start = micros();
  display.fillScreen(TFT_BLACK);
  for (int y = 0; y < display.height(); y += 5) {
    display.drawFastHLine(0, y, display.width(), TFT_RED);
  }
  for (int x = 0; x < display.width(); x += 5) {
    display.drawFastVLine(x, 0, display.height(), TFT_BLUE);
  }
  return micros() - start;
}

static unsigned long testRects() {
  unsigned long start = micros();
  display.fillScreen(TFT_BLACK);
  int cx = display.width() / 2;
  int cy = display.height() / 2;
  int maxSize = min(display.width(), display.height());

  for (int size = maxSize; size > 8; size -= 10) {
    int x = cx - size / 2;
    int y = cy - size / 2;
    display.drawRect(x, y, size, size, colorWheel(size * 2));
  }
  return micros() - start;
}

static unsigned long testFilledRects() {
  unsigned long start = micros();
  display.fillScreen(TFT_BLACK);
  int cx = display.width() / 2;
  int cy = display.height() / 2;
  int maxSize = min(display.width(), display.height());

  for (int size = maxSize; size > 8; size -= 12) {
    int x = cx - size / 2;
    int y = cy - size / 2;
    display.fillRect(x, y, size, size, colorWheel(size * 3));
    display.drawRect(x, y, size, size, TFT_WHITE);
  }
  return micros() - start;
}

static unsigned long testCircles() {
  unsigned long start = micros();
  display.fillScreen(TFT_BLACK);
  int maxR = min(display.width(), display.height()) / 2 - 8;
  for (int r = 8; r <= maxR; r += 8) {
    display.drawCircle(display.width() / 2, display.height() / 2, r, colorWheel(r * 2));
  }
  for (int y = 24; y < display.height(); y += 48) {
    for (int x = 20; x < display.width(); x += 44) {
      display.fillCircle(x, y, 10, colorWheel(x + y));
    }
  }
  return micros() - start;
}

static unsigned long testTriangles() {
  unsigned long start = micros();
  display.fillScreen(TFT_BLACK);
  int cx = display.width() / 2;
  int cy = display.height() / 2;
  int maxTri = min(display.width(), display.height()) / 2 - 4;

  for (int i = 0; i <= maxTri; i += 10) {
    display.drawTriangle(
      cx, cy - i,
      cx - i, cy + i,
      cx + i, cy + i,
      colorWheel(i * 3)
    );
  }
  for (int i = maxTri; i > 0; i -= 14) {
    display.fillTriangle(
      cx, cy - i,
      cx - i, cy + i,
      cx + i, cy + i,
      colorWheel(200 - i)
    );
  }
  return micros() - start;
}

static unsigned long testRoundRects() {
  unsigned long start = micros();
  display.fillScreen(TFT_BLACK);
  for (int i = 0; i < 70; i += 8) {
    display.drawRoundRect(
      4 + i / 2,
      8 + i,
      display.width() - 8 - i,
      display.height() - 16 - i * 2,
      8,
      colorWheel(i * 4)
    );
  }
  return micros() - start;
}

static unsigned long testText() {
  unsigned long start = micros();
  display.fillScreen(TFT_BLACK);
  display.setTextDatum(TL_DATUM);

  display.setTextColor(TFT_GREEN, TFT_BLACK);
  display.drawString("Graphic", 8, 14, 4);

  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.drawString("135 x 240 TFT", 10, 62, 2);

  display.setTextColor(TFT_YELLOW, TFT_BLACK);
  display.drawString("Seeed_GFX", 10, 88, 2);

  display.setTextColor(TFT_WHITE, TFT_BLACK);
  for (int i = 0; i < 7; ++i) {
    display.setCursor(10, 122 + i * 16);
    display.print("Line ");
    display.print(i + 1);
    display.print(" 0x");
    display.print(0x1000 + i * 137, HEX);
  }
  return micros() - start;
}

static unsigned long testGradient() {
  unsigned long start = micros();
  int w = display.width();
  int h = display.height();
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      uint8_t r = map(x, 0, w - 1, 0, 255);
      uint8_t g = map(y, 0, h - 1, 0, 255);
      uint8_t b = (x + y) & 0xFF;
      display.drawPixel(x, y, display.color565(r, g, b));
    }
  }
  return micros() - start;
}

static void printResult(const char *name, unsigned long us) {
  Serial.print(name);
  Serial.print(": ");
  Serial.print(us / 1000.0f, 2);
  Serial.println(" ms");
}

static void showResultScreen() {
  display.fillScreen(TFT_BLACK);
  display.setTextDatum(MC_DATUM);
  display.setTextColor(TFT_GREEN, TFT_BLACK);
  display.drawString("Graphic", display.width() / 2, 78, 4);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.drawString("Finished", display.width() / 2, 126, 4);
  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.drawString("Reset to rerun", display.width() / 2, 178, 2);
  display.drawRoundRect(8, 8, display.width() - 16, display.height() - 16, 8, TFT_BLUE);
}

void setup() {
  Serial.begin(115200);
  delay(800);

  Serial.println();
  Serial.println("=== XIAO ESP32-S3 Plus 1.14 graphic test ===");

  if (!display.begin<Board_XIAO_1inch14_LCD<LCD_RST_PIN, LCD_BL_PIN>,
                   Config_Seeed_1inch14_LCD_ST7789>()) {
    Serial.println(display.lastResult().message);
    return;
  }

  Serial.print("LCD width: ");
  Serial.println(display.width());
  Serial.print("LCD height: ");
  Serial.println(display.height());

  showTitle("Graphic Test");

  unsigned long us;

  us = testColorBars();
  printResult("Color bars", us);
  delay(900);

  us = testLines(TFT_CYAN);
  printResult("Lines", us);
  delay(900);

  us = testFastLines();
  printResult("Fast lines", us);
  delay(900);

  us = testRects();
  printResult("Rectangles", us);
  delay(900);

  us = testFilledRects();
  printResult("Filled rectangles", us);
  delay(900);

  us = testCircles();
  printResult("Circles", us);
  delay(900);

  us = testTriangles();
  printResult("Triangles", us);
  delay(900);

  us = testRoundRects();
  printResult("Round rectangles", us);
  delay(900);

  us = testText();
  printResult("Text", us);
  delay(1100);

  us = testGradient();
  printResult("Pixel gradient", us);
  delay(1200);

  showResultScreen();
  Serial.println("Graphic test finished.");
}

void loop() {
  delay(1000);
}
