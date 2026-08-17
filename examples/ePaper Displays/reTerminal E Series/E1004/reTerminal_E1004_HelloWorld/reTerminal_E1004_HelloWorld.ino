/**
 * Product: reTerminal E1004
 * Display: 13.3 inch full-color E Ink Spectra 6, 1200x1600
 * Special hardware: the panel requires two display chip-select pins.
 * Wiki: https://wiki.seeedstudio.com/getting_started_with_reterminal_e1004/
 */

#include <Seeed_GFX.h>
#include <font/GFXFF/FreeSans18pt7b.h>
#include <font/GFXFF/FreeSansBold24pt7b.h>

Seeed_GFX display(Seeed_Product::RETERMINAL_E1004);

void setup() {
    Serial.begin(115200);
    delay(2500);
    Serial.println();
    Serial.println("reTerminal E1004 HelloWorld");
#if defined(ESP32)
    Serial.printf("PSRAM: %lu KB total, %lu KB free\n",
                  static_cast<unsigned long>(ESP.getPsramSize() / 1024),
                  static_cast<unsigned long>(ESP.getFreePsram() / 1024));
#endif
    Serial.println("display.begin() ...");
    Serial.flush();
    if (!display.begin()) {
        Serial.print("display.begin() failed: ");
        Serial.println(display.lastResult().message);
        Serial.flush();
        return;
    }
    Serial.printf("display ready: %u x %u\n", display.width(), display.height());

    // Build the complete frame on a white background, then perform one
    // physical Spectra 6 refresh. A separate white pre-refresh would double
    // the slow full-refresh cycle and leave the glass white between cycles.
    display.fillScreen(TFT_WHITE);
    Serial.println("drawing content ...");
    display.fillRect(0, 0, display.width(), 180, TFT_BLACK);
    display.setTextDatum(MC_DATUM);
    display.setTextColor(TFT_WHITE);
    display.setFreeFont(&FreeSansBold24pt7b);
    display.setTextSize(2);
    display.drawString("reTerminal E1004", display.width() / 2, 90);
    display.fillRoundRect(100, 300, 1000, 420, 28, TFT_RED);
    display.fillRoundRect(180, 380, 840, 260, 22, TFT_YELLOW);
    display.setTextColor(TFT_BLACK);
    display.setFreeFont(&FreeSans18pt7b);
    display.setTextSize(2);
    display.drawString("13.3 inch Spectra 6", display.width() / 2, 850);
    display.setFreeFont(&FreeSansBold24pt7b);
    display.setTextSize(3);
    display.drawString("Hello World", display.width() / 2, 1100);
    display.setFreeFont(nullptr);
    display.setTextSize(1);
    display.setTextDatum(TL_DATUM);
    Serial.println("content refresh() ...");
    Serial.flush();
    const GfxResult refreshResult = display.refresh();
    if (!refreshResult.ok()) {
        Serial.print("content refresh() failed: ");
        Serial.println(refreshResult.message);
    } else {
        Serial.println("content refresh() complete");
    }
    Serial.flush();
}

void loop() { delay(1000); }
