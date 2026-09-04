/**
 * Panel: 2.9 inch monochrome ePaper (128x296, SSD1680)
 * Board: Board_XIAO_ePaper_EN04 (template API - no registered product for this board)
 *
 * The 2.9" glass is physically landscape. The SSD1680 controller RAM is
 * natively portrait (128 x 296), so we rotate the frame buffer into a
 * 296 x 128 landscape viewport with setRotation(3) so text reads along the
 * long edge and the wide canvas fits the demo content without clipping.
 */
#include <Seeed_GFX.h>
#include "board/boards/XIAO_ePaper_Boards.h"
#include "panel/configs/Seeed_Panel_Configs.h"
#include "driver/epaper/Driver_SSD1680.h"
#include "panel/Panel_EPaper.h"

Seeed_GFX display;

void setup() {
    Serial.begin(115200);
    if (!display.begin<Board_XIAO_ePaper_EN04, Config_Seeed_ePaper_2inch9_BW_SSD1680>()) {
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
    display.drawString("2.9 inch Monochrome", 10, 6);
    display.setTextSize(1);
    display.drawRightString("EN04 / SSD1680", w - 8, 10, 1);

    // --- Divider under the title ---
    display.drawLine(8, 28, w - 8, 28, TFT_BLACK);

    // --- Shape sampler ---
    display.drawCircle(30, 60, 14, TFT_BLACK);
    display.fillCircle(30, 60, 6, TFT_BLACK);
    display.drawRect(58, 46, 44, 28, TFT_BLACK);
    display.fillRect(70, 54, 20, 12, TFT_BLACK);
    for (int i = 0; i < 5; i++)
        display.drawLine(118 + i * 6, 46, 142 + i * 6, 74, TFT_BLACK);
    display.drawTriangle(180, 74, 200, 46, 220, 74, TFT_BLACK);
    for (int i = 0; i < 24; i += 4)
        display.drawLine(232 + i, 46, 232, 46 + i, TFT_BLACK);

    // --- Hello-world banner ---
    display.fillRect(8, 92, w - 16, 26, TFT_BLACK);
    display.setTextColor(TFT_WHITE);
    display.setTextSize(2);
    display.drawCentreString("Hello World", w / 2, 97, 1);

    display.update();
}

void loop() {}
