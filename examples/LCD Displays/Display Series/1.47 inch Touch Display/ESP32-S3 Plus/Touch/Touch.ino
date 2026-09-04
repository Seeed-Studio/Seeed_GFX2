/**
 * Product: XIAO 1.47 inch Touch Display
 *          JD9853A 172x320 + AXS5106L capacitive touch (I2C address 0x63)
 * Target:  XIAO ESP32-S3 Plus (RST=13, BL=12). For nRF52840 Plus use the sibling
 *          nRF52840 Plus folder (RST=38, BL=37).
 * Wiring:  LCD CS=D2, DC=D3, MOSI=D10, SCLK=D8; RST=13, BL=12.
 *          Touch SDA=D4, SCL=D5, INT=D7; RST shares LCD RST.
 *
 * display.begin() resets the shared LCD/touch RST line and initializes the
 * LCD. Therefore the touch object uses RST=-1. Pulling that line low again
 * after LCD initialization would reset the LCD and leave the screen black.
 */

#include <Seeed_GFX.h>
#include "board/boards/XIAO_LCD_Board.h"
#include "driver/tft/Driver_JD9853A.h"
#include "panel/Panel_TFT.h"
#include "touch/Touch_AXS5106L.h"

Seeed_GFX display;

static constexpr int8_t LCD_RST_PIN = 13;  // XIAO ESP32-S3 Plus
static constexpr int8_t LCD_BL_PIN = 12;

// The shared reset has already been handled by the display board.
Touch_AXS5106L touch(-1, D7, Wire, 172, 320);

void setup() {
    Serial.begin(115200);

    if (!display.begin<
            Board_XIAO_1inch47_Touch_Display<LCD_RST_PIN, LCD_BL_PIN>,
                       Config_Seeed_1inch47_Touch_JD9853A>()) {
        Serial.println(display.lastResult().message);
        return;
    }

    // touch.begin() now starts Wire and configures IRQ without resetting LCD.
    if (!display.attachTouch(touch, display.panel().driver().bus())) {
        Serial.println(display.lastResult().message);
        return;
    }

    display.fillScreen(TFT_BLACK);
    display.setTextColor(TFT_WHITE);
    display.setTextSize(2);
    display.drawCentreString("Touch me", 86, 150, 1);
    display.setTextSize(1);
    display.drawCentreString("AXS5106L I2C 0x63", 86, 180, 1);
}

void loop() {
    int32_t x = 0;
    int32_t y = 0;
    if (display.getTouch(&x, &y)) {
        display.fillCircle(static_cast<int16_t>(x), static_cast<int16_t>(y),
                           3, TFT_RED);
    }
}
