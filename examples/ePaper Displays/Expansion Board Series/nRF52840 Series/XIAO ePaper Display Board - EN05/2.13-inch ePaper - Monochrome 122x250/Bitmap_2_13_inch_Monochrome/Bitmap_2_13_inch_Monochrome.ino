/**
 * Panel: 2.13-inch ePaper - Monochrome 122x250
 * Board: EN05 (ePaper Driver Board for Seeed Studio XIAO nRF52840 v2)
 * Demo:  Display an embedded bitmap image.
 *
 * This example was adapted from the EE04 variant for the EN05 board.

 * 2.13-inch geometry note:
 *   - Official visible area: 122x250.
 *   - Controller storage row: 128 pixels.
 *   - The source stays 122 pixels wide; the library writes it with the correct
 *     source stride and keeps the six controller-only columns white.
 *
 */

#include <Seeed_GFX.h>
#include "board/boards/XIAO_ePaper_Boards.h"
#include "panel/configs/Seeed_Panel_Configs.h"
#include "driver/epaper/Driver_SSD1680.h"
#include "panel/Panel_EPaper.h"
#include "image.h"

Seeed_GFX display;

void setup() {
    Serial.begin(115200);
    if (!display.begin<Board_XIAO_ePaper_EN05, Config_Seeed_ePaper_2inch13_BW_SSD1680>()) {
        Serial.println(display.lastResult().message);
        return;
    }


    display.fillScreen(TFT_WHITE);

    // The source is packed 1bpp, MSB first. In library buffers 1 means black, so BLACK/WHITE is intentional.
    display.drawBitmap(0, 0, gImage_2inch13, 122, 250, TFT_BLACK, TFT_WHITE);
    const GfxResult refreshResult = display.refresh();
    if (!refreshResult) {
        Serial.println(refreshResult.message);
        return;
    }

    Serial.println("Bitmap displayed successfully");
}

void loop() {
    // nothing to do
}
