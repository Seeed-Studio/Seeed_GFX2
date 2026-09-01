/**
 * Product: XIAO ePaper Display Board - EE02
 * Panel: 7.09 inch full-color E Ink Spectra 6, 1200x1600, GDEB0709E01
 *        (dual COG / two chip selects, OTP waveform, ~27 s full refresh)
 * Product overview: https://wiki.seeedstudio.com/seeed_epaper_displays/
 */

#include <Seeed_GFX.h>

Seeed_GFX display(Seeed_Product::XIAO_EPAPER_7INCH09_C);

void setup() {
    Serial.begin(115200);
    if (!display.begin()) {
        Serial.println(display.lastResult().message);
        return;
    }

    display.fillScreen(TFT_WHITE);

    // Header bar
    display.fillRect(0, 0, display.width(), 180, TFT_BLACK);
    display.setTextColor(TFT_WHITE);
    display.setTextSize(5);
    display.drawString("XIAO ePaper Display Board - EE02", 105, 62);

    display.setTextColor(TFT_BLACK);
    display.setTextSize(6);
    display.drawString("7.09 inch Spectra 6", 258, 280);

    // Hello World block
    display.fillRoundRect(110, 460, 980, 520, 28, TFT_YELLOW);
    display.fillRoundRect(190, 540, 820, 360, 24, TFT_RED);
    display.setTextColor(TFT_WHITE);
    display.setTextSize(8);
    display.drawString("Hello World", 336, 688);

    // Six native-color swatches: a first-light check that all Spectra 6
    // inks (black/white/red/yellow/blue/green) appear and that the dual
    // chip-select row split is aligned.
    display.setTextColor(TFT_BLACK);
    display.setTextSize(4);
    display.drawString("Spectra 6 native colors", 324, 1030);

    const uint16_t swatchColors[6] = {TFT_BLACK, TFT_WHITE, TFT_RED,
                                      TFT_YELLOW, TFT_BLUE, TFT_GREEN};
    const char* swatchNames[6] = {"BLK", "WHT", "RED", "YEL", "BLU", "GRN"};
    for (uint8_t i = 0; i < 6; i++) {
        const int16_t x = 55 + i * (170 + 14);
        display.fillRect(x, 1100, 170, 280, swatchColors[i]);
        if (swatchColors[i] == TFT_WHITE) {
            display.drawRect(x, 1100, 170, 280, TFT_BLACK);
        }
        display.drawString(swatchNames[i], x + 49, 1420);
    }

    Serial.println("Refreshing (Spectra 6 full refresh takes ~27 s)...");
    display.update();
    Serial.println("Done.");
}

void loop() { delay(1000); }
