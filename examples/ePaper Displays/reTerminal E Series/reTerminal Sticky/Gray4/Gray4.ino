/**
 * Product: reTerminal Sticky
 * Display: 3.97-inch 800x480 monochrome ePaper with 4-level gray support
 * Modeled on: reTerminal_E1001_Gray4 (same panel resolution, SSD1677/SSD2677)
 *
 * Gray4 uses a packed 4bpp frame buffer whose valid indexes are:
 *   0 = black, 1/2 = intermediate gray, 3 = white.
 * The embedded L4_GRAY asset is already packed as two indexes per byte;
 * it is not an RGB565 image and must be loaded with pushImage4BPP().
 */

#include <Seeed_GFX.h>
#include "image.h"

Seeed_GFX display(Seeed_Product::reTerminal_Sticky);

void setup() {
    Serial.begin(115200);
    delay(2000);  // let USB CDC enumerate so the early prints are not lost
    Serial.println("[sticky] sketch start (Gray4)");
    Serial.println("[sticky] display.begin() ... (busy timeouts can take a while)");

    if (!display.begin()) {
        Serial.printf("Display initialization failed: %s\n",
                      display.lastResult().message);
        return;
    }

    // Diagnostics: Sticky production mixes SSD1677 and SSD2677 modules;
    // Driver_Sticky_Auto probes at begin() (reset -> 0x70 -> read one byte,
    // 0x07 = SSD2677). Print the resolution so units with a missing image
    // can be traced to either the probe or the driver path.
    // No driverAs<>() here: Arduino targets build with -fno-rtti, so
    // dynamic_cast is unavailable; IDriver::probedChipId() covers it.
    IDriver* sticky = display.driverPtr();
    const int chipId = sticky->probedChipId();
    if (chipId >= 0) {
        Serial.printf("[sticky] probe chipId=0x%02X -> driver %s\n",
                      chipId, sticky->name());
    } else {
        Serial.printf("[sticky] driver: %s\n", sticky->name());
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
    Serial.println("reTerminal Sticky Gray4 example complete");
}

void loop() {
    delay(1000);
}
