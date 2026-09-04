/**
 * Panel: 4.26-inch ePaper - Monochrome 800x480
 * Demo:  Display an embedded bitmap image.
 *
 * This example was migrated from Seeed_GFX-master/examples/ePaper
 * and adapted from the TFT_eSPI/EPaper API to the Seeed_GFX v2 API.
 *
 */

#include <Seeed_GFX.h>
#include "board/boards/XIAO_ePaper_Boards.h"
#include "panel/configs/Seeed_Panel_Configs.h"
#include "driver/epaper/Driver_SSD1677.h"
#include "panel/Panel_EPaper.h"
#include "image.h"

Seeed_GFX display;

void setup() {
    Serial.begin(115200);
    if (!display.begin<Board_XIAO_ePaper_EN04, Config_Seeed_ePaper_4inch26_BW_SSD1677>()) {
        Serial.println(display.lastResult().message);
        return;
    }


    display.fillScreen(TFT_WHITE);

    // The source is packed 1bpp, MSB first. In library buffers 1 means black, so BLACK/WHITE is intentional.
    display.drawBitmap(0, 0, gImage_4inch26, 800, 480, TFT_BLACK, TFT_WHITE);
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
