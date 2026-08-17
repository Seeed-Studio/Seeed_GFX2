/**
 * Panel: 10.3 Monochrome ePaper Display
 * Demo:  Display an embedded bitmap image.
 *
 * This example was migrated from Seeed_GFX-master/examples/ePaper
 * and adapted from the TFT_eSPI/EPaper API to the Seeed_GFX v2 API.
 *
 */

#include <Seeed_GFX.h>
#include "board/boards/XIAO_EPaper_Boards.h"
#include "panel/configs/Seeed_Panel_Configs.h"
#include "driver/epaper/Driver_ED103TC2.h"
#include "panel/Panel_EPaper.h"
#include "image.h"

Seeed_GFX display;

void setup() {
    Serial.begin(115200);
    if (!display.begin<Board_XIAO_EPaper_EE03, Config_XIAO_EPaper_10inch3_BW_ED103TC2>()) {
        Serial.println(display.lastResult().message);
        return;
    }


    display.fillScreen(TFT_WHITE);

    // The source is packed 1bpp, MSB first. In library buffers 1 means black, so BLACK/WHITE is intentional.
    display.drawBitmap(0, 0, gImage_10inch3, 1872, 1404, TFT_BLACK, TFT_WHITE);
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
