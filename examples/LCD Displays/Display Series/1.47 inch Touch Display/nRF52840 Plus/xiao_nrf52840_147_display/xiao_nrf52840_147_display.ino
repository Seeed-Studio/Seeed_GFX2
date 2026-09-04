/**
 * Product: XIAO 1.47 inch Touch Display (JD9853A 172x320 + AXS5106L touch)
 * Display: JD9853A 172x320, BGR, capacitive touch
 * Target:  XIAO nRF52840 Plus (RST=38, BL=37). For ESP32-S3 Plus use the sibling
 *          ESP32-S3 Plus folder (RST=13, BL=12).
 * Wiring:  CS=D2, DC=D3, MOSI=D10, SCLK=D8; RST=38, BL=37.
 * Demo:    TFT_eSPI LCD demo: flashes full-screen colors, draws 8 color bars, then a final text/geometry screen.
 *
 * Ported from XIAO-Display-Board-main (xiao_nrf52840_147_display, TFT_eSPI). The Seeed_GFX
 * Board/Config templates replace the original bus+panel setup, manual MADCTL/invert/
 * swapbytes, and (for TFT_eSPI) the driver.h User_Setup. DROP driver.h + tft.init()/
 * applyXIAO147PanelFix(MADCTL 0x48)/invertDisplay(false) — Driver_JD9853A/Config handle.
 * Keep <Adafruit_TinyUSB.h>. drawString+setTextDatum -> display equivalents 1:1.
 */

#include <Seeed_GFX.h>
#include "board/boards/XIAO_LCD_Board.h"
#include "driver/tft/Driver_JD9853A.h"
#include "panel/Panel_TFT.h"
#include <Adafruit_TinyUSB.h>

Seeed_GFX display;

static constexpr int8_t LCD_RST_PIN = 38;  // XIAO nRF52840 Plus
static constexpr int8_t LCD_BL_PIN  = 37;

static void flashColors() {
  // Full-screen colors make wiring/order problems obvious at a glance.
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
  // Draw several color bands to verify RGB color output.
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
  // Final static screen verifies text rendering, geometry, and panel bounds.
  const int w = display.width();
  const int h = display.height();

  display.fillScreen(TFT_BLACK);

  display.drawRoundRect(4, 4, w - 8, h - 8, 10, TFT_DARKGREY);
  display.drawRoundRect(8, 8, w - 16, h - 16, 8, TFT_BLUE);

  display.setTextDatum(MC_DATUM);

  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.drawString("Seeed_GFX Test", w / 2, 42, 2);

  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.drawString("Hello XIAO!", w / 2, 98, 4);

  display.setTextColor(TFT_YELLOW, TFT_BLACK);
  display.drawString("1.47 Inch", w / 2, 152, 2);
  display.drawString("Touch Display", w / 2, 178, 2);

  display.setTextColor(TFT_GREEN, TFT_BLACK);
  display.drawString("Powered By", w / 2, 230, 2);
  display.drawString("XIAO nRF52840 Plus", w / 2, 256, 2);

  display.drawFastHLine(28, 292, w - 56, TFT_DARKGREY);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("Seeed_GFX 1.47 Inch Touch Display LCD Demo");
  Serial.println("Board: XIAO nRF52840 Plus");

  // The Board template drives RST + BL and brings up the JD9853A panel with the
  // verified orientation/invert state baked into Config — no manual MADCTL, init,
  // rotation, or invertDisplay calls are needed.
  if (!display.begin<Board_XIAO_1inch47_Touch_Display<LCD_RST_PIN, LCD_BL_PIN>, Config_Seeed_1inch47_Touch_JD9853A>()) {
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
  // Static display demo: all drawing is done in setup().
}
