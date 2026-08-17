/**
 * Panel: 4.26 inch monochrome ePaper (800x480, SSD1677)
 * Board: XIAO ePaper Breakout Board (template API - no registered product for this board).
 * Horizontal mirror is applied automatically from the panel config (4.26" glass is mirrored).
 */
#include <Seeed_GFX.h>
#include "board/boards/XIAO_EPaper_Boards.h"
#include "panel/configs/Seeed_Panel_Configs.h"
#include "driver/epaper/Driver_SSD1677.h"
#include "panel/Panel_EPaper.h"

Seeed_GFX display;

void setup() {
    Serial.begin(115200);
    if (!display.begin<Board_XIAO_EPaper_Breakout, Config_XIAO_EPaper_4inch26_BW_SSD1677>()) { Serial.println(display.lastResult().message); return; }


    display.fillScreen(TFT_WHITE);

    const int16_t w = display.width();   // 800
    const int16_t h = display.height();  // 480

    // Top banner
    display.fillRect(0, 0, w, 86, TFT_BLACK);
    display.setTextColor(TFT_WHITE);
    display.setTextSize(3);
    display.drawString("XIAO ePaper Breakout Board", 48, 28);

    // Product info
    display.setTextColor(TFT_BLACK);
    display.setTextSize(2);
    display.drawString("4.26 inch monochrome ePaper", 60, 125);
    display.setTextSize(1);
    display.drawString("800 x 480  |  SSD1677  |  1 bpp", 96, 170);

    // Rounded rectangle frame
    display.drawRoundRect(48, 210, 704, 170, 12, TFT_BLACK);

    // Hello World in the center
    display.setTextSize(5);
    display.drawString("Hello World", 180, 260);

    // Bottom bar
    display.fillRect(0, h - 40, w, 40, TFT_BLACK);
    display.setTextColor(TFT_WHITE);
    display.setTextSize(1);
    display.drawString("Seeed Studio  |  seeedstudio.com", 48, h - 30);

    display.update();
}

void loop() { delay(1000); }
