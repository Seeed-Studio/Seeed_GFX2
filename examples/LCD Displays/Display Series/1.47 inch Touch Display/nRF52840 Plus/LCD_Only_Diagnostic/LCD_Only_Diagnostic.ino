/**
 * Product: XIAO 1.47 inch Touch Display (LCD-only diagnostic)
 *          JD9853A 172x320 + AXS5106L touch
 * Target:  XIAO nRF52840 Plus (RST=38, BL=37). For ESP32-S3 Plus use the sibling
 *          ESP32-S3 Plus folder (RST=13, BL=12).
 * Wiring:  CS=D2, DC=D3, MOSI=D10, SCLK=D8; RST=38, BL=37.
 *
 * This sketch checks only the LCD. The standalone no-touch 1.47" SPI module
 * is a different ST7789V3 product; use its separate product folder instead.
 */

#include <Seeed_GFX.h>
#include "board/boards/XIAO_LCD_Board.h"
#include "driver/tft/Driver_JD9853A.h"
#include "panel/Panel_TFT.h"

Seeed_GFX display;

static constexpr int8_t LCD_RST_PIN = 38;  // XIAO nRF52840 Plus
static constexpr int8_t LCD_BL_PIN = 37;

void setup() {
    Serial.begin(115200);

    if (!display.begin<
            Board_XIAO_1inch47_Touch_Display<LCD_RST_PIN, LCD_BL_PIN>,
                       Config_Seeed_1inch47_Touch_JD9853A>()) {
        Serial.println(display.lastResult().message);
        return;
    }

    display.fillScreen(TFT_BLACK);
    display.fillRoundRect(12, 20, 148, 90, 12, TFT_BLUE);
    display.setTextColor(TFT_WHITE);
    display.setTextSize(2);
    display.drawCentreString("1.47 inch", 86, 42, 1);
    display.drawCentreString("JD9853A", 86, 70, 1);
    display.fillRect(12, 135, 44, 150, TFT_RED);
    display.fillRect(64, 135, 44, 150, TFT_GREEN);
    display.fillRect(116, 135, 44, 150, TFT_BLUE);
}

void loop() {}
