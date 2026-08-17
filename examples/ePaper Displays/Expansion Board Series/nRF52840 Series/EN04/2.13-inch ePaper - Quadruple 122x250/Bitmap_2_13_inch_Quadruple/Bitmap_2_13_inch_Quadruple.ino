/**
 * Panel: 2.13-inch ePaper - Quadruple 122x250
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
#include "driver/epaper/Driver_JD79676.h"
#include "panel/Panel_EPaper.h"
#include "image.h"

Seeed_GFX display;

void setup() {
    Serial.begin(115200);
    if (!display.begin<Board_XIAO_EPaper_EN04, Config_XIAO_EPaper_2inch13_BWRY_JD79676>()) {
        Serial.println(display.lastResult().message);
        return;
    }


    display.fillScreen(TFT_WHITE);

    // The original asset is packed indexed 4bpp (two pixels per byte), not RGB565.
    if (!display.pushImage4BPP(0, 0, 122, 250, gImage_2inch13_BWRY, true)) {
        Serial.println("Packed 4bpp image rejected");
        return;
    }
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
