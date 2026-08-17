/*
  This sketch is the same as the Font_Demo_2 example, except the fonts in this
  example are in a FLASH (program memory) array. This means that processors
  such as the STM32 series that are not supported by a SPIFFS library can use
  smooth (anti-aliased) fonts.
*/

/*
  There are four different methods of plotting anti-aliased fonts to the screen.

  This sketch uses method 2, using graphics calls plotting direct to the TFT:
    display.drawString(string, x, y);
    display.drawNumber(integer, x, y);
    display.drawFloat(float, dp, x, y); // dp = number of decimal places

  setTextDatum() and setTextPadding() functions work with those draw functions.

  This method is good for static text that does not change often because changing
  values may flicker.
  
*/

// A processing sketch to create new fonts can be found in the Tools folder of TFT_eSPI
// https://github.com/Bodmer/TFT_eSPI/tree/master/Tools/Create_Smooth_Font/Create_font

// This sketch uses font files created from the Noto family of fonts:
// https://www.google.com/get/noto/

#include "NotoSansBold15.h"
#include "NotoSansBold36.h"

// The font names are arrays references, thus must NOT be in quotes ""
#define AA_FONT_SMALL NotoSansBold15
#define AA_FONT_LARGE NotoSansBold36

#include <SPI.h>
#include <Seeed_GFX.h>

// -- Board & Driver: edit for your hardware --
#include "board/boards/Wio_Terminal.h"
#include "driver/tft/Driver_ILI9341.h"
#include "panel/Panel_TFT.h"       // Hardware-specific library

Seeed_GFX display;

void drawDatumMarker(int x, int y);

void setup(void) {

  Serial.begin(250000);

  // -- Edit for your board/config --
    if (!display.begin<Board_Wio_Terminal, Config_Wio_Terminal_ILI9341>()) {
        Serial.println(display.lastResult().message);
        while (true) delay(1000);
    }

  // Wio Terminal's upright landscape orientation is rotation 3.
  display.setRotation(3);
}

void loop() {

  display.fillScreen(TFT_BLACK);

  display.setTextColor(TFT_WHITE, TFT_BLACK); // Set the font colour and the background colour

  display.setTextDatum(TC_DATUM); // Top Centre datum

  int xpos = display.width() / 2; // Half the screen width
  int ypos = 10;


  // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
  // Small font
  // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

  display.loadFont(AA_FONT_SMALL); // Must load the font first

  display.drawString("Small 15pt font", xpos, ypos);

  ypos += display.fontHeight();   // Get the font height and move ypos down

  display.setTextColor(TFT_GREEN, TFT_BLACK);

  // If the string does not fit the screen width, then the next character will wrap to a new line
  display.drawString("Ode To A Small Lump Of Green Putty I Found In My Armpit One Midsummer Morning", xpos, ypos);

  display.setTextColor(TFT_GREEN, TFT_BLUE); // Background colour does not match the screen background!
  display.drawString("Anti-aliasing causes odd looking shadow effects if the text and screen background colours are not the same!", xpos, ypos + 60);

  display.unloadFont(); // Remove the font to recover memory used

  delay(5000);

  // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
  // Large font
  // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

  display.loadFont(AA_FONT_LARGE); // Load another different font

  display.fillScreen(TFT_BLACK);

  // The "true" parameter forces background drawing for smooth fonts
  display.setTextColor(TFT_GREEN, TFT_BLUE, true); // Change the font colour and the background colour

  display.drawString("36pt font", xpos, ypos);

  ypos += display.fontHeight();  // Get the font height and move ypos down

  // Set text padding to 100 pixels wide area to over-write old values on screen
  display.setTextPadding(100);

  // Draw changing numbers - likely to flicker using this plot method!
  for (int i = 0; i <= 99; i++) {
    display.drawFloat(i / 10.0, 1, xpos, ypos);
    delay (200);
  }

  // Turn off text padding by setting value to 0
  display.setTextPadding(0);

  display.unloadFont(); // Remove the font to recover memory used

  delay(5000);

  // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
  // Setting the 12 datum positions works with free fonts
  // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

  // Integer numbers, floats and strings can be drawn relative to a x,y datum, e.g.:
  // display.drawNumber( 123, x, y);
  // display.drawFloat( 1.23, dp, x, y); // Where dp is number of decimal places to show
  // display.drawString( "Abc", x, y);

  display.fillScreen(TFT_BLACK);

  display.setTextColor(TFT_DARKGREY, TFT_BLACK, false);

  // Use middle of screen as datum
  xpos = display.width() /2;
  ypos = display.height()/2;

  display.loadFont(AA_FONT_SMALL);
  display.setTextDatum(TL_DATUM);
  display.drawString("[Top left]", xpos, ypos);
  drawDatumMarker(xpos, ypos);
  delay(1000);

  display.fillScreen(TFT_BLACK);
  display.setTextDatum(TC_DATUM);
  display.drawString("[Top centre]", xpos, ypos);
  drawDatumMarker(xpos, ypos);
  delay(1000);

  display.fillScreen(TFT_BLACK);
  display.setTextDatum(TR_DATUM);
  display.drawString("[Top right]", xpos, ypos);
  drawDatumMarker(xpos, ypos);
  delay(1000);

  display.fillScreen(TFT_BLACK);
  display.setTextDatum(ML_DATUM);
  display.drawString("[Middle left]", xpos, ypos);
  drawDatumMarker(xpos, ypos);
  delay(1000);

  display.fillScreen(TFT_BLACK);
  display.setTextDatum(MC_DATUM);
  display.drawString("[Middle centre]", xpos, ypos);
  drawDatumMarker(xpos, ypos);
  delay(1000);

  display.fillScreen(TFT_BLACK);
  display.setTextDatum(MR_DATUM);
  display.drawString("[Middle right]", xpos, ypos);
  drawDatumMarker(xpos, ypos);
  delay(1000);

  display.fillScreen(TFT_BLACK);
  display.setTextDatum(BL_DATUM);
  display.drawString("[Bottom left]", xpos, ypos);
  drawDatumMarker(xpos, ypos);
  delay(1000);

  display.fillScreen(TFT_BLACK);
  display.setTextDatum(BC_DATUM);
  display.drawString("[Bottom centre]", xpos, ypos);
  drawDatumMarker(xpos, ypos);
  delay(1000);

  display.fillScreen(TFT_BLACK);
  display.setTextDatum(BR_DATUM);
  display.drawString("[Bottom right]", xpos, ypos);
  drawDatumMarker(xpos, ypos);
  delay(1000);

  display.fillScreen(TFT_BLACK);
  display.setTextDatum(L_BASELINE);
  display.drawString("[Left baseline]", xpos, ypos);
  drawDatumMarker(xpos, ypos);
  delay(1000);

  display.fillScreen(TFT_BLACK);
  display.setTextDatum(C_BASELINE);
  display.drawString("[Centre baseline]", xpos, ypos);
  drawDatumMarker(xpos, ypos);
  delay(1000);

  display.fillScreen(TFT_BLACK);
  display.setTextDatum(R_BASELINE);
  display.drawString("[Right baseline]", xpos, ypos);
  drawDatumMarker(xpos, ypos);
  delay(1000);

  display.unloadFont(); // Remove the font to recover memory used

  delay(4000);

}

// Draw a + mark centred on x,y
void drawDatumMarker(int x, int y)
{
  display.drawLine(x - 5, y, x + 5, y, TFT_GREEN);
  display.drawLine(x, y - 5, x, y + 5, TFT_GREEN);
}
