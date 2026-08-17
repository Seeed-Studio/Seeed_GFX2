/**
 * Product: Seeed Studio Round Display for XIAO
 * Display: 1.28 inch, 240x240, GC9A01, capacitive touch
 * Wiki: https://wiki.seeedstudio.com/get_start_round_display/
 */

#include <Seeed_GFX.h>

Seeed_GFX display(Seeed_Product::XIAO_ROUND_DISPLAY);

void setup() {
    Serial.begin(115200);

    if (!display.begin()) {
        Serial.println(display.lastResult().message);
        return;
    }

    display.fillScreen(TFT_BLACK);
    display.drawCircle(120, 120, 108, TFT_DARKGREY);
    display.fillCircle(120, 72, 12, TFT_GREEN);
    display.setTextColor(TFT_WHITE);
    display.setTextSize(2);
    display.drawCentreString("Round Display", 120, 105, 1);
    display.setTextSize(1);
    display.drawCentreString("for Seeed Studio XIAO", 120, 135, 1);
}

void loop() {}
