/**
 * Panel: 7.3 inch full-color E Ink Spectra 6 (800x480, ED2208)
 * Board: Board_XIAO_ePaper_EN04 (template API - no registered product for this board).
 * Colors: Black / White / Red / Yellow / Blue / Green.
 */
#include <Seeed_GFX.h>
#include "board/boards/XIAO_ePaper_Boards.h"
#include "panel/configs/Seeed_Panel_Configs.h"
#include "driver/epaper/Driver_ED2208.h"
#include "panel/Panel_EPaper.h"

Seeed_GFX display;

void setup() {
    Serial.begin(115200);
    if (!display.begin<Board_XIAO_ePaper_EN04, Config_Seeed_ePaper_7inch3_Colorful_ED2208>()) { Serial.println(display.lastResult().message); return; }

    display.fillScreen(TFT_WHITE);

    const int16_t w = display.width();   // 800
    const int16_t h = display.height();  // 480

    // Top banner
    display.fillRect(0, 0, w, 82, TFT_BLACK);
    display.setTextColor(TFT_WHITE);
    display.setTextSize(3);
    display.drawString("XIAO ePaper Display Board - EN04", 36, 26);

    // Six-color palette swatches
    const uint16_t colors[] = { TFT_BLACK, TFT_RED, TFT_YELLOW,
                               TFT_GREEN, TFT_BLUE, TFT_WHITE };
    for (uint8_t i = 0; i < 6; ++i) {
        int16_t x = 46 + i * 120;
        display.fillRoundRect(x, 135, 96, 190, 10, colors[i]);
        display.drawRoundRect(x, 135, 96, 190, 10, TFT_BLACK);
    }

    display.setTextColor(TFT_BLACK);
    display.setTextSize(3);
    display.drawString("7.3 inch Spectra 6", 195, 380);

    const GfxResult refreshResult = display.refresh();
    if (!refreshResult) Serial.println(refreshResult.message);
}

void loop() {}
