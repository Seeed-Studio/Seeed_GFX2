/**
 * Product: 1.69 inch LCD Display Module
 * Display: 240x280 native ST7789V2 RAM; official presentation is 280x240
 *          landscape and is selected by the panel config.
 * Pin map (standard XIAO standalone LCD wiring):
 *   CS=D1, DC=D3, RST=D0, BL=D6, MOSI=D10, SCLK=D8
 *
 * NOTE: This is a generic SPI module. If your wiring differs, use the
 * template API with a custom board.
 */

#include <Seeed_GFX.h>

Seeed_GFX display(Seeed_Product::Seeed_LCD_1INCH69);

void setup() {
    Serial.begin(115200);
    if (!display.begin()) {
        Serial.println(display.lastResult().message);
        return;
    }

    const int16_t w = display.width();   // 280: config's landscape rotation
    const int16_t h = display.height();  // 240
    display.fillScreen(TFT_BLACK);
    display.drawRoundRect(3, 3, w - 6, h - 6, 10, TFT_WHITE);

    display.fillRoundRect(12, 14, w - 24, 46, 8, TFT_WHITE);
    display.setTextColor(TFT_BLACK);
    display.setTextSize(2);
    display.drawCentreString("1.69 inch LCD", w / 2, 21, 1);
    display.setTextSize(1);
    display.drawCentreString("ST7789V2 / 280 x 240", w / 2, 47, 1);

    display.setTextColor(TFT_WHITE);
    display.drawCentreString("Seeed Studio", w / 2, 78, 1);

    const int16_t swatchY = 100;
    const int16_t swatchW = 60;
    const int16_t swatchH = 78;
    const uint16_t colors[] = {TFT_RED, TFT_GREEN, TFT_BLUE, TFT_YELLOW};
    for (uint8_t i = 0; i < 4; ++i) {
        const int16_t x = 13 + i * 67;
        display.fillRoundRect(x, swatchY, swatchW, swatchH, 7, colors[i]);
        display.drawRoundRect(x, swatchY, swatchW, swatchH, 7, TFT_WHITE);
    }

    display.setTextColor(TFT_WHITE);
    display.setTextSize(2);
    display.drawCentreString("Hello World", w / 2, 196, 1);
    display.setTextSize(1);
    display.drawCentreString("Landscape", w / 2, 220, 1);
}

void loop() {}
