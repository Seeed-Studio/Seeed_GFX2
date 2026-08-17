/**
 * Panel: 2.9-inch ePaper - Monocolor 128x296
 * Demo:  Display an embedded bitmap image.
 *
 * This example was migrated from Seeed_GFX-master/examples/ePaper
 * and adapted from the TFT_eSPI/EPaper API to the Seeed_GFX v2 API.
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
    if (!display.begin<Board_XIAO_EPaper_Breakout, Config_XIAO_EPaper_2inch9_BW_SSD1680>()) {
        Serial.println(display.lastResult().message);
        return;
    }
    // Rotate the portrait asset into the glass's 296x128 landscape viewport.
    display.setRotation(3);
    display.fillScreen(TFT_WHITE);

    // Source is packed 1bpp MSB-first; rotate it clockwise while drawing.
    display.drawBitmapRotatedCW(0, 0, gImage_2inch9, 128, 296,
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
