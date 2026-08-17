/**
 * Same-panel A/B quality diagnostic: 2.13-inch SSD1680 on XIAO Breakout.
 *
 * Use this sketch and the EE04 counterpart with the SAME panel, ambient
 * temperature, light, and normal full refresh. Photograph both from the front
 * after the image settles. Do not use partial or fast refresh.
 */
#include <Seeed_GFX.h>
#include "board/boards/XIAO_EPaper_Boards.h"
#include "panel/configs/Seeed_Panel_Configs.h"
#include "driver/epaper/Driver_SSD1680.h"
#include "panel/Panel_EPaper.h"

Seeed_GFX display;

static void drawQualityPattern(const char* boardName) {
    const int16_t w = display.width();
    const int16_t h = display.height();
    display.fillScreen(TFT_WHITE);
    display.drawRect(0, 0, w, h, TFT_BLACK);
    display.drawRect(2, 2, w - 4, h - 4, TFT_BLACK);

    display.setTextColor(TFT_BLACK);
    display.setTextSize(1);
    display.drawString("SSD1680 A/B QUALITY", 8, 8);
    display.drawRightString(boardName, w - 8, 8, 1);
    display.drawFastHLine(8, 21, w - 16, TFT_BLACK);

    display.setTextSize(1);
    display.drawString("AaBbCc 0123456789", 8, 30);
    display.setTextSize(2);
    display.drawString("Aa 12", 8, 43);
    display.setTextSize(1);
    display.drawString("thin: IIII llll 1111", 8, 66);
    display.drawString("diag: / / / / /", 8, 78);

    for (int16_t x = 0; x < 64; ++x) {
        display.drawPixel(150 + x, 30 + (x & 1), TFT_BLACK);
        display.drawPixel(150 + x, 40 + ((x >> 1) & 1), TFT_BLACK);
    }
    for (int16_t y = 0; y < 32; ++y) {
        for (int16_t x = 0; x < 64; ++x) {
            if (((x + y) & 1) == 0) display.drawPixel(150 + x, 55 + y, TFT_BLACK);
        }
    }
    display.drawLine(226, 30, w - 12, 90, TFT_BLACK);
    display.drawLine(226, 90, w - 12, 30, TFT_BLACK);
    display.drawCentreString("250x122 ROT3 FULL", w / 2, h - 17, 1);
}

void setup() {
    Serial.begin(115200);
    if (!display.begin<Board_XIAO_EPaper_Breakout,
                       Config_XIAO_EPaper_2inch13_BW_SSD1680>()) {
        Serial.println(display.lastResult().message);
        return;
    }
    display.setRotation(3);
    Serial.printf("board=Breakout driver=%s w=%u h=%u rot=%u\n",
                  display.driverPtr()->name(), display.width(), display.height(),
                  display.getRotation());
    drawQualityPattern("BREAKOUT");
    const GfxResult result = display.refresh();
    Serial.printf("refresh=%s\n", result.message);
}

void loop() {}
