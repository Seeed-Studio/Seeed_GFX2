/**
 * Panel: 2.13 inch BWRY ePaper (122x250 visible, 128x250 controller RAM)
 * Driver: Seeed-compatible JD79676 path
 * Native colors: Black / White / Red / Yellow
 * Board: EN05 (template API - no registered product for this board).
 */
#include <Seeed_GFX.h>
#include "board/boards/XIAO_ePaper_Boards.h"
#include "panel/configs/Seeed_Panel_Configs.h"
#include "driver/epaper/Driver_JD79676.h"
#include "panel/Panel_EPaper.h"

Seeed_GFX display;

void setup() {
    Serial.begin(115200);
    if (!display.begin<Board_XIAO_ePaper_EN05, Config_Seeed_ePaper_2inch13_BWRY_JD79676>()) { Serial.println(display.lastResult().message); return; }

    // Storage remains 128x250, while the drawing viewport is 122x250.
    // Rotation exposes the correct 250x122 visible landscape area.
    display.setRotation(3);

    display.fillScreen(TFT_WHITE);

    const int16_t w = display.width();   // 212
    const int16_t h = display.height();  // 104

    display.drawRect(2, 2, w - 4, h - 4, TFT_BLACK);

    display.setTextColor(TFT_BLACK);
    display.setTextSize(2);
    display.drawString("2.13 BWRY", 6, 4);
    display.setTextSize(1);
    display.drawRightString("EN05 / 4-color", w - 6, 8, 1);

    display.drawLine(6, 26, w - 6, 26, TFT_BLACK);

    const int16_t sw = 45, sh = 44, gap = 6;
    const int16_t sy = 32;
    const uint16_t colors[4] = { TFT_BLACK, TFT_WHITE, TFT_RED, TFT_YELLOW };
    for (uint8_t i = 0; i < 4; i++) {
        int16_t sx = 6 + i * (sw + gap);
        display.fillRect(sx, sy, sw, sh, colors[i]);
        display.drawRect(sx, sy, sw, sh, TFT_BLACK);
    }

    display.fillRect(6, h - 18, w - 12, 14, TFT_BLACK);
    display.setTextColor(TFT_YELLOW);
    display.setTextSize(1);
    display.drawCentreString("Hello World", w / 2, h - 16, 1);

    display.update();
}

void loop() {}
