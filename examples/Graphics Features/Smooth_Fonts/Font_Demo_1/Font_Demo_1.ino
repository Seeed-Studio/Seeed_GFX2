/*
  This sketch is the same as the Font_Demo_1 example, except the fonts in this
  example are in a FLASH (program memory) array. This means that processors
  such as the STM32 series that are not supported by a SPIFFS library can use
  smooth (anti-aliased) fonts.
*/

/*
  There are four different methods of plotting anti-aliased fonts to the screen.

  This sketch uses method 1, using display.print() and display.println() calls.

  In some cases the sketch shows what can go wrong too, so read the comments!
  
  The font is rendered WITHOUT a background, but a background colour needs to be
  set so the anti-aliasing of the character is performed correctly. This is because
  characters are drawn one by one.
  
  This method is good for static text that does not change often because changing
  values may flicker. The text appears at the tft cursor coordinates.

  It is also possible to "print" text directly into a created sprite, for example using
  spr.println("Hello"); and then push the sprite to the screen. That method is not
  demonstrated in this sketch.
  
*/

//  A processing sketch to create new fonts can be found in the Tools folder of TFT_eSPI
//  https://github.com/Bodmer/TFT_eSPI/tree/master/Tools/Create_Smooth_Font/Create_font

//  This sketch uses font files created from the Noto family of fonts:
//  https://www.google.com/get/noto/

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

  display.setTextColor(TFT_WHITE, TFT_BLACK); // Set the font colour AND the background colour
                                          // so the anti-aliasing works

  display.setCursor(0, 0); // Set cursor at top left of screen


  // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
  // Small font
  // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
  Serial.println("Loading font");

  display.loadFont(AA_FONT_SMALL);    // Must load the font first

  display.println("Small 15pt font"); // println moves cursor down for a new line

  display.println(); // New line

  display.print("ABC"); // print leaves cursor at end of line

  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.println("1234"); // Added to line after ABC

  display.setTextColor(TFT_YELLOW, TFT_BLACK);
  // print stream formatting can be used,see:
  // https://www.arduino.cc/en/Serial/Print
  int ivalue = 1234;
  display.println(ivalue);       // print as an ASCII-encoded decimal
  display.println(ivalue, DEC);  // print as an ASCII-encoded decimal
  display.println(ivalue, HEX);  // print as an ASCII-encoded hexadecimal
  display.println(ivalue, OCT);  // print as an ASCII-encoded octal
  display.println(ivalue, BIN);  // print as an ASCII-encoded binary

  display.println(); // New line
  display.setTextColor(TFT_MAGENTA, TFT_BLACK);
  float fvalue = 1.23456;
  display.println(fvalue, 0);  // no decimal places
  display.println(fvalue, 1);  // 1 decimal place
  display.println(fvalue, 2);  // 2 decimal places
  display.println(fvalue, 5);  // 5 decimal places

  delay(5000);

  // Get ready for the next demo while we have this font loaded
  display.fillScreen(TFT_BLACK);
  display.setCursor(0, 0); // Set cursor at top left of screen
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.println("Two clean ways to");
  display.println("print changing values...");

  display.unloadFont(); // Remove the font to recover memory used


  // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
  // Large font
  // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

  display.loadFont(AA_FONT_LARGE); // Load another different font

  //display.fillScreen(TFT_BLACK);
  
  // Keep the original two-row animation, but make both rows erase the
  // previous value cleanly instead of accumulating glyphs.
  for (int i = 0; i <= 99; i++)
  {
    display.setCursor(50, 50);
    display.setTextColor(TFT_GREEN, TFT_BLACK, true);
    display.print("      ");
    display.setCursor(50, 50);
    display.print(i / 10.0, 1);

    // Adding "true" fills each smooth-font character background.
    display.setTextColor(TFT_GREEN, TFT_BLACK, true);
    display.setCursor(50, 90);
    display.print(i / 10.0, 1);
    
    delay (200);
  }

  delay(5000);

  // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
  // Large font text wrapping
  // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

  display.fillScreen(TFT_BLACK);
  
  display.setTextColor(TFT_YELLOW, TFT_BLACK); // Change the font colour and the background colour

  display.setCursor(0, 0); // Set cursor at top left of screen

  display.println("Large font!");

  display.setTextWrap(true); // Wrap on width
  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.println("Long lines wrap to the next line");

  display.setTextWrap(false, false); // Wrap on width and height switched off
  display.setTextColor(TFT_MAGENTA, TFT_BLACK);
  display.println("Unless text wrap is switched off");

  display.unloadFont(); // Remove the font to recover memory used

  delay(8000);
}
