// Draw random coloured smooth (anti-aliased) rounded rectangles on the TFT

#include <Seeed_GFX.h>

// -- Board & Driver: edit for your hardware --
#include "board/boards/XIAO_LCD_Board.h"
#include "driver/tft/Driver_ST7789.h"
#include "panel/Panel_TFT.h"

Seeed_GFX display;

void setup(void) {
  // -- Edit for your board/config --
    if (!display.begin<Board_Seeed_1inch69_LCD, Config_Seeed_1inch69_LCD_ST7789>()) {
        Serial.println(display.lastResult().message);
        while (true) delay(1000);
    }
  display.fillScreen(TFT_BLACK); // Background is black
}

void loop() {
  display.fillScreen(TFT_BLACK);
  display.setCursor(0, 0);

  // Draw some random smooth rounded rectangles
  for (int i = 0; i < 20; i++)
  {
    // Keep the radius non-zero so the outline thickness range is valid.
    int radius = random(2, 60);
    int w = random(2 * radius, 160);
    int h = random(2 * radius, 160);
    int t = random(1, max(2, radius / 3 + 1));
    int x = random(display.width() - w);
    int y = random(display.height() - h);

    // Random colour is anti-aliased (blended) with background colour (black in this case)
    display.drawSmoothRoundRect(x, y, radius, radius - t, w, h, random(0x10000), TFT_BLACK);
  }
  display.print("Variable thickness");
  delay(2000);

  display.fillScreen(TFT_BLACK);
  display.setCursor(0, 0);

  // Draw some random minimum thickness smooth rounded rectangles
  for (int i = 0; i < 20; i++)
  {
    int radius = random(2, 60);
    int w = random(2 * radius, 160);
    int h = random(2 * radius, 160);
    int t = 0;
    int x = random(display.width() - w);
    int y = random(display.height() - h);

    // Random colour is anti-aliased (blended) with background colour (black in this case)
    display.drawSmoothRoundRect(x, y, radius, radius - t, w, h, random(0x10000), TFT_BLACK);
  }
  display.print("Minimum thickness");
  delay(2000);
}
