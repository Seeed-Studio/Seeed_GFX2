/**
 * @file   Color.h
 * @brief  Color type definitions and constants for Seeed_GFX v2.0
 *
 * Color definitions for 16-bit RGB565, 24-bit RGB888, grayscale,
 * and ePaper color modes (6-color, BWRY, multi-gray).
 */

#ifndef SEEED_GFX_COLOR_H
#define SEEED_GFX_COLOR_H

#include <stdint.h>

// Color type definitions

typedef uint16_t RGB565;   // 16-bit color: RRRRRGGGGGGBBBBB
typedef uint32_t RGB888;   // 24-bit color: RRRRRRRRGGGGGGGGBBBBBBBB
typedef uint8_t  Grayscale; // 8-bit grayscale

// Standard 16-bit (RGB565) Color Definitions

#define TFT_BLACK       0x0000  /*   0,   0,   0 */
#define TFT_NAVY        0x000F  /*   0,   0, 128 */
#define TFT_DARKGREEN   0x03E0  /*   0, 128,   0 */
#define TFT_DARKCYAN    0x03EF  /*   0, 128, 128 */
#define TFT_MAROON      0x7800  /* 128,   0,   0 */
#define TFT_PURPLE      0x780F  /* 128,   0, 128 */
#define TFT_OLIVE       0x7BE0  /* 128, 128,   0 */
#define TFT_LIGHTGREY   0xD69A  /* 211, 211, 211 */
#define TFT_DARKGREY    0x7BEF  /* 128, 128, 128 */
#define TFT_BLUE        0x001F  /*   0,   0, 255 */
#define TFT_GREEN       0x07E0  /*   0, 255,   0 */
#define TFT_CYAN        0x07FF  /*   0, 255, 255 */
#define TFT_RED         0xF800  /* 255,   0,   0 */
#define TFT_MAGENTA     0xF81F  /* 255,   0, 255 */
#define TFT_YELLOW      0xFFE0  /* 255, 255,   0 */
#define TFT_WHITE       0xFFFF  /* 255, 255, 255 */
#define TFT_ORANGE      0xFDA0  /* 255, 180,   0 */
#define TFT_GREENYELLOW 0xB7E0  /* 180, 255,   0 */
#define TFT_PINK        0xFE19  /* 255, 192, 203 */
#define TFT_BROWN       0x9A60  /* 150,  75,   0 */
#define TFT_GOLD        0xFEA0  /* 255, 215,   0 */
#define TFT_SILVER      0xC618  /* 192, 192, 192 */
#define TFT_SKYBLUE     0x867D  /* 135, 206, 235 */
#define TFT_VIOLET      0x915C  /* 180,  46, 226 */

// Transparent color marker (encodes/decodes to same 16-bit value)
#define TFT_TRANSPARENT 0x0120

// ePaper 6-Color Mode Definitions

#ifdef USE_COLORFULL_EPAPER
    #define TFT_EPD_BLACK   0x0F
    #define TFT_EPD_WHITE   0x00
    #define TFT_EPD_BLUE    0x0D
    #define TFT_EPD_YELLOW  0x0B
    #define TFT_EPD_GREEN   0x02
    #define TFT_EPD_RED     0x06
    // In the 6-color mode, other colors will be mapped to similar ones
    #define TFT_EPD_NAVY        TFT_EPD_BLUE
    #define TFT_EPD_DARKGREEN   TFT_EPD_GREEN
    #define TFT_EPD_DARKCYAN    TFT_EPD_GREEN
    #define TFT_EPD_MAROON      TFT_EPD_RED
    #define TFT_EPD_PURPLE      TFT_EPD_BLUE
    #define TFT_EPD_OLIVE       TFT_EPD_YELLOW
    #define TFT_EPD_LIGHTGREY   TFT_EPD_WHITE
    #define TFT_EPD_DARKGREY    TFT_EPD_BLACK
    #define TFT_EPD_CYAN        TFT_EPD_GREEN
    #define TFT_EPD_MAGENTA     TFT_EPD_RED
    #define TFT_EPD_ORANGE      TFT_EPD_YELLOW
    #define TFT_EPD_GREENYELLOW TFT_EPD_GREEN
    #define TFT_EPD_PINK        TFT_EPD_RED
    #define TFT_EPD_BROWN       TFT_EPD_YELLOW
    #define TFT_EPD_GOLD        TFT_EPD_YELLOW
    #define TFT_EPD_SILVER      TFT_EPD_WHITE
    #define TFT_EPD_SKYBLUE     TFT_EPD_BLUE
    #define TFT_EPD_VIOLET      TFT_EPD_BLUE
