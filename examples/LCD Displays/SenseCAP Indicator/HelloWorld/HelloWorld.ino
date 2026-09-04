#include <Seeed_GFX.h>

// Select SenseCAP_Indicator_DX when the unit has the RGB-only DX panel.
Seeed_GFX display(Seeed_Product::SenseCAP_Indicator_GX);

void setup() {
    Serial.begin(115200);
#if defined(CORE_DEBUG_LEVEL) && CORE_DEBUG_LEVEL >= 5
    Serial.println(
        "WARNING: Set Tools > Core Debug Level to None; Verbose I2C logs "
        "make Indicator startup much slower.");
#endif
    Serial.println("SenseCAP Indicator display init starting");
    const uint32_t initStartedMs = millis();
    if (!display.begin()) {
        Serial.print("SenseCAP Indicator display init failed: ");
        Serial.println(display.lastResult().message);
        return;
    }
    Serial.print("SenseCAP Indicator display init OK, ms: ");
    Serial.println(millis() - initStartedMs);

    // The panel lifecycle already enabled the backlight. The complete scene is
    // still published atomically by this one double-buffered transaction.
    display.startWrite();
    display.fillScreen(TFT_NAVY);
    display.setTextColor(TFT_WHITE, TFT_NAVY);
    display.setTextDatum(MC_DATUM);
    display.drawString("Hello",
                       display.width() / 2, display.height() / 2 - 38, 4);
    display.drawString("SenseCAP Indicator",
                       display.width() / 2, display.height() / 2 + 38, 4);
    display.endWrite();
    IBus& displayBus = display.panel().driver().bus();
    if (displayBus.lastError() != 0) {
        Serial.print("SenseCAP Indicator first frame failed: ");
        Serial.println(displayBus.lastErrorMessage());
        return;
    }
    Serial.println("SenseCAP Indicator frame rendered");
}

void loop() {}
