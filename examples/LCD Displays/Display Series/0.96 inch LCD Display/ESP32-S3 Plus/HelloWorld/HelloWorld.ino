/**
 * Product: XIAO 0.96 inch LCD Board (ST7789 80x160 IPS, no touch)
 * Target:  XIAO ESP32-S3 Plus. For nRF52840 Plus use the sibling nRF52840 Plus
 *          folder (RST=38, BL=37).
 * Wiring:  CS=D2, DC=D3, MOSI=D10, SCLK=D8; RST=D17(=13), BL=D18(=12).
 *
 * Verified on hardware: RST=13 (D17) + BL=12 (D18) light the panel. The actual
 * fix for "screen doesn't light up" was the dedicated 80x160 init sequence in
 * Driver_ST7789 (matching the downloaded Arduino_GFX firmware) — the generic
 * ST7789 init does NOT wake this 80x160 panel, even though it works for
 * 135x240/172x320. (The D17<->D19 BSP swap documented for some XIAO Plus
 * boards does NOT apply to this 0.96 ESP32-S3 Plus board — RST=D17 works.)
 * BGR, rotation 2, offset 24/0 and INVOFF match the downloaded dashboard.
 */

#include <Seeed_GFX.h>
#include "board/boards/XIAO_LCD_Board.h"
#include "driver/tft/Driver_ST7789.h"
#include "panel/Panel_TFT.h"

Seeed_GFX display;

static constexpr int8_t LCD_RST_PIN = 13;  // D17 on ESP32-S3 Plus (verified on hardware)
static constexpr int8_t LCD_BL_PIN = 12;   // D18 (not swapped)

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
    display.drawCentreString("ESP32-S3 Plus", 40, 90, 1);
}

void loop() {}
