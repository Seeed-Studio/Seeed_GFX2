#ifndef SEEED_GFX_BUTTON_H
#define SEEED_GFX_BUTTON_H

#include <Arduino.h>
#include "../core/Font.h"

class Seeed_GFX;

/** Sketch-compatible, stateful button widget modeled after TFT_eSPI_Button. */
class Seeed_GFX_Button {
public:
    Seeed_GFX_Button();

    void initButton(Seeed_GFX* gfx, int16_t x, int16_t y, uint16_t w, uint16_t h,
                    uint16_t outline, uint16_t fill, uint16_t textcolor,
                    char* label, uint8_t textsize);
    void initButtonUL(Seeed_GFX* gfx, int16_t x, int16_t y, uint16_t w, uint16_t h,
                      uint16_t outline, uint16_t fill, uint16_t textcolor,
                      char* label, uint8_t textsize);
    void setLabelDatum(int16_t xDelta, int16_t yDelta, uint8_t datum = MC_DATUM);
    void drawButton(bool inverted = false, String longName = "");
    bool contains(int16_t x, int16_t y) const;
    void press(bool pressed) { laststate = currstate; currstate = pressed; }
    bool isPressed() const { return currstate; }
    bool justPressed() const { return currstate && !laststate; }
    bool justReleased() const { return !currstate && laststate; }

private:
    Seeed_GFX* _gfx;
    int16_t _x, _y, _xd, _yd;
    uint16_t _w, _h, _outline, _fill, _text;
    uint8_t _textsize, _datum;
    String _label;
    bool currstate, laststate;
};

// Allows sketches that used the legacy class to migrate without a widget rewrite.
using TFT_eSPI_Button = Seeed_GFX_Button;

#endif
