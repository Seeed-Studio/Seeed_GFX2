/**
 * Panel: 7.5 inch Tri-Color ePaper (Black / White / Red)
 * Board: Board_XIAO_EPaper_EN04 (template API - no registered product for this board).
 * NOTE: no tri-color panel config registered yet; this placeholder uses the
 * BW config, so the red plane is not driven. Replace with a real tri-color
 * config when available. The layout below is rendered in black & white.
 */
#include <Seeed_GFX.h>
#include "board/boards/XIAO_EPaper_Boards.h"
#include "panel/configs/Seeed_Panel_Configs.h"
#include "driver/epaper/Driver_UC8179.h"
#include "panel/Panel_EPaper.h"

Seeed_GFX display;

void setup() {
    Serial.begin(115200);
    if (!display.begin<Board_XIAO_EPaper_EN04, Config_XIAO_EPaper_7inch5_BW_UC8179>()) { Serial.println(display.lastResult().message); return; }

    display.fillScreen(TFT_WHITE);

    const int16_t w = display.width();   // 800
    const int16_t h = display.height();  // 480

    // Top banner
    display.fillRect(0, 0, w, 80, TFT_BLACK);
    display.setTextColor(TFT_WHITE);
    display.setTextSize(3);
    display.drawString("7.5 inch Tri-Color", 36, 26);

    // Subtitle
    display.setTextColor(TFT_BLACK);
    display.setTextSize(2);
    display.drawString("Black / White / Red", 48, 110);
    display.setTextSize(1);
    display.drawString("placeholder BW config - red plane not yet wired", 48, 140);

    display.drawLine(16, 168, w - 16, 168, TFT_BLACK);

    // Shape sampler
    display.drawCircle(110, 280, 55, TFT_BLACK);
    display.fillCircle(110, 280, 24, TFT_BLACK);
    display.drawRect(210, 225, 140, 110, TFT_BLACK);
    display.fillRect(250, 250, 60, 60, TFT_BLACK);
    display.drawTriangle(400, 225, 540, 225, 470, 345, TFT_BLACK);
    display.fillTriangle(400, 345, 540, 345, 470, 250, TFT_BLACK);
    for (int i = 0; i < 90; i += 9)
        display.drawLine(600 + i, 345, 600, 345 - i, TFT_BLACK);

    // Hello-world banner
    display.fillRect(16, h - 64, w - 32, 48, TFT_BLACK);
    display.setTextColor(TFT_WHITE);
    display.setTextSize(4);
    display.drawCentreString("Hello World", w / 2, h - 48, 1);

    display.update();
}

void loop() {}
