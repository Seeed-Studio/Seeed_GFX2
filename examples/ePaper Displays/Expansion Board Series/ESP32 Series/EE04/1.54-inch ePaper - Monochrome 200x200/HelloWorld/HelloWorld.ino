/**
 * Panel: 1.54 inch monochrome ePaper (200x200, SSD1681)
 * Uses the registered product enum (EE04 driver board).
 */
#include <Seeed_GFX.h>

Seeed_GFX display(Seeed_Product::XIAO_EPAPER_1INCH54);

void setup() {
    Serial.begin(115200);
    if (!display.begin()) { Serial.println(display.lastResult().message); return; }


    display.setRotation(3);

    display.fillScreen(TFT_WHITE);

    const int16_t w = display.width();   // 200
    const int16_t h = display.height();  // 200

    // Outer frame
    display.drawRect(3, 3, w - 6, h - 6, TFT_BLACK);

    // Title
    display.setTextColor(TFT_BLACK);
    display.setTextSize(2);
    display.drawCentreString("1.54 inch BW", w / 2, 12, 1);
    display.setTextSize(1);
    display.drawCentreString("SSD1681", w / 2, 32, 1);

    // Divider
    display.drawLine(12, 48, w - 12, 48, TFT_BLACK);

    // Shape sampler
    display.drawCircle(48, 110, 22, TFT_BLACK);
    display.fillCircle(48, 110, 9, TFT_BLACK);
    display.drawRect(86, 88, 44, 44, TFT_BLACK);
    display.fillRect(98, 100, 20, 20, TFT_BLACK);
    display.drawTriangle(150, 88, 184, 88, 167, 132, TFT_BLACK);
    display.fillTriangle(150, 132, 184, 132, 167, 96, TFT_BLACK);

    // Hello-world banner
    display.fillRect(12, h - 34, w - 24, 24, TFT_BLACK);
    display.setTextColor(TFT_WHITE);
    display.setTextSize(2);
    display.drawCentreString("Hello World", w / 2, h - 30, 1);

    display.update();
}

void loop() {}
