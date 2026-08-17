/**
 * Product: ePaper Driver Board for Seeed Studio XIAO (EN05)
 * Panel:   2.13 inch monochrome ePaper, 122x250 (active), SSD1680
 * SKU:     104990850
 * Wiki:    https://wiki.seeedstudio.com/xiao_eink_expansion_board_v2/
 *
 * The 2.13" glass is physically landscape (250x122 long edge horizontal).
 * The SSD1680 RAM is portrait (128x250), so we rotate the frame buffer
 * into a 250x122 landscape viewport with setRotation(3). The font output
 * range (title textSize 2 @ (6,4), subtitle @ (w-6,8), divider y=26,
 * banner @ (6,h-18,w-12,14)) and shape sampler match the 2.13" BWRY color
 * HelloWorld and the 2.13" flex mono HelloWorld.
 */
#include <Seeed_GFX.h>
#include "board/boards/XIAO_EPaper_Boards.h"
#include "panel/configs/Seeed_Panel_Configs.h"
#include "driver/epaper/Driver_SSD1680.h"
#include "panel/Panel_EPaper.h"

Seeed_GFX display;

void setup() {
    Serial.begin(115200);
    if (!display.begin<Board_XIAO_EPaper_EN05, Config_XIAO_EPaper_2inch13_BW_SSD1680>()) {
        Serial.println(display.lastResult().message);
        return;
    }

    // Rotate into landscape so text reads along the long edge.
    display.setRotation(3);

    display.fillScreen(TFT_WHITE);

    const int16_t w = display.width();   // ~250 (active)
    const int16_t h = display.height();  // ~122 (active)

    // --- Outer frame ---
    display.drawRect(2, 2, w - 4, h - 4, TFT_BLACK);

    // --- Title (textSize 2 — same range as the 2.13" BWRY color layout) ---
    display.setTextColor(TFT_BLACK);
    display.setTextSize(2);
    display.drawString("2.13 inch BW", 6, 4);
    display.setTextSize(1);
    display.drawRightString("SSD1680", w - 6, 8, 1);

    // --- Divider (y=26 — same as the BWRY color layout) ---
    display.drawLine(6, 26, w - 6, 26, TFT_BLACK);

    // --- Shape sampler (same style/positions as the 2.13" flex mono) ---
    display.drawCircle(30, 52, 12, TFT_BLACK);
    display.fillCircle(30, 52, 5, TFT_BLACK);
    display.drawRect(54, 40, 32, 24, TFT_BLACK);
    display.fillRect(62, 46, 16, 12, TFT_BLACK);
    display.drawLine(96, 40, 128, 66, TFT_BLACK);
    display.drawLine(96, 66, 128, 40, TFT_BLACK);
    display.drawTriangle(140, 40, 170, 40, 155, 68, TFT_BLACK);
    display.fillRect(180, 44, 28, 22, TFT_BLACK);

    // --- Hello-world banner (same range as the BWRY color layout) ---
    display.fillRect(6, h - 18, w - 12, 14, TFT_BLACK);
    display.setTextColor(TFT_WHITE);
    display.setTextSize(1);
    display.drawCentreString("Hello World", w / 2, h - 16, 1);

    display.update();
}

void loop() { delay(1000); }
