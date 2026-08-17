/**
 * Product: XIAO ePaper Display Board (ESP32-S3) - EE04
 * Panel: 7.5 inch monochrome ePaper, 800x480, UC8179
 * Wiki: https://wiki.seeedstudio.com/epaper_ee04/
 */

#include <Seeed_GFX.h>
#include <font/GFXFF/FreeSans18pt7b.h>
#include <font/GFXFF/FreeSansBold24pt7b.h>

Seeed_GFX display(Seeed_Product::XIAO_EPAPER_7INCH5);

void setup() {
    Serial.begin(115200);
    if (!display.begin()) {
        Serial.println(display.lastResult().message);
        return;
    }

    display.fillScreen(TFT_WHITE);

    const int16_t w = display.width();   // 800

    // Top banner
    display.fillRect(0, 0, w, 90, TFT_BLACK);
    display.setTextColor(TFT_WHITE);
    display.setFreeFont(&FreeSansBold24pt7b);
    display.drawString("XIAO ePaper Display Board - EE04", 24, 20);

    // Subtitle
    display.setTextColor(TFT_BLACK);
    display.setFreeFont(&FreeSans18pt7b);
    display.drawString("7.5 inch monochrome ePaper", 36, 120);
    display.drawString("800 x 480  |  UC8179  |  1 bpp", 36, 160);

    // Decorative box
    display.drawRoundRect(36, 210, w - 72, 180, 16, TFT_BLACK);

    // Hello World text
    display.setFreeFont(&FreeSansBold24pt7b);
    display.setTextSize(2);
    display.drawCentreString("Hello World", w / 2, 270, 1);

    // Reset font and size
    display.setFreeFont(nullptr);
    display.setTextSize(1);

    const GfxResult refreshResult = display.refresh();
    if (!refreshResult) Serial.println(refreshResult.message);
}

void loop() { delay(1000); }
