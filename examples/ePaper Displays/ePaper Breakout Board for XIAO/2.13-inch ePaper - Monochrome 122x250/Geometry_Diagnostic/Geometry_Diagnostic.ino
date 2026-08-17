/**
 * Breakout geometry diagnostic: 2.13 inch monochrome ePaper (SSD1680).
 *
 * Compare this full-refresh test with EE04 using the same panel. The Breakout
 * board applies its front-view mirror correction automatically; do not call
 * setHorizontalMirror() or setVerticalMirror() here.
 *
 * Report from the front of the glass:
 * - whether TOP/BTM or the one-pixel outer border is cut off;
 * - the number of missing/overflow tick rows at top and bottom;
 * - a photo showing all four corners.
 *
 * Do not use partial refresh: rotation 3 uses Breakout's native-Y correction,
 * which is intentionally supported on the full-refresh path only.
 */
#include <Seeed_GFX.h>
#include "board/boards/XIAO_EPaper_Boards.h"
#include "panel/configs/Seeed_Panel_Configs.h"
#include "driver/epaper/Driver_SSD1680.h"
#include "panel/Panel_EPaper.h"

Seeed_GFX display;

static void drawHorizontalRuler(int16_t y, int16_t w, int8_t tickDirection) {
    display.drawFastHLine(0, y, w, TFT_BLACK);
    for (int16_t x = 0; x < w; x += 10) {
        const int16_t tick = (x % 50 == 0) ? 5 : 3;
        display.drawFastVLine(x, y - (tickDirection < 0 ? tick - 1 : 0),
                              tick, TFT_BLACK);
    }
}

static void drawVerticalRuler(int16_t x, int16_t h, int8_t tickDirection) {
    display.drawFastVLine(x, 0, h, TFT_BLACK);
    for (int16_t y = 0; y < h; y += 10) {
        const int16_t tick = (y % 50 == 0) ? 5 : 3;
        display.drawFastHLine(x - (tickDirection < 0 ? tick - 1 : 0), y,
                              tick, TFT_BLACK);
    }
}

void setup() {
    Serial.begin(115200);
    if (!display.begin<Board_XIAO_EPaper_Breakout,
                       Config_XIAO_EPaper_2inch13_BW_SSD1680>()) {
        Serial.println(display.lastResult().message);
        return;
    }

    display.setRotation(3);
    const int16_t w = display.width();
    const int16_t h = display.height();

    display.fillScreen(TFT_WHITE);
    display.drawRect(0, 0, w, h, TFT_BLACK);
    display.drawRect(1, 1, w - 2, h - 2, TFT_BLACK);
    drawHorizontalRuler(0, w, 1);
    drawHorizontalRuler(h - 1, w, -1);
    drawVerticalRuler(0, h, 1);
    drawVerticalRuler(w - 1, h, -1);

    display.setTextColor(TFT_BLACK);
    display.setTextSize(1);
    display.drawString("TL", 7, 8);
    display.drawRightString("TR", w - 7, 8, 1);
    display.drawString("BL", 7, h - 17);
    display.drawRightString("BR", w - 7, h - 17, 1);
    display.drawCentreString("TOP: 0", w / 2, 8, 1);
    display.drawCentreString("BTM: H-1", w / 2, h - 17, 1);
    display.drawCentreString("BREAKOUT GEOMETRY", w / 2, h / 2 - 7, 1);
    display.drawCentreString("250 x 122 / ROT3", w / 2, h / 2 + 7, 1);
    display.drawFastHLine(w / 2 - 12, h / 2, 25, TFT_BLACK);
    display.drawFastVLine(w / 2, h / 2 - 12, 25, TFT_BLACK);
    display.update();
}

void loop() {}
