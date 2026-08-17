#include "Button.h"
#include "../Seeed_GFX.h"

Seeed_GFX_Button::Seeed_GFX_Button()
    : _gfx(nullptr), _x(0), _y(0), _xd(0), _yd(0), _w(0), _h(0),
      _outline(TFT_WHITE), _fill(TFT_BLACK), _text(TFT_WHITE),
      _textsize(1), _datum(MC_DATUM), currstate(false), laststate(false) {}

void Seeed_GFX_Button::initButton(Seeed_GFX* gfx, int16_t x, int16_t y,
                                  uint16_t w, uint16_t h, uint16_t outline,
                                  uint16_t fill, uint16_t textcolor,
                                  char* label, uint8_t textsize) {
    initButtonUL(gfx, x - static_cast<int16_t>(w / 2), y - static_cast<int16_t>(h / 2),
                 w, h, outline, fill, textcolor, label, textsize);
}

void Seeed_GFX_Button::initButtonUL(Seeed_GFX* gfx, int16_t x, int16_t y,
                                    uint16_t w, uint16_t h, uint16_t outline,
                                    uint16_t fill, uint16_t textcolor,
                                    char* label, uint8_t textsize) {
    _gfx = gfx; _x = x; _y = y; _w = w; _h = h;
    _outline = outline; _fill = fill; _text = textcolor;
    _textsize = textsize ? textsize : 1;
    _label = label ? label : "";
    _xd = _yd = 0; _datum = MC_DATUM;
    currstate = laststate = false;
}

void Seeed_GFX_Button::setLabelDatum(int16_t xDelta, int16_t yDelta, uint8_t datum) {
    _xd = xDelta; _yd = yDelta; _datum = datum;
}

void Seeed_GFX_Button::drawButton(bool inverted, String longName) {
    if (!_gfx || !_w || !_h) return;
    const uint16_t fill = inverted ? _text : _fill;
    const uint16_t text = inverted ? _fill : _text;
    const int16_t radius = static_cast<int16_t>((_w < _h ? _w : _h) / 4);
    _gfx->fillRoundRect(_x, _y, _w, _h, radius, fill);
    _gfx->drawRoundRect(_x, _y, _w, _h, radius, _outline);
    const uint8_t priorDatum = _gfx->getTextDatum();
    const uint8_t priorSize = _gfx->textsize;
    const uint32_t priorColor = _gfx->textcolor;
    _gfx->setTextDatum(_datum);
    _gfx->setTextSize(_textsize);
    _gfx->setTextColor(text);
    const String& label = longName.length() ? longName : _label;
    _gfx->drawString(label, _x + static_cast<int16_t>(_w / 2) + _xd,
                     _y + static_cast<int16_t>(_h / 2) + _yd);
    _gfx->setTextColor(static_cast<uint16_t>(priorColor));
    _gfx->setTextSize(priorSize);
    _gfx->setTextDatum(priorDatum);
}

bool Seeed_GFX_Button::contains(int16_t x, int16_t y) const {
    return x >= _x && y >= _y && x < _x + static_cast<int16_t>(_w) &&
           y < _y + static_cast<int16_t>(_h);
}
