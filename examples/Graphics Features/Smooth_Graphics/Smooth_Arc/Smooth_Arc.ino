// Example for drawSmoothArc function.
// Draws smooth arcs with rounded or square smooth ends

#include <Seeed_GFX.h>

// -- Board & Driver: edit for your hardware --
#include "board/boards/XIAO_LCD_Board.h"
#include "driver/tft/Driver_ST7789.h"
#include "panel/Panel_TFT.h"       // Include the graphics library
Seeed_GFX display;  // Create object "tft"

// Setup
void setup(void) {
  Serial.begin(115200);

  // -- Edit for your board/config --
    if (!display.begin<Board_Seeed_1inch69_LCD, Config_Seeed_1inch69_LCD_ST7789>()) {
        Serial.println(display.lastResult().message);
        while (true) delay(1000);
    }
  display.setRotation(1);
  display.fillScreen(TFT_BLACK);
}

// Main loop
void loop()
{
  static uint32_t count = 0;

  uint16_t fg_color = random(0x10000);
  uint16_t bg_color = TFT_BLACK;       // This is the background colour used for smoothing (anti-aliasing)

  uint16_t x = random(display.width());  // Position of centre of arc
  uint16_t y = random(display.height());

  uint8_t radius       = random(20, display.width()/4); // Outer arc radius
  uint8_t thickness    = random(1, radius / 4);     // Thickness
  uint8_t inner_radius = radius - thickness;        // Calculate inner radius (can be 0 for circle segment)

  // 0 degrees is at 6 o'clock position
  // Arcs are drawn clockwise from start_angle to end_angle
  uint16_t start_angle = random(361); // Start angle must be in range 0 to 360
  uint16_t end_angle   = random(361); // End angle must be in range 0 to 360

  bool arc_end = random(2);           // true = round ends, false = square ends (arc_end parameter can be omitted, ends will then be square)

  display.drawSmoothArc(x, y, radius, inner_radius, start_angle, end_angle, fg_color, bg_color, arc_end);

  count++;
  if (count < 30) delay(500); // After 15s draw as fast as possible!
}
