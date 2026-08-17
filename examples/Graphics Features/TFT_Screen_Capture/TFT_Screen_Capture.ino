/*
  TFT Screen Capture - Arduino-side server for Seeed_GFX2

  This sketch sends the Wio Terminal screen to the Processing client at
  tools/Screenshot_client/Screenshot_client.pde.

  Usage:
    1. Upload this sketch to a Wio Terminal.
    2. Open the Processing client and select the 921600-baud serial port.
    3. Run the client; screenshots are saved in its sketch folder.

  Screen capture requires controller RAM-read support and a physically
  connected read-data signal. Wio Terminal provides both through its built-in
  ILI9341 on LCD_SPI/SPI3; Seeed_GFX2 uses a 20 MHz read clock.

  Adapted from the original TFT_eSPI example by Bodmer.
*/

#include <Seeed_GFX.h>
#include "board/boards/Wio_Terminal.h"
#include "driver/tft/Driver_ILI9341.h"
#include "panel/Panel_TFT.h"

Seeed_GFX display;

#define RGB_TEST false

bool screenServer(void);
void rainbow_fill();

void setup() {
    Serial.begin(921600);

    if (!display.begin<Board_Wio_Terminal,
                       Config_Wio_Terminal_ILI9341>()) {
        Serial.println(display.lastResult().message);
        while (true) delay(1000);
    }

    if (!display.capabilities().readback) {
        Serial.println("Screen capture requires a readable display bus");
        while (true) delay(1000);
    }

    display.setRotation(3);
    display.fillScreen(TFT_BLACK);
    randomSeed(analogRead(A0));
}

void loop() {
    static uint32_t targetTime = 0;

    if (static_cast<int32_t>(millis() - targetTime) >= 0) {
        targetTime = millis() + 2000;

        if (RGB_TEST) {
            display.fillScreen(TFT_BLACK);
            display.fillRect(0, 0, 16, 16, TFT_RED);
            display.fillRect(16, 0, 16, 16, TFT_GREEN);
            display.fillRect(32, 0, 16, 16, TFT_BLUE);
        } else {
            display.setRotation(random(4));
            rainbow_fill();

            display.setTextColor(TFT_BLACK);
            display.setTextDatum(TC_DATUM);
            const int xpos = display.width() / 2;

            display.setTextFont(0);
            display.drawString("Original Adafruit font!", xpos, 5);
            display.drawString("Font size 2", xpos, 14, 2);
            display.drawString("Font size 4", xpos, 30, 4);
            display.drawString("12.34", xpos, 54, 6);
            display.drawString("12.34 is in font size 6", xpos, 92, 2);

            const float pi = 3.1415926f;
            const int ypos = 110;
            display.setTextDatum(TR_DATUM);
            display.drawFloat(pi, 3, xpos, ypos, 2);
            display.setTextDatum(TL_DATUM);
            display.drawString(" is pi", xpos, ypos, 2);

            display.setTextSize(1);
            display.setTextDatum(TC_DATUM);
            display.setTextColor(TFT_BLACK);
            display.drawString("Transparent...", xpos, 125, 4);
            display.setTextColor(TFT_WHITE, TFT_BLACK);
            display.drawString("White on black", xpos, 150, 4);

            display.setTextColor(TFT_GREEN, TFT_BLACK);
            display.setTextFont(2);
            display.setTextDatum(BC_DATUM);
            display.setTextPadding(display.width() + 1);
            const int bottom = display.height() - 10;
            display.drawString("Ode to a Small Lump of Green Putty", xpos, bottom - 32);
            display.drawString("I Found in My Armpit One Midsummer", xpos, bottom - 16);
            display.drawString("Morning", xpos, bottom);
            display.setTextDatum(TL_DATUM);
            display.setTextPadding(0);
        }

        screenServer();
    }
}

void rainbow_fill() {
    static byte red = 0x1F;
    static byte green = 0;
    static byte blue = 0;
    static byte state = 0;
    static uint16_t colour = red << 11;

    for (int i = display.height() - 1; i >= 0; --i) {
        switch (state) {
            case 0: green++; if (green == 64) { green = 63; state = 1; } break;
            case 1: red--;   if (red == 255)  { red = 0;    state = 2; } break;
            case 2: blue++;  if (blue == 32)  { blue = 31;  state = 3; } break;
            case 3: green--; if (green == 255){ green = 0;  state = 4; } break;
            case 4: red++;   if (red == 32)   { red = 31;   state = 5; } break;
            case 5: blue--;  if (blue == 255) { blue = 0;   state = 0; } break;
        }
        colour = red << 11 | green << 5 | blue;
        display.drawFastHLine(0, i, display.width(), colour);
    }
}
