// Arc drawing example - draw a colour wheel

#include <Seeed_GFX.h>

// -- Board & Driver: edit for your hardware --
#include "board/boards/XIAO_LCD_Board.h"
#include "driver/tft/Driver_ST7789.h"
#include "panel/Panel_TFT.h"       // Include the graphics library
Seeed_GFX display;  // Create object "tft"

uint16_t colors[12];

// Setup
void setup(void) {
  Serial.begin(115200);
  // -- Edit for your board/config --
    if (!display.begin<Board_Seeed_1inch69_LCD, Config_Seeed_1inch69_LCD_ST7789>()) {
        Serial.println(display.lastResult().message);
        while (true) delay(1000);
    }
  display.fillScreen(TFT_BLACK);

  // Create the outer ring colours
  for (uint8_t c = 0; c < 2; c++) {
    colors[c + 10] = display.alphaBlend(128 + c * 127, TFT_RED,     TFT_MAGENTA);
    colors[c +  8] = display.alphaBlend(128 + c * 127, TFT_MAGENTA, TFT_BLUE);
    colors[c +  6] = display.alphaBlend(128 + c * 127, TFT_BLUE,    TFT_GREEN);
    colors[c +  4] = display.alphaBlend(128 + c * 127, TFT_GREEN,   TFT_YELLOW);
    colors[c +  2] = display.alphaBlend(128 + c * 127, TFT_YELLOW,  TFT_ORANGE);
    colors[c +  0] = display.alphaBlend(128 + c * 127, TFT_ORANGE,  TFT_RED);
  }
}

// Main loop
void loop() {
  uint16_t rDelta = (display.width() - 1) / 10;
  uint16_t x = display.width() / 2;
  uint16_t y = display.height() / 2;
  bool smooth = true;

  // Draw rings as a series of arcs, increasingly blend colour with white towards middle
  for (uint16_t i = 5; i > 0; i--) {
    for (uint16_t angle = 0; angle <= 330; angle += 30) {
      uint16_t radius = i * rDelta;
      uint16_t wheelColor = display.alphaBlend((i * 255.0)/5.0, colors[angle / 30], TFT_WHITE);
      display.drawArc(x, y, radius, radius - rDelta, angle, angle + 30, wheelColor, TFT_BLACK, smooth);
    }
    smooth = false;  // Only outer ring is smooth
  }

  while (1) delay(100);
}
