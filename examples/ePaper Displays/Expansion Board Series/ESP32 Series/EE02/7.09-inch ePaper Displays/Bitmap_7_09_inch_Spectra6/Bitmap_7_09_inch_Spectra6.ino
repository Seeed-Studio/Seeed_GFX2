/**
 * Product: XIAO ePaper Display Board - EE02
 * Panel: 7.09 inch full-color E Ink Spectra 6, 1200x1600, GDEB0709E01
 *        (dual COG / two chip selects, OTP waveform, ~27 s full refresh)
 * Demo:  Display an embedded bitmap image.
 * Product overview: https://wiki.seeedstudio.com/seeed_epaper_displays/
 */

#include <Seeed_GFX.h>
#include "image.h"

Seeed_GFX display(Seeed_Product::XIAO_EPAPER_7INCH09_C);

void setup() {
    Serial.begin(115200);
    if (!display.begin()) {
        Serial.println(display.lastResult().message);
        return;
    }

    display.setRotation(3);

    display.fillScreen(TFT_WHITE);

    // The asset is packed indexed 4bpp (two pixels per byte), not RGB565.
    if (!display.pushImage4BPP(0, 0, 1600, 1200, gImage_7inch09, true)) {
        Serial.println("Packed 4bpp image rejected");
        return;
    }

    Serial.println("Refreshing (Spectra 6 full refresh takes ~27 s)...");
    const GfxResult refreshResult = display.refresh();
    if (!refreshResult) {
        Serial.println(refreshResult.message);
        return;
    }

    Serial.println("Bitmap displayed successfully");
}

void loop() {
    // nothing to do
}
