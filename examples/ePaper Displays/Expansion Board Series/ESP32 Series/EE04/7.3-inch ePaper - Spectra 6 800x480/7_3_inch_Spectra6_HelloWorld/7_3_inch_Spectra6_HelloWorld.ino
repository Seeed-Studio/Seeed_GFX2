/**
 * Product: XIAO ePaper Display Board (ESP32-S3) - EE04
 * Panel: 7.3 inch full-color E Ink Spectra 6, 800x480, ED2208
 * Wiki: https://wiki.seeedstudio.com/epaper_ee04/
 */

#include <Seeed_GFX.h>

Seeed_GFX display(Seeed_Product::XIAO_EPAPER_7INCH3_C);

void setup() {
    Serial.begin(115200);
    if (!display.begin()) {
        Serial.println(display.lastResult().message);
        return;
    }

    display.fillScreen(TFT_WHITE);
    display.fillRect(0, 0, display.width(), 82, TFT_BLACK);
    display.setTextColor(TFT_WHITE);
    display.setTextSize(3);
    display.drawString("XIAO ePaper Display Board - EE04", 36, 26);

    const uint16_t colors[] = {TFT_BLACK, TFT_RED, TFT_YELLOW,
                               TFT_GREEN, TFT_BLUE, TFT_WHITE};
    for (uint8_t i = 0; i < 6; ++i) {
        int16_t x = 46 + i * 120;
        display.fillRoundRect(x, 135, 96, 190, 10, colors[i]);
        display.drawRoundRect(x, 135, 96, 190, 10, TFT_BLACK);
    }

    display.setTextColor(TFT_BLACK);
    display.setTextSize(3);
    display.drawString("7.3 inch Spectra 6", 195, 380);
    const GfxResult refreshResult = display.refresh();
    if (!refreshResult) Serial.println(refreshResult.message);
}

void loop() { delay(1000); }
