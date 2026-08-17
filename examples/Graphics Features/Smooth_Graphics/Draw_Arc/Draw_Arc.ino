// Example for drawArc function. This is intended for arc based meters.
// (See arcMeter example)

// Draws arcs without smooth ends, suitable for dynamically changing arc
// angles to avoid residual anti-alias pixels at the arc segment joints.

// The sides of the arc can optionally be smooth or not. Smooth arcs have
// a much better appearance, especially at small sizes.

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

  uint8_t radius       = random(20, display.width() / 4); // Outer arc radius
  uint8_t thickness    = random(1, radius / 4);     // Thickness
  uint8_t inner_radius = radius - thickness;        // Calculate inner radius (can be 0 for circle segment)

  // 0 degrees is at 6 o'clock position
  // Arcs are drawn clockwise from start_angle to end_angle
  // Start angle can be greater than end angle, the arc will then be drawn through 0 degrees
  uint16_t start_angle = random(361); // Start angle must be in range 0 to 360
  uint16_t end_angle   = random(361); // End angle must be in range 0 to 360

  bool smooth = random(2); // true = smooth sides, false = no smooth sides

  display.drawArc(x, y, radius, inner_radius, start_angle, end_angle, fg_color, bg_color, smooth);

  count++;
  if (count < 30) delay(500); // After 15s draw as fast as possible!
}
