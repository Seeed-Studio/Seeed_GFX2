/**
 * Panel: 13.3-inch ePaper Displays
 * Demo:  Display an embedded bitmap image.
 *
 * This example was migrated from Seeed_GFX-master/examples/ePaper
 * and adapted from the TFT_eSPI/EPaper API to the Seeed_GFX v2 API.
 *
 */

#include <Seeed_GFX.h>
#include "board/boards/XIAO_EPaper_Boards.h"
#include "panel/configs/Seeed_Panel_Configs.h"
#include "driver/epaper/Driver_T133A01.h"
#include "panel/Panel_EPaper.h"
#include "image.h"

Seeed_GFX display;

void setup() {
    Serial.begin(115200);
    if (!display.begin<Board_XIAO_EPaper_EE02, Config_XIAO_EPaper_13inch3_Colorful_T133A01>()) {
        Serial.println(display.lastResult().message);
        return;
    }

    // The asset is landscape 1600x1200; the official native panel geometry is 1200x1600.
    display.setRotation(1);


    display.fillScreen(TFT_WHITE);

    // The original asset is packed indexed 4bpp (two pixels per byte), not RGB565.
    if (!display.pushImage4BPP(0, 0, 1600, 1200, gImage_13inch3, true)) {
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
