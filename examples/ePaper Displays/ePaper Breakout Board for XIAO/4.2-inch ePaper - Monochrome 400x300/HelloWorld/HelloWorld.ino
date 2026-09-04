/**
 * Panel: 4.2 inch monochrome ePaper (400x300, SSD1683)
 * Board: XIAO ePaper Breakout Board (template API - no registered product for this board).
 */
#include <Seeed_GFX.h>
#include "board/boards/XIAO_ePaper_Boards.h"
#include "panel/configs/Seeed_Panel_Configs.h"
#include "driver/epaper/Driver_SSD1683.h"
#include "panel/Panel_EPaper.h"

Seeed_GFX display;

void setup() {
    Serial.begin(115200);
    if (!display.begin<Board_XIAO_ePaper_Breakout, Config_Seeed_ePaper_4inch2_BW_SSD1683>()) { Serial.println(display.lastResult().message); return; }

    display.fillScreen(TFT_WHITE);

    const int16_t w = display.width();   // 400
    const int16_t h = display.height();  // 300

    display.drawRect(3, 3, w - 6, h - 6, TFT_BLACK);

    display.setTextColor(TFT_BLACK);
    display.setTextSize(3);
    display.drawString("4.2 inch Monochrome", 10, 8);
    display.setTextSize(1);
    display.drawRightString("SSD1683", w - 10, 16, 1);

    display.drawLine(10, 46, w - 10, 46, TFT_BLACK);

    // Shape sampler
    display.drawCircle(80, 160, 45, TFT_BLACK);
    display.fillCircle(80, 160, 20, TFT_BLACK);
    display.drawRect(150, 115, 100, 90, TFT_BLACK);
    display.fillRect(175, 140, 50, 40, TFT_BLACK);
    display.drawTriangle(280, 115, 380, 115, 330, 210, TFT_BLACK);
    for (int i = 0; i < 60; i += 6)
        display.drawLine(280 + i, 230, 280, 230 - i, TFT_BLACK);

    // Hello-world banner
    display.fillRect(14, h - 54, w - 28, 42, TFT_BLACK);
    display.setTextColor(TFT_WHITE);
    display.setTextSize(3);
    display.drawCentreString("Hello World", w / 2, h - 40, 1);

    display.update();
}
void loop() {}
