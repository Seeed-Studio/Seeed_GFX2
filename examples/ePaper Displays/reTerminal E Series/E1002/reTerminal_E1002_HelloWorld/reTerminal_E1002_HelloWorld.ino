/**
 * Product: reTerminal E1002
 * Display: 7.3 inch full-color E Ink Spectra 6, 800x480
 * Wiki: https://wiki.seeedstudio.com/reterminal_e10xx_main_page/
 */

#include <Seeed_GFX.h>
#include <font/GFXFF/FreeSans18pt7b.h>
#include <font/GFXFF/FreeSansBold24pt7b.h>

Seeed_GFX display(Seeed_Product::RETERMINAL_E1002);

void setup() {
    Serial.begin(115200);
    if (!display.begin()) {
        Serial.println(display.lastResult().message);
        return;
    }

    // Pre-clear: force physical white refresh to erase previous image
    display.fillScreen(TFT_WHITE);
    display.refresh();
    delay(500);
    display.fillScreen(TFT_WHITE);
    display.fillRect(0, 0, display.width(), 92, TFT_BLACK);
    display.setTextDatum(MC_DATUM);
    display.setTextColor(TFT_WHITE);
    display.setFreeFont(&FreeSansBold24pt7b);
    display.setTextSize(1);
    display.drawString("reTerminal E1002", display.width() / 2, 46);
    display.fillRoundRect(60, 140, 200, 220, 20, TFT_RED);
    display.fillRoundRect(300, 140, 200, 220, 20, TFT_YELLOW);
    display.fillRoundRect(540, 140, 200, 220, 20, TFT_BLUE);
    display.setTextColor(TFT_BLACK);
    display.setFreeFont(&FreeSans18pt7b);
    display.drawString("7.3 inch Spectra 6", display.width() / 2, 420);
    display.setFreeFont(nullptr);
    display.setTextSize(1);
    display.setTextDatum(TL_DATUM);
    const GfxResult refreshResult = display.refresh();
    if (!refreshResult.ok()) {
        Serial.println(refreshResult.message);
    }
}

void loop() { delay(1000); }
