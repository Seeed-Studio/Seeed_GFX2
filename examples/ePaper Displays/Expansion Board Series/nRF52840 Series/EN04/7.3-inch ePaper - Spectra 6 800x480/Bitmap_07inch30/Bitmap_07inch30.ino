/**
 * Panel: 7.3-inch ePaper - Spectra 6 800x480
 * Demo:  Display an embedded bitmap image.
 *
 * This example was migrated from Seeed_GFX-master/examples/ePaper
 * and adapted from the TFT_eSPI/EPaper API to the Seeed_GFX v2 API.
 *
 */

#include <Seeed_GFX.h>
#include "board/boards/XIAO_EPaper_Boards.h"
#include "panel/configs/Seeed_Panel_Configs.h"
#include "driver/epaper/Driver_ED2208.h"
#include "panel/Panel_EPaper.h"
#include "image.h"

Seeed_GFX display;

void setup() {
    Serial.begin(115200);
    if (!display.begin<Board_XIAO_EPaper_EN04, Config_XIAO_EPaper_7inch3_Colorful_ED2208>()) {
        Serial.println(display.lastResult().message);
        return;
    }


    display.fillScreen(TFT_WHITE);

    // The original asset is packed indexed 4bpp (two pixels per byte), not RGB565.
    if (!display.pushImage4BPP(0, 0, 800, 480, gImage_7inch3, true)) {
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
