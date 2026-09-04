// XIAO ePaper Display Board (ESP32-S3) - EE04
// Panel: 7.3 inch full-color E Ink Spectra 6, 800x480

#include <Seeed_GFX.h>
#include "board/boards/XIAO_ePaper_Boards.h"
#include "driver/epaper/Driver_ED2208.h"
#include "panel/Panel_EPaper.h"

Seeed_GFX display;

void setup() {
    Serial.begin(115200);
    if (!display.begin<Board_XIAO_ePaper_EE04,
                   Config_Seeed_ePaper_7inch3_Colorful_ED2208>()) {
        Serial.println(display.lastResult().message);
        return;
    }
    display.fillScreen(TFT_WHITE);
    display.fillRect(0, 0, 800, 40, TFT_BLACK);
    display.setTextColor(TFT_WHITE);
    display.drawString("Seeed Studio", 20, 10);
    display.setTextColor(TFT_BLACK);
    display.drawString("7.3\" Spectra 6 ePaper", 20, 60);
    display.drawString("Template API", 20, 90);
    const GfxResult refreshResult = display.refresh();
    if (!refreshResult) Serial.println(refreshResult.message);
}

void loop() { delay(1000); }
