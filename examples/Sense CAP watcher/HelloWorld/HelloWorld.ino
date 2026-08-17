#include <Seeed_GFX.h>

Seeed_GFX display(Seeed_Product::SENSECAP_WATCHER);

void setup() {
    Serial.begin(115200);
    if (!display.begin()) {
        Serial.print("SenseCAP Watcher display init failed: ");
        Serial.println(display.lastResult().message);
        return;
    }

    display.fillScreen(TFT_BLACK);
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.setTextDatum(MC_DATUM);
    display.drawString("Hello SenseCAP Watcher",
                       display.width() / 2, display.height() / 2, 4);
}

void loop() {}
