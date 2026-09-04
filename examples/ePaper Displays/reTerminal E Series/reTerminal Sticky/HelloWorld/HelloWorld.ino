/**
  * Product: reTerminal Sticky
 * Display: 3.97 inch monochrome ePaper, 800x480, SSD1677/SSD2677
 * Wiki: https://www.seeedstudio.com/sticky/docs/quick-start
 */
#include <Seeed_GFX.h>

Seeed_GFX display(Seeed_Product::reTerminal_Sticky);

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

    const int16_t w = display.width();   // 800
    const int16_t h = display.height();  // 480

    // Top banner
    display.fillRect(0, 0, w, 92, TFT_BLACK);
    display.setTextColor(TFT_WHITE);
    display.setTextSize(4);
    display.drawString("reTerminal Sticky", 150, 28);

    // Subtitle
    display.setTextColor(TFT_BLACK);
    display.setTextSize(3);
    display.drawString("3.97 inch monochrome ePaper", 125, 155);
    display.setTextSize(1);
    display.drawString("800 x 480  | 1 bpp", 160, 190);

    // Hello World card
    display.drawRoundRect(90, 220, 620, 160, 16, TFT_BLACK);
    display.setTextSize(5);
    display.drawString("Hello World", 215, 270);

    // Bottom bar
    display.fillRect(0, h - 40, w, 40, TFT_BLACK);
    display.setTextColor(TFT_WHITE);
    display.setTextSize(1);
    display.drawString("Seeed Studio  |  seeedstudio.com", 48, h - 30);

    display.update();
}

void loop() { delay(1000); }
