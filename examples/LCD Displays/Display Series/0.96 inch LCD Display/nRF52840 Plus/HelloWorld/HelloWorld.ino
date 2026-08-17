/**
 * Product: XIAO 0.96 inch LCD Board (ST7789 80x160 IPS, no touch)
 * Target:  XIAO nRF52840 Plus (RST=38, BL=37). For ESP32-S3 Plus use the sibling
 *          ESP32-S3 Plus folder (RST=13, BL=12).
 * Wiring:  CS=D2, DC=D3, MOSI=D10, SCLK=D8; RST=38, BL=37.
 *
 * BGR, rotation 2, offset 24/0 and INVOFF match the downloaded dashboard:
 * its RGB path documented red/blue swapping and its final IPS inversion call
 * resolves to the controller's INVOFF command.
 */

#include <Seeed_GFX.h>
#include "board/boards/XIAO_LCD_Board.h"
#include "driver/tft/Driver_ST7789.h"
#include "panel/Panel_TFT.h"

Seeed_GFX display;

static constexpr int8_t LCD_RST_PIN = 38;  // XIAO nRF52840 Plus
static constexpr int8_t LCD_BL_PIN = 37;

void setup() {
    Serial.begin(115200);

    if (!display.begin<Board_XIAO_0inch96_LCD<LCD_RST_PIN, LCD_BL_PIN>,
                       Config_Seeed_0inch96_LCD_ST7789>()) {
        Serial.println(display.lastResult().message);
        return;
    }

    display.fillScreen(TFT_BLACK);
    display.setTextColor(TFT_WHITE);
    display.setTextSize(2);
    display.drawCentreString("0.96\"", 40, 55, 1);
    display.setTextSize(1);
    display.drawCentreString("nRF52840 Plus", 40, 90, 1);
}

void loop() {}
