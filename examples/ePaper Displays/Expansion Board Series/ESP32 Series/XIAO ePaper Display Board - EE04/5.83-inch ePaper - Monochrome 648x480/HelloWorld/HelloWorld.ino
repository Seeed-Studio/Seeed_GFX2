/**
 * Panel: 5.83 inch monochrome ePaper (648x480, UC8179)
 * Uses the registered product enum (EE04 driver board).
 */
#include <Seeed_GFX.h>

Seeed_GFX display(Seeed_Product::Seeed_ePaper_5INCH83);

void setup() {
    Serial.begin(115200);
    if (!display.begin()) { Serial.println(display.lastResult().message); return; }
    display.fillScreen(TFT_WHITE);

    const int16_t w = display.width();   // 648
    const int16_t h = display.height();  // 480

    display.drawRect(4, 4, w - 8, h - 8, TFT_BLACK);

    display.setTextColor(TFT_BLACK);
    display.setTextSize(3);
    display.drawString("5.83 inch Monochrome", 12, 10);
    display.setTextSize(1);
    display.drawRightString("UC8179 / 648x480", w - 12, 18, 1);

    display.drawLine(12, 50, w - 12, 50, TFT_BLACK);

    // Shape sampler
    display.drawCircle(110, 180, 60, TFT_BLACK);
    display.fillCircle(110, 180, 26, TFT_BLACK);
    display.drawRect(210, 120, 150, 120, TFT_BLACK);
    display.fillRect(250, 150, 70, 60, TFT_BLACK);
    display.drawTriangle(400, 120, 560, 120, 480, 250, TFT_BLACK);
    for (int i = 0; i < 80; i += 8)
        display.drawLine(210 + i, 300, 210, 300 - i, TFT_BLACK);

    // Hello-world banner
    display.fillRect(16, h - 70, w - 32, 54, TFT_BLACK);
    display.setTextColor(TFT_WHITE);
    display.setTextSize(4);
    display.drawCentreString("Hello World", w / 2, h - 52, 1);

    display.update();
}
void loop() {}
