/**
 * Product: ePaper Driver Board for Seeed Studio XIAO
 * Panel: 4.26 inch monochrome ePaper, 800x480, SSD1677
 * Wiki: https://wiki.seeedstudio.com/xiao_eink_expansion_board_v2/
 *
 * This example shows a basic "Hello World" layout with text,
 * geometric shapes, and a refresh call on a 4.26" monochrome panel.
 */

#include <Seeed_GFX.h>

Seeed_GFX display(Seeed_Product::Seeed_ePaper_4INCH26);

void setup() {
    Serial.begin(115200);
    if (!display.begin()) {
        Serial.println(display.lastResult().message);
        return;
    }

    // Fill the screen with white
    display.fillScreen(TFT_WHITE);

    // Top banner
    display.fillRect(0, 0, display.width(), 86, TFT_BLACK);
    display.setTextColor(TFT_WHITE);
    display.setTextSize(3);
    display.drawString("XIAO ePaper Driver Board", 48, 28);

    // Product info
    display.setTextColor(TFT_BLACK);
    display.setTextSize(2);
    display.drawString("4.26 inch monochrome ePaper", 60, 125);
    display.setTextSize(1);
    display.drawString("800 x 480  |  SSD1677  |  1 bpp", 96, 170);

    // Rounded rectangle frame
    display.drawRoundRect(48, 210, 704, 170, 12, TFT_BLACK);

    // Hello World in the center
    display.setTextSize(5);
    display.drawString("Hello World", 180, 260);

    // Bottom bar
    display.fillRect(0, display.height() - 40, display.width(), 40, TFT_BLACK);
    display.setTextColor(TFT_WHITE);
    display.setTextSize(1);
    display.drawString("Seeed Studio  |  seeedstudio.com", 48, display.height() - 30);

    // Refresh the ePaper panel
    display.update();
}

void loop() {
    delay(1000);
}