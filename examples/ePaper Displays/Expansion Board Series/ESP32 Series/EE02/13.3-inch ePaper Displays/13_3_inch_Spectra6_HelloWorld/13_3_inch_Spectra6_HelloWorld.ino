/**
 * Product: XIAO ePaper Display Board - EE02
 * Panel: 13.3 inch full-color E Ink Spectra 6, 1200x1600, T133A01
 * Special hardware: the panel requires two display chip-select pins.
 * Product overview: https://wiki.seeedstudio.com/seeed_epaper_displays/
 */

#include <Seeed_GFX.h>

Seeed_GFX display(Seeed_Product::XIAO_EPAPER_13INCH3_C);

void setup() {
    Serial.begin(115200);
    if (!display.begin()) {
        Serial.println(display.lastResult().message);
        return;
    }

    display.fillScreen(TFT_WHITE);
    display.fillRect(0, 0, display.width(), 180, TFT_BLACK);
    display.setTextColor(TFT_WHITE);
    display.setTextSize(5);
    display.drawString("XIAO ePaper Display Board - EE02", 70, 62);
    display.setTextColor(TFT_BLACK);
    display.setTextSize(6);
    display.drawString("13.3 inch Spectra 6", 145, 330);
    display.fillRoundRect(110, 520, 980, 520, 28, TFT_YELLOW);
    display.fillRoundRect(190, 600, 820, 360, 24, TFT_RED);
    display.setTextColor(TFT_WHITE);
    display.setTextSize(8);
    display.drawString("Hello World", 250, 735);
    display.update();
}

void loop() { delay(1000); }
