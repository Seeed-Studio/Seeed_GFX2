/**
 * Product: reTerminal E1001
 * Display: 7.5-inch 800x480 monochrome ePaper with 4-level gray support
 * Source:  Seeed_GFX examples/ePaper/Gray/GrayLevel4
 *
 * Gray4 uses a packed 4bpp frame buffer whose valid indexes are:
 *   0 = black, 1/2 = intermediate gray, 3 = white.
 * The embedded L4_GRAY asset is already packed as two indexes per byte;
 * it is not an RGB565 image and must be loaded with pushImage4BPP().
 */

#include <Seeed_GFX.h>
#include "image.h"

Seeed_GFX display(Seeed_Product::reTerminal_E1001);

void setup() {
    Serial.begin(115200);
    delay(200);

    if (!display.begin()) {
        Serial.printf("Display initialization failed: %s\n",
                      display.lastResult().message);
        return;
    }

    const GfxResult modeResult =
        display.panel().configure(PanelMode::Gray4);
    if (!modeResult) {
        Serial.printf("Gray4 configuration failed: %s\n",
                      modeResult.message);
        return;
    }

    // Preserve the original packed Gray4 indexes byte-for-byte.
    if (!display.pushImage4BPP(0, 0, 800, 480, L4_GRAY, true)) {
        Serial.println("Packed Gray4 image was rejected");
        return;
    }

    const GfxResult refreshResult = display.refresh();
    if (!refreshResult) {
        Serial.printf("Gray4 refresh failed: %s\n", refreshResult.message);
        return;
    }
    Serial.println("reTerminal E1001 Gray4 example complete");
}

void loop() {
    delay(1000);
}
