/**
 * Panel: 7.5 inch monochrome ePaper (800x480, UC8179)
 * Board: XIAO ePaper Breakout Board (template API - no registered product for this board).
 */
#include <Seeed_GFX.h>
#include "board/boards/XIAO_ePaper_Boards.h"
#include "panel/configs/Seeed_Panel_Configs.h"
#include "driver/epaper/Driver_UC8179.h"
#include "panel/Panel_EPaper.h"

Seeed_GFX display;

void setup() {
    Serial.begin(115200);
    if (!display.begin<Board_XIAO_ePaper_Breakout, Config_Seeed_ePaper_7inch5_BW_UC8179>()) { Serial.println(display.lastResult().message); return; }

    display.fillScreen(TFT_WHITE);

    const int16_t w = display.width();   // 800
    const int16_t h = display.height();  // 480

    // Top banner
    display.fillRect(0, 0, w, 86, TFT_BLACK);
    display.setTextColor(TFT_WHITE);
    display.setTextSize(3);
    display.drawString("XIAO ePaper Breakout Board", 36, 28);

    // Subtitle
    display.setTextColor(TFT_BLACK);
    display.setTextSize(2);
    display.drawString("7.5 inch monochrome ePaper", 48, 135);
    display.setTextSize(1);
    display.drawString("800 x 480  |  UC8179  |  1 bpp", 96, 170);

    // Shape sampler
    display.drawCircle(120, 320, 55, TFT_BLACK);
    display.fillCircle(120, 320, 24, TFT_BLACK);
    display.drawRect(230, 265, 140, 110, TFT_BLACK);
    display.fillRect(270, 290, 60, 60, TFT_BLACK);
    display.drawTriangle(430, 265, 560, 265, 495, 385, TFT_BLACK);
    display.fillTriangle(430, 385, 560, 385, 495, 290, TFT_BLACK);
    for (int i = 0; i < 90; i += 9)
        display.drawLine(620 + i, 385, 620, 385 - i, TFT_BLACK);

    // Hello-world banner
    display.fillRect(16, h - 56, w - 32, 42, TFT_BLACK);
    display.setTextColor(TFT_WHITE);
    display.setTextSize(4);
    display.drawCentreString("Hello World", w / 2, h - 44, 1);

    const GfxResult refreshResult = display.refresh();
    if (!refreshResult) Serial.println(refreshResult.message);
}

void loop() { delay(1000); }
