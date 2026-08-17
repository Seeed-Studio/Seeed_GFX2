/**
 * Product: 1.47 inch LCD SPI Display (standalone, no touch, ST7789V3)
 * Display: 172x320 native controller RAM; official front-view presentation is
 *          320x172 landscape. The panel config supplies rotation 1.
 * Wiki: https://wiki.seeedstudio.com/1-47inch_lcd_spi_display/
 *
 * Wiki wiring: CS=D1, DC=D3, RST=D0, BL=D6, MOSI=D10, SCLK=D8.
 */

#include <Seeed_GFX.h>
#include "board/boards/XIAO_LCD_Board.h"
#include "driver/tft/Driver_ST7789.h"
#include "panel/Panel_TFT.h"

Seeed_GFX display;

void setup() {
    Serial.begin(115200);

    if (!display.begin<Board_Seeed_1inch47_LCD,
                       Config_Seeed_1inch47_LCD_ST7789>()) {
        Serial.println(display.lastResult().message);
        return;
    }

    const int16_t w = display.width();   // 320: config's landscape rotation
    const int16_t h = display.height();  // 172
    display.fillScreen(TFT_BLACK);
    display.drawRoundRect(3, 3, w - 6, h - 6, 10, TFT_WHITE);

    display.fillRoundRect(12, 14, w - 24, 42, 8, TFT_BLUE);
    display.setTextColor(TFT_WHITE);
    display.setTextSize(2);
    display.drawCentreString("1.47 inch LCD", w / 2, 20, 1);
    display.setTextSize(1);
    display.drawCentreString("ST7789V3 / 320 x 172", w / 2, 45, 1);

    const int16_t swatchY = 75;
    const int16_t swatchW = 74;
    const int16_t swatchH = 62;
    const uint16_t colors[] = {TFT_RED, TFT_GREEN, TFT_BLUE, TFT_YELLOW};
    for (uint8_t i = 0; i < 4; ++i) {
        const int16_t x = 11 + i * 78;
        display.fillRoundRect(x, swatchY, swatchW, swatchH, 7, colors[i]);
        display.drawRoundRect(x, swatchY, swatchW, swatchH, 7, TFT_WHITE);
    }

    display.setTextColor(TFT_WHITE);
    display.drawCentreString("Landscape", w / 2, 148, 1);
}

void loop() {}
