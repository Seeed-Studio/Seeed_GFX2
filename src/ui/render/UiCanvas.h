#ifndef SEEED_UI_CANVAS_H
#define SEEED_UI_CANVAS_H

#include <stddef.h>
#include "../UiConfig.h"
#include "../UiTypes.h"

class Seeed_GFX;

class UiCanvas {
public:
    explicit UiCanvas(Seeed_GFX& gfx) : _gfx(gfx) {}

    bool save();
    bool restore();
    void clipRect(const UiRect& rect);
    void resetClip();

    void fillRect(const UiRect& rect, uint16_t color);
    void drawRect(const UiRect& rect, uint16_t color, int16_t thickness = 1);
    void drawText(int16_t x, int16_t y, const char* text, uint16_t color,
                  uint8_t size = 1);
    void drawImage565(const UiRect& destination, const uint16_t* pixels);

    Seeed_GFX& gfx() { return _gfx; }

private:
    struct State {
        int32_t viewportX, viewportY, viewportW, viewportH;
        int32_t originX, originY;
        uint32_t textColor, textBackground;
        uint8_t textSize, textFont, textDatum;
        bool viewportDatum;
    };

    Seeed_GFX& _gfx;
    State _states[SEEED_UI_CANVAS_STACK_DEPTH] = {};
    size_t _depth = 0;
};

#endif
