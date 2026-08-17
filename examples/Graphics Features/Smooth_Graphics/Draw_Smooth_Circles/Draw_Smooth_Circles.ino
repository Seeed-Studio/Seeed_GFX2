// Example for drawSmoothCircle function. Which draws anti-aliased circles
// The circle periphery has a "thickness" of ~3 pixles to minimise the
// "braiding" effect present in narrow anti-aliased lines.

// For thicker or thinner circle outlines use the drawArc function.

#include <Seeed_GFX.h>

// -- Board & Driver: edit for your hardware --
#include "board/boards/XIAO_LCD_Board.h"
#include "driver/tft/Driver_ST7789.h"
#include "panel/Panel_TFT.h"       // Include the graphics library
Seeed_GFX display;  // Create object "tft"

unsigned int rainbow(byte value);

// Setup
void setup(void) {
  Serial.begin(115200);
  // -- Edit for your board/config --
    if (!display.begin<Board_Seeed_1inch69_LCD, Config_Seeed_1inch69_LCD_ST7789>()) {
        Serial.println(display.lastResult().message);
        while (true) delay(1000);
    }
  display.fillScreen(TFT_BLACK);
}

// Main loop
void loop()
{
  static uint32_t radius = 2;
  static uint32_t index = 0;

  uint16_t fg_color = rainbow(index);
  uint16_t bg_color = TFT_BLACK;       // This is the background colour used for smoothing (anti-aliasing)

  uint16_t x = display.width() / 2; // Position of centre of arc
  uint16_t y = display.height() / 2;

  display.drawSmoothCircle(x, y, radius, fg_color, bg_color);

  radius += 11;
  index += 5;
  index = index%192;

  const uint32_t maxRadius = min(display.width(), display.height()) / 2;
  if (radius > maxRadius) {
    delay (1000);
    radius = 2;
  }
}


// Return a 16-bit rainbow colour
unsigned int rainbow(byte value)
{
  // If 'value' is in the range 0-159 it is converted to a spectrum colour
  // from 0 = red through to 127 = blue to 159 = violet
  // Extending the range to 0-191 adds a further violet to red band
 
  value = value%192;
  
  byte red   = 0; // Red is the top 5 bits of a 16-bit colour value
  byte green = 0; // Green is the middle 6 bits, but only top 5 bits used here
  byte blue  = 0; // Blue is the bottom 5 bits

  byte sector = value >> 5;
  byte amplit = value & 0x1F;

  switch (sector)
  {
    case 0:
      red   = 0x1F;
      green = amplit; // Green ramps up
      blue  = 0;
      break;
    case 1:
      red   = 0x1F - amplit; // Red ramps down
      green = 0x1F;
      blue  = 0;
      break;
    case 2:
      red   = 0;
      green = 0x1F;
      blue  = amplit; // Blue ramps up
      break;
    case 3:
      red   = 0;
      green = 0x1F - amplit; // Green ramps down
      blue  = 0x1F;
      break;
    case 4:
      red   = amplit; // Red ramps up
      green = 0;
      blue  = 0x1F;
      break;
    case 5:
      red   = 0x1F;
      green = 0;
      blue  = 0x1F - amplit; // Blue ramps down
      break;
  }
  return red << 11 | green << 6 | blue;
}
