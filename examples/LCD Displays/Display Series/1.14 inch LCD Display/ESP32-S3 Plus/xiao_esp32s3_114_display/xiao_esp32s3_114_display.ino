/**
 * Product: XIAO 1.14 inch LCD Board (ST7789 135x240 IPS, no touch)
 * Display: ST7789 135x240, RGB, no touch
 * Target:  XIAO ESP32-S3 Plus (RST=13, BL=12). For nRF52840 Plus use the sibling
 *          nRF52840 Plus folder (RST=38, BL=37).
 * Wiring:  CS=D2, DC=D3, MOSI=D10, SCLK=D8; RST=13, BL=12.
 * Demo:    Pure display demo: flashes full-screen colors, draws 8 color bars, then a labeled "Seeed_GFX / 1.14 Inch / ST7789" screen.
 *
 * Ported from XIAO-Display-Board-main (xiao_esp32s3_114_display.ino, TFT_eSPI). The Seeed_GFX
 * Board/Config templates replace the original bus+panel setup, manual MADCTL/invert/
 * swapbytes, and (for TFT_eSPI) the driver.h User_Setup. Pure GFX: drops driver.h,
 * tft.init()/setRotation(0)/invertDisplay(true); drawString+setTextDatum map 1:1.
 */

#include <Seeed_GFX.h>
#include "board/boards/XIAO_LCD_Board.h"
#include "driver/tft/Driver_ST7789.h"
#include "panel/Panel_TFT.h"

Seeed_GFX display;

static constexpr int8_t LCD_RST_PIN = 13;  // XIAO ESP32-S3 Plus
static constexpr int8_t LCD_BL_PIN = 12;

static void flashColors() {
  const uint16_t colors[] = {
    TFT_RED,
    TFT_GREEN,
    TFT_BLUE,
    TFT_WHITE,
    TFT_BLACK
  };

  for (uint8_t i = 0; i < 5; i++) {
    display.fillScreen(colors[i]);
    delay(450);
  }
}

static void drawColorBars() {
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

  const int w = display.width();
  const int h = display.height();
  const int barH = h / 8;

  for (uint8_t i = 0; i < 8; i++) {
    display.fillRect(0, i * barH, w, barH, colors[i]);
  }

  delay(1200);
}

static void drawFinalScreen() {
  const int w = display.width();
  const int h = display.height();

  display.fillScreen(TFT_BLACK);

  display.drawRoundRect(4, 4, w - 8, h - 8, 8, TFT_DARKGREY);
  display.drawRoundRect(8, 8, w - 16, h - 16, 6, TFT_BLUE);

  display.setTextDatum(MC_DATUM);

  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.drawString("Seeed_GFX", w / 2, 42, 2);

  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.drawString("1.14 Inch", w / 2, 92, 4);

  display.setTextColor(TFT_YELLOW, TFT_BLACK);
  display.drawString("XIAO ESP32-S3 Plus", w / 2, 150, 1);

  display.setTextColor(TFT_GREEN, TFT_BLACK);
  display.drawString("ST7789 Display", w / 2, 190, 2);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("Seeed_GFX 1.14 Inch Display LCD Demo");
  Serial.println("Board: XIAO ESP32-S3 Plus");

  if (!display.begin<Board_XIAO_1inch14_LCD<LCD_RST_PIN, LCD_BL_PIN>,
                     Config_Seeed_1inch14_LCD_ST7789>()) {
    Serial.println(display.lastResult().message);
    return;
  }

  Serial.print("LCD width: ");
  Serial.println(display.width());

  Serial.print("LCD height: ");
  Serial.println(display.height());

  flashColors();
  drawColorBars();
  drawFinalScreen();

  Serial.println("LCD demo finished.");
}

void loop() {
}
