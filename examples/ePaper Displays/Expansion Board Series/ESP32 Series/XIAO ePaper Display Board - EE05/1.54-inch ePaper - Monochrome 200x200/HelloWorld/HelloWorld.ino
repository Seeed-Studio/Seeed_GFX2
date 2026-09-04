/**
 * Panel: 1.54 inch monochrome ePaper (200x200, SSD1681)
 * Board: EE05 (template API - no registered product for this board).
 */
#include <Seeed_GFX.h>
#include "board/boards/XIAO_ePaper_Boards.h"
#include "panel/configs/Seeed_Panel_Configs.h"
#include "driver/epaper/Driver_SSD1681.h"
#include "panel/Panel_EPaper.h"

Seeed_GFX display;

void setup() {
    Serial.begin(115200);
    if (!display.begin<Board_XIAO_ePaper_EE05, Config_Seeed_ePaper_1inch54_BW_SSD1681>()) { Serial.println(display.lastResult().message); return; }


    display.setRotation(3);

    display.fillScreen(TFT_WHITE);

    const int16_t w = display.width();   // 200
    const int16_t h = display.height();  // 200

    display.drawRect(3, 3, w - 6, h - 6, TFT_BLACK);

    display.setTextColor(TFT_BLACK);
    display.setTextSize(2);
    display.drawCentreString("1.54 inch BW", w / 2, 12, 1);
    display.setTextSize(1);
    display.drawCentreString("EE05 / SSD1681", w / 2, 32, 1);

    display.drawLine(12, 48, w - 12, 48, TFT_BLACK);

    display.drawCircle(48, 110, 22, TFT_BLACK);
    display.fillCircle(48, 110, 9, TFT_BLACK);
    display.drawRect(86, 88, 44, 44, TFT_BLACK);
    display.fillRect(98, 100, 20, 20, TFT_BLACK);
    display.drawTriangle(150, 88, 184, 88, 167, 132, TFT_BLACK);
    display.fillTriangle(150, 132, 184, 132, 167, 96, TFT_BLACK);

    display.fillRect(12, h - 34, w - 24, 24, TFT_BLACK);
    display.setTextColor(TFT_WHITE);
    display.setTextSize(2);
    display.drawCentreString("Hello World", w / 2, h - 30, 1);

    display.update();
}

void loop() {}
