/**
 * Panel: 2.13-inch ePaper - Monochrome 122x250
 * Demo:  Display an embedded bitmap image.
 *
 * This example was migrated from Seeed_GFX-master/examples/ePaper
 * and adapted from the TFT_eSPI/EPaper API to the Seeed_GFX v2 API.

 * 2.13-inch geometry note:
 *   - Official visible area: 122x250.
 *   - Controller storage row: 128 pixels.
 *   - The source stays 122 pixels wide; the library writes it with the correct
 *     source stride and keeps the six controller-only columns white.
 *
 */

#include <Seeed_GFX.h>
#include "board/boards/XIAO_EPaper_Boards.h"
#include "panel/configs/Seeed_Panel_Configs.h"
#include "driver/epaper/Driver_SSD1680.h"
#include "panel/Panel_EPaper.h"
#include "image.h"

Seeed_GFX display;

void setup() {
    Serial.begin(115200);
    if (!display.begin<Board_XIAO_EPaper_EE04, Config_XIAO_EPaper_2inch13_BW_SSD1680>()) {
        Serial.println(display.lastResult().message);
        return;
    }
    // Match Breakout's 250x122 landscape presentation. The portrait source
    // remains unchanged and is rotated in software for a like-for-like A/B.
    display.setRotation(3);
    display.fillScreen(TFT_WHITE);

    display.drawBitmapRotatedCW(0, 0, gImage_2inch13, 122, 250,
                                TFT_BLACK, TFT_WHITE);
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
