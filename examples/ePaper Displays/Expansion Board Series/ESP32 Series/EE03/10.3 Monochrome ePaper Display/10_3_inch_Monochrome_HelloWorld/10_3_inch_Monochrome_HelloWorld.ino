/**
 * Product: XIAO ePaper Display Board - EE03
 * Panel: 10.3 inch monochrome ePaper, 1404x1872, ED103TC2/IT8951
 * Product overview: https://wiki.seeedstudio.com/xiao_epaper_display_board_overview/
 */

#include <Seeed_GFX.h>

Seeed_GFX display(Seeed_Product::XIAO_EPAPER_10INCH3);

void setup() {
    Serial.begin(115200);
    if (!display.begin()) {
        Serial.println(display.lastResult().message);
        return;
    }

    display.fillScreen(TFT_WHITE);
    display.fillRect(0, 0, display.width(), 190, TFT_BLACK);
    display.setTextColor(TFT_WHITE);
    display.setTextSize(6);
    display.drawString("XIAO ePaper Display Board - EE03", 70, 65);
    display.setTextColor(TFT_BLACK);
    display.setTextSize(7);
    display.drawString("10.3 inch monochrome", 105, 390);
    display.drawRoundRect(100, 610, display.width() - 200, 500, 32, TFT_BLACK);
    display.setTextSize(10);
    display.drawString("Hello World", 250, 790);
    display.update();
}

void loop() { delay(1000); }
