// Reads a screen image off the TFT and sends it to a Processing client sketch
// over the serial port. Use a high baud rate, e.g. 921600.

// At 921600 baud a 320 x 240 image with 16-bit colour transfers in ~1.67s
// and 24-bit colour in ~2.5s which is close to the theoretical minimum.

// Adapted for Seeed_GFX v2.0 from the original TFT_eSPI screen server
// by Bodmer (https://github.com/Bodmer/TFT_eSPI).

// Created by: Bodmer 27/1/17, updated 23/11/18
// Adapted for Seeed_GFX2: 2025

#include <Seeed_GFX.h>

extern Seeed_GFX display;

//                                  Definitions
#define PIXEL_TIMEOUT 100     // 100ms Time-out between pixel requests
#define START_TIMEOUT 10000   // 10s Maximum time to wait at start transfer

#define BITS_PER_PIXEL 16     // 24 for RGB colour format, 16 for 565 colour format

// File names must be alpha-numeric characters (0-9, a-z, A-Z) or "/" underscore "_"
// other ascii characters are stripped out by client
#define DEFAULT_FILENAME "tft_screenshots/screenshot"
#define FILE_TYPE "png"       // jpg, bmp, png, tif are valid

// Filename extension
// '#' = add incrementing number, '@' = add timestamp, '%' add millis() timestamp,
// '*' = add nothing
#define FILE_EXT  '@'

// Number of pixels to send in a burst (minimum of 1), no benefit above 8
// NPIXELS 1 = use readPixel() => slow, 16-bit pixels only
// NPIXELS >1 uses readRect()  2 = 1.75s, 4 = 1.68s, 8 = 1.67s
#define NPIXELS 8

bool screenServer(String filename);
bool serialScreenServer(String filename);
void sendParameters(String filename);

//                           Screen server call with no filename
bool screenServer(void)
{
    return screenServer(DEFAULT_FILENAME);
}

//                           Screen server call with filename
bool screenServer(String filename)
{
    delay(0); // yield() for ESP8266

    bool result = serialScreenServer(filename);

    delay(0);

    return result;
}

//                Serial server function that sends the data to the client
bool serialScreenServer(String filename)
{
    // Precautionary receive buffer garbage flush for 50ms
    uint32_t clearTime = millis() + 50;
    while (millis() < clearTime && Serial.read() >= 0) delay(0);

    bool wait = true;
    uint32_t lastCmdTime = millis();

    // Wait for the starting flag with a start time-out
    while (wait)
    {
        delay(0);
        if (Serial.available() > 0) {
            uint8_t cmd = Serial.read();
            if (cmd == 'S') {
                clearTime = millis() + 50;
                while (millis() < clearTime && Serial.read() >= 0) delay(0);

                wait = false;
                lastCmdTime = millis();

                // Send screen size etc. using a simple header with delimiters
                sendParameters(filename);
            }
        }
        else
        {
            if (millis() - lastCmdTime > START_TIMEOUT) return false;
        }
    }

    uint8_t color[3 * NPIXELS]; // RGB and 565 format color buffer for N pixels

    const uint32_t screenWidth = static_cast<uint32_t>(display.width());
    const uint32_t screenHeight = static_cast<uint32_t>(display.height());

    // Send all the pixels on the whole screen
    for (uint32_t y = 0; y < screenHeight; y++)
    {
        for (uint32_t x = 0; x < screenWidth; x += NPIXELS)
        {
            const uint32_t remaining = screenWidth - x;
            const uint32_t chunkPixels = min((uint32_t)NPIXELS, remaining);
            delay(0);

            // Wait here for serial data to arrive or a time-out elapses
            while (Serial.available() == 0)
            {
                if (millis() - lastCmdTime > PIXEL_TIMEOUT) return false;
                delay(0);
            }

            // Serial data must be available to get here
            if (Serial.read() == 'X') {
                // X command byte means abort
                clearTime = millis() + 50;
                while (millis() < clearTime && Serial.read() >= 0) delay(0);
                return false;
            }
            lastCmdTime = millis();

#if defined BITS_PER_PIXEL && BITS_PER_PIXEL >= 24 && NPIXELS > 1
            // Fetch N RGB pixels from x,y and put in buffer
            display.readRectRGB(x, y, chunkPixels, 1, color);
            Serial.write(color, 3 * chunkPixels);
#else
            // Fetch N 565 format pixels from x,y and put in buffer
            uint16_t pixels[NPIXELS];
            if (NPIXELS > 1) {
                display.readRect(x, y, chunkPixels, 1, pixels);
            } else {
                pixels[0] = display.readPixel(x, y);
            }
            // The serial protocol is big-endian RGB565 regardless of the
            // MCU's native uint16_t byte order.
            for (uint32_t i = 0; i < chunkPixels; ++i) {
                color[i * 2] = pixels[i] >> 8;
                color[i * 2 + 1] = pixels[i] & 0xFF;
            }
            Serial.write(color, 2 * chunkPixels);
#endif
        }
    }

    Serial.flush();

    return true;
}

//    Send screen size etc. using a simple header with delimiters for client checks
void sendParameters(String filename)
{
    Serial.write('W'); // Width
    Serial.write(display.width()  >> 8);
    Serial.write(display.width()  & 0xFF);

    Serial.write('H'); // Height
    Serial.write(display.height() >> 8);
    Serial.write(display.height() & 0xFF);

    Serial.write('Y'); // Bits per pixel (16 or 24)
    if (NPIXELS > 1) Serial.write(BITS_PER_PIXEL);
    else Serial.write(16); // readPixel() only provides 16-bit values

    Serial.write('?'); // Filename next
    Serial.print(filename);

    Serial.write('.'); // End of filename marker

    Serial.write(FILE_EXT); // Filename extension identifier

    Serial.write(*FILE_TYPE); // First character defines file type j,b,p,t
}
