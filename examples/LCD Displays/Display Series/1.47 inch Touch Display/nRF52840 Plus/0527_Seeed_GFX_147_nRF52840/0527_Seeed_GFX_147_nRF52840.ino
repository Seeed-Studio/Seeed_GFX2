/**
 * Product: XIAO 1.47 inch Touch Display (JD9853A 172x320 + AXS5106L touch)
 * Display: JD9853A 172x320, BGR, capacitive touch
 * Target:  XIAO nRF52840 Plus (RST=38, BL=37). For ESP32-S3 Plus use the sibling
 *          ESP32-S3 Plus folder (RST=13, BL=12).
 * Wiring:  CS=D2, DC=D3, MOSI=D10, SCLK=D8; RST=38, BL=37.
 * Demo:    Flash 5 fill colors, 8 color bars, final "Hello XIAO / 1.47 Touch
 *          Display / Powered By XIAO nRF52840 Plus" screen. No peripherals.
 *
 * Ported from XIAO-Display-Board-main (0527_Seeed_GFX_147_nRF52840, TFT_eSPI). The Seeed_GFX
 * Board/Config templates replace the original bus+panel setup, manual MADCTL/invert/
 * swapbytes, and (for TFT_eSPI) the driver.h User_Setup. Pure GFX; no peripherals.
 */

#include <Seeed_GFX.h>
#include "board/boards/XIAO_LCD_Board.h"
#include "driver/tft/Driver_JD9853A.h"
#include "panel/Panel_TFT.h"

Seeed_GFX display;

static constexpr int8_t LCD_RST_PIN = 38;  // XIAO nRF52840 Plus
static constexpr int8_t LCD_BL_PIN  = 37;

static void flashColors()
{
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

static void drawColorBars()
{
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

static void drawFinalScreen()
{
  const int w = display.width();
  const int h = display.height();

  display.fillScreen(TFT_BLACK);

  // Border
  display.drawRoundRect(4, 4, w - 8, h - 8, 10, TFT_DARKGREY);
  display.drawRoundRect(8, 8, w - 16, h - 16, 8, TFT_BLUE);

  display.setTextDatum(MC_DATUM);

  // Top label
  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.drawString("Seeed_GFX Test", w / 2, 42, 2);

  // Main title
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.drawString("Hello XIAO!", w / 2, 98, 4);

  // Product name
  display.setTextColor(TFT_YELLOW, TFT_BLACK);
  display.drawString("1.47 Inch", w / 2, 152, 2);
  display.drawString("Touch Display", w / 2, 178, 2);

  // Powered by
  display.setTextColor(TFT_GREEN, TFT_BLACK);
  display.drawString("Powered By", w / 2, 230, 2);
  display.drawString("XIAO nRF52840 Plus", w / 2, 256, 2);

  // Small footer line
  display.drawFastHLine(28, 292, w - 56, TFT_DARKGREY);
}

void setup()
{
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("Seeed_GFX 1.47 Inch Touch Display LCD Demo");
  Serial.println("Board: XIAO nRF52840 Plus");

  // The Board template drives RST+BL and the Config bakes rotation/MADCTL/invert,
  // so the manual preparePins()/forceBacklightOn()/hardResetPanel() +
  // tft.init()/setXIAO147Rotation()/applyXIAO147PanelFix() from the TFT_eSPI
  // source are all dropped.
  if (!display.begin<Board_XIAO_1inch47_Touch_Display<38, 37>, Config_XIAO_1inch47_Touch_JD9853A>()) {
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

void loop()
{
}
