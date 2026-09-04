/**
 * Product: Wio Terminal
 * Display: built-in 2.4 inch, 320x240, ILI9341
 * Wiki: https://wiki.seeedstudio.com/Wio-Terminal-Getting-Started/
 */

#include <Seeed_GFX.h>

Seeed_GFX display(Seeed_Product::Wio_Terminal);

void setup() {
    Serial.begin(115200);

    if (!display.begin()) {
        Serial.println(display.lastResult().message);
        return;
    }

    display.fillScreen(TFT_BLACK);
    display.fillRoundRect(20, 45, 280, 150, 12, TFT_DARKGREEN);
    display.setTextColor(TFT_WHITE);
    display.setTextSize(3);
    display.drawCentreString("Wio Terminal", 160, 85, 1);
    display.setTextSize(1);
    display.drawCentreString("Built-in 2.4 inch LCD", 160, 135, 1);
}

void loop() {}
