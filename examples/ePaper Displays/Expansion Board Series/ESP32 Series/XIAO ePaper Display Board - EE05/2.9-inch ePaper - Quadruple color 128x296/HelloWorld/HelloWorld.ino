/**
 * Panel: 2.9 inch BWRY (128x296, JD79667) — Black / White / Red / Yellow
 * Board: Board_XIAO_ePaper_EE05 (template API - no registered product for this board).
 *
 * The 2.9" glass is physically landscape. The JD79667 controller RAM is
 * natively portrait (128 x 296), so we rotate the frame buffer into a
 * 296 x 128 landscape viewport with setRotation(3). The previous portrait
 * layout clipped the centered "2.9 inch BWRY" title (size-2 ~168 px wide did
 * not fit a 128 px width); landscape gives the wide canvas the demo needs.
 */
#include <Seeed_GFX.h>
#include "board/boards/XIAO_ePaper_Boards.h"
#include "panel/configs/Seeed_Panel_Configs.h"
#include "driver/epaper/Driver_JD79667.h"
#include "panel/Panel_EPaper.h"

Seeed_GFX display;

void setup() {
    Serial.begin(115200);
    if (!display.begin<Board_XIAO_ePaper_EE05, Config_Seeed_ePaper_2inch9_BWRY_JD79667>()) {
        Serial.println(display.lastResult().message);
        return;
    }

    // Rotate the portrait controller buffer into 296x128 landscape.
    display.setRotation(3);

    display.fillScreen(TFT_WHITE);

    const int16_t w = display.width();   // 296
    const int16_t h = display.height();  // 128

    // --- Outer frame ---
    display.drawRect(2, 2, w - 4, h - 4, TFT_BLACK);

    // --- Title row ---
    display.setTextColor(TFT_BLACK);
    display.setTextSize(2);
    display.drawString("2.9 inch BWRY", 10, 6);
    display.setTextSize(1);
    display.drawRightString("EE05 / 4-color", w - 8, 10, 1);

    // --- Divider ---
    display.drawLine(8, 28, w - 8, 28, TFT_BLACK);

    // --- Palette swatches: Black / White / Red / Yellow ---
    const int16_t sw = 50, sh = 48, gap = 8;
    const int16_t startX = (w - (sw * 4 + gap * 3)) / 2;  // 36
    const int16_t sy = 34;
    const uint16_t colors[4] = { TFT_BLACK, TFT_WHITE, TFT_RED, TFT_YELLOW };
    const char* names[4] = { "BLACK", "WHITE", "RED", "YELLOW" };
    for (int i = 0; i < 4; i++) {
        int16_t sx = startX + i * (sw + gap);
        display.fillRect(sx, sy, sw, sh, colors[i]);
        display.drawRect(sx, sy, sw, sh, TFT_BLACK);
        display.setTextColor(TFT_BLACK);
        display.setTextSize(1);
        display.drawCentreString(names[i], sx + sw / 2, sy + sh + 2, 1);
    }

    // --- Hello-world banner (yellow text on black) ---
    display.fillRect(8, h - 22, w - 16, 18, TFT_BLACK);
    display.setTextColor(TFT_YELLOW);
    display.setTextSize(2);
    display.drawCentreString("Hello World", w / 2, h - 19, 1);

    display.update();
}

void loop() {}
