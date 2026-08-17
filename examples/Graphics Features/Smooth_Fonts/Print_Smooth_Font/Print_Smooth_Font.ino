/*
  Sketch to demonstrate using the print class with smooth fonts,
  the Smooth fonts are stored in a FLASH program memory array.

  Sketch is written for a 240 x 320 display

  New font files in the .vlw format can be created using the Processing
  sketch in the library Tools folder. The Processing sketch can convert
  TrueType fonts in *.ttf or *.otf files.

  The library supports 16-bit Unicode characters:
  https://en.wikipedia.org/wiki/Unicode_font

  The characters supported are in the in the Basic Multilingual Plane:
  https://en.wikipedia.org/wiki/Plane_(Unicode)#Basic_Multilingual_Plane

  Make sure all the display driver and pin connections are correct by
  editing the User_Setup.h file in the TFT_eSPI library folder.
*/

//  The font is stored in an array within a sketch tab.

//  A processing sketch to create new fonts can be found in the Tools folder of TFT_eSPI
//  https://github.com/Bodmer/TFT_eSPI/tree/master/Tools/Create_Smooth_Font/Create_font

#include "Final_Frontier_28.h"

// Graphics and font library
#include <Seeed_GFX.h>

// -- Board & Driver: edit for your hardware --
#include "board/boards/Wio_Terminal.h"
#include "driver/tft/Driver_ILI9341.h"
#include "panel/Panel_TFT.h"
#include <SPI.h>

Seeed_GFX display;  // Invoke library

// Setup
void setup(void) {
  Serial.begin(115200); // Used for messages

  // -- Edit for your board/config --
    if (!display.begin<Board_Wio_Terminal, Config_Wio_Terminal_ILI9341>()) {
        Serial.println(display.lastResult().message);
        while (true) delay(1000);
    }
  // Wio Terminal's upright landscape orientation is rotation 3.
  display.setRotation(3);
}

// Main loop
void loop() {
  // Wrap test at right and bottom of screen
  display.setTextWrap(true, true);

  // Font and background colour, background colour is used for anti-alias blending
  display.setTextColor(TFT_WHITE, TFT_BLACK);

  // Load the font
  display.loadFont(Final_Frontier_28);

  // Display all characters of the font
  display.showFont(2000);

  // Set "cursor" at top left corner of display (0,0)
  // (cursor will move to next line automatically during printing with 'display.println'
  //  or stay on the line is there is room for the text with display.print)
  display.setCursor(0, 0);

  // Set the font colour to be white with a black background, set text size multiplier to 1
  display.setTextColor(TFT_WHITE, TFT_BLACK);

  // We can now plot text on screen using the "print" class
  display.println("Hello World!");

  // Set the font colour to be yellow
  display.setTextColor(TFT_YELLOW, TFT_BLACK);
  display.println(1234.56);

  // Set the font colour to be red
  display.setTextColor(TFT_RED, TFT_BLACK);
  display.println((uint32_t)3735928559, HEX); // Should print DEADBEEF

  // Set the font colour to be green with black background
  display.setTextColor(TFT_GREEN, TFT_BLACK);
  display.println("Anti-aliased font!");
  display.println("");

  // Test some print formatting functions
  float fnumber = 123.45;

  // Set the font colour to be blue
  display.setTextColor(TFT_BLUE, TFT_BLACK);
  display.print("Float = ");       display.println(fnumber);           // Print floating point number
  display.print("Binary = ");      display.println((int)fnumber, BIN); // Print as integer value in binary
  display.print("Hexadecimal = "); display.println((int)fnumber, HEX); // Print as integer number in Hexadecimal

  // Unload the font to recover used RAM
  display.unloadFont();

  delay(10000);
}
