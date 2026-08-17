/**
 * Product: XIAO 1.14 inch LCD Board (ST7789 135x240 IPS, no touch)
 * Target:  XIAO ESP32-S3 Plus (RST=13, BL=12). For nRF52840 Plus use the sibling
 *          nRF52840 Plus folder (RST=38, BL=37).
 * Wiring:  CS=D2, DC=D3, MOSI=D10, SCLK=D8; RST=13, BL=12.
 *
 * RGB, INVON and the 52/40 (53/40 inverted) offsets follow the downloaded
 * Arduino_GFX and TFT_eSPI reference examples.
 */

#include <Seeed_GFX.h>
#include "board/boards/XIAO_LCD_Board.h"
#include "driver/tft/Driver_ST7789.h"
#include "panel/Panel_TFT.h"

Seeed_GFX display;

static constexpr int8_t LCD_RST_PIN = 13;  // XIAO ESP32-S3 Plus
static constexpr int8_t LCD_BL_PIN = 12;

void setup() {
    Serial.begin(115200);

    if (!display.begin<Board_XIAO_1inch14_LCD<LCD_RST_PIN, LCD_BL_PIN>,
                       Config_Seeed_1inch14_LCD_ST7789>()) {
        Serial.println(display.lastResult().message);
        return;
    }

    display.fillScreen(TFT_BLACK);
    display.setTextColor(TFT_WHITE);
    display.setTextSize(2);
    display.drawCentreString("1.14\"", 67, 100, 1);
    display.setTextSize(1);
    display.drawCentreString("ESP32-S3 Plus", 67, 135, 1);
}

void loop() {}