#endif

// ePaper BWRY (Black-White-Red-Yellow) Mode Definitions

#ifdef USE_BWRY_EPAPER
    #define TFT_EPD_WHITE   0x00
    #define TFT_EPD_YELLOW  0x0B
    #define TFT_EPD_RED     0x06
    #define TFT_EPD_BLACK   0x0F

    #define TFT_EPD_BLUE         TFT_EPD_YELLOW
    #define TFT_EPD_GREEN        TFT_EPD_YELLOW
    #define TFT_EPD_NAVY         TFT_EPD_BLUE
    #define TFT_EPD_DARKGREEN    TFT_EPD_GREEN
    #define TFT_EPD_DARKCYAN     TFT_EPD_GREEN
    #define TFT_EPD_MAROON       TFT_EPD_RED
    #define TFT_EPD_PURPLE       TFT_EPD_BLUE
    #define TFT_EPD_OLIVE        TFT_EPD_YELLOW
    #define TFT_EPD_LIGHTGREY    TFT_EPD_WHITE
    #define TFT_EPD_DARKGREY     TFT_EPD_BLACK
    #define TFT_EPD_CYAN         TFT_EPD_GREEN
    #define TFT_EPD_MAGENTA      TFT_EPD_RED
    #define TFT_EPD_ORANGE       TFT_EPD_YELLOW
    #define TFT_EPD_GREENYELLOW  TFT_EPD_GREEN
    #define TFT_EPD_PINK         TFT_EPD_RED
    #define TFT_EPD_BROWN        TFT_EPD_YELLOW
    #define TFT_EPD_GOLD         TFT_EPD_YELLOW
    #define TFT_EPD_SILVER       TFT_EPD_WHITE
    #define TFT_EPD_SKYBLUE      TFT_EPD_BLUE
    #define TFT_EPD_VIOLET       TFT_EPD_BLUE
#endif

// ePaper Multi-Gray Mode Definitions

#ifdef USE_MUTIGRAY_EPAPER
    #ifdef GRAY_LEVEL4
        #define TFT_GRAY_0  0x00
        #define TFT_GRAY_1  0x01
        #define TFT_GRAY_2  0x02
        #define TFT_GRAY_3  0x03
    #elif defined(GRAY_LEVEL16)
        #define TFT_GRAY_0  0x00
        #define TFT_GRAY_1  0x01
        #define TFT_GRAY_2  0x02
        #define TFT_GRAY_3  0x03
        #define TFT_GRAY_4  0x04
        #define TFT_GRAY_5  0x05
        #define TFT_GRAY_6  0x06
        #define TFT_GRAY_7  0x07
        #define TFT_GRAY_8  0x08
        #define TFT_GRAY_9  0x09
        #define TFT_GRAY_10 0x0A
        #define TFT_GRAY_11 0x0B
        #define TFT_GRAY_12 0x0C
        #define TFT_GRAY_13 0x0D
        #define TFT_GRAY_14 0x0E
        #define TFT_GRAY_15 0x0F
    #endif
#endif

// Color order constants

#define TFT_BGR 0   // Colour order Blue-Green-Red
#define TFT_RGB 1   // Colour order Red-Green-Blue

// Default 4-bit palette for sprites

#if defined(__AVR__)
#include <avr/pgmspace.h>
#elif defined(ARDUINO_ARCH_ESP8266) || defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
#include <pgmspace.h>
#else
#ifndef PROGMEM
#define PROGMEM
#endif
#endif

static const uint16_t default_4bit_palette[] PROGMEM = {
    TFT_BLACK,     //  0
    TFT_BROWN,     //  1
    TFT_RED,       //  2
    TFT_ORANGE,    //  3
    TFT_YELLOW,    //  4  Colours 0-9 follow the resistor colour code!
    TFT_GREEN,     //  5
    TFT_BLUE,      //  6
    TFT_PURPLE,    //  7
    TFT_DARKGREY,  //  8
    TFT_WHITE,     //  9
    TFT_CYAN,      // 10
    TFT_MAGENTA,   // 11
    TFT_MAROON,    // 12
    TFT_DARKGREEN, // 13
    TFT_NAVY,      // 14
    TFT_PINK       // 15
};

#endif // SEEED_GFX_COLOR_H