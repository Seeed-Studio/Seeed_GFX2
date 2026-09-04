/**
 * Product: reTerminal E1001
 * Display: 7.5 inch monochrome ePaper, 800x480
 * Wiki: https://wiki.seeedstudio.com/reterminal_e10xx_main_page/
 */

#include <Seeed_GFX.h>
#include <font/GFXFF/FreeSans18pt7b.h>
#include <font/GFXFF/FreeSansBold24pt7b.h>

Seeed_GFX display(Seeed_Product::reTerminal_E1001);

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
    display.drawString("reTerminal E1001", display.width() / 2, 46);
    display.setTextColor(TFT_BLACK);
    display.setFreeFont(&FreeSans18pt7b);
    display.drawString("7.5 inch monochrome ePaper", display.width() / 2, 165);
    display.drawRoundRect(90, 230, 620, 150, 16, TFT_BLACK);
    display.setFreeFont(&FreeSansBold24pt7b);
    display.setTextSize(2);
    display.drawString("Hello World", display.width() / 2, 305);
    display.setFreeFont(nullptr);
    display.setTextSize(1);
    display.setTextDatum(TL_DATUM);
    const GfxResult refreshResult = display.refresh();
    if (!refreshResult.ok()) {
        Serial.println(refreshResult.message);
    }
}

void loop() { delay(1000); }
