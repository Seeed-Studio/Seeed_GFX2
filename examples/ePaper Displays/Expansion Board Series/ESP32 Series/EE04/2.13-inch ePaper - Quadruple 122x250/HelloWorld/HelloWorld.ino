/**
 * Panel: 2.13 inch BWRY ePaper (122x250 visible, 128x250 controller RAM)
 * Driver: Seeed-compatible JD79676 path
 * Native colors: Black / White / Red / Yellow
 * Uses the registered product enum (EE04 driver board).
 */
#include <Seeed_GFX.h>

Seeed_GFX display(Seeed_Product::XIAO_EPAPER_2INCH13_BWRY);

void setup() {
    Serial.begin(115200);
    if (!display.begin()) { Serial.println(display.lastResult().message); return; }

    // Storage remains 128x250, while the drawing viewport is 122x250.
    // Rotation exposes the correct 250x122 visible landscape area.
    display.setRotation(3);

    display.fillScreen(TFT_WHITE);

    const int16_t w = display.width();   // 212
    const int16_t h = display.height();  // 104

    display.drawRect(2, 2, w - 4, h - 4, TFT_BLACK);

    display.setTextColor(TFT_BLACK);
    display.setTextSize(2);
    display.drawString("2.13 BWRY", 6, 4);
    display.setTextSize(1);
    display.drawRightString("4-color", w - 6, 8, 1);

    display.drawLine(6, 26, w - 6, 26, TFT_BLACK);

    // Palette swatches: Black / White / Red / Yellow
    const int16_t sw = 45, sh = 44, gap = 6;
    const int16_t sy = 32;
    const uint16_t colors[4] = { TFT_BLACK, TFT_WHITE, TFT_RED, TFT_YELLOW };
    for (uint8_t i = 0; i < 4; i++) {
        int16_t sx = 6 + i * (sw + gap);
        display.fillRect(sx, sy, sw, sh, colors[i]);
        display.drawRect(sx, sy, sw, sh, TFT_BLACK);
    }

    // Hello-world banner (yellow on black)
    display.fillRect(6, h - 18, w - 12, 14, TFT_BLACK);
    display.setTextColor(TFT_YELLOW);
    display.setTextSize(1);
    display.drawCentreString("Hello World", w / 2, h - 16, 1);

    display.update();
}

void loop() {}
