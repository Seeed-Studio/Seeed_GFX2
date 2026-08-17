#include "ClassicWidgets.h"
#include "../Seeed_GFX.h"
#include <math.h>

namespace {
int32_t clamp32(int32_t value, int32_t low, int32_t high) {
    if (low > high) { const int32_t swap = low; low = high; high = swap; }
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

uint8_t lineCode(int32_t x, int32_t y, int32_t left, int32_t top,
                 int32_t right, int32_t bottom) {
    uint8_t code = 0;
    if (x < left) code |= 1;
    else if (x > right) code |= 2;
    if (y < top) code |= 4;
    else if (y > bottom) code |= 8;
    return code;
}

bool clipLine(int16_t& x0, int16_t& y0, int16_t& x1, int16_t& y1,
              int16_t left, int16_t top, int16_t right, int16_t bottom) {
    int32_t ax = x0, ay = y0, bx = x1, by = y1;
    uint8_t aCode = lineCode(ax, ay, left, top, right, bottom);
    uint8_t bCode = lineCode(bx, by, left, top, right, bottom);
    while (true) {
        if (!(aCode | bCode)) {
            x0 = static_cast<int16_t>(ax); y0 = static_cast<int16_t>(ay);
            x1 = static_cast<int16_t>(bx); y1 = static_cast<int16_t>(by);
            return true;
        }
        if (aCode & bCode) return false;
        const uint8_t outside = aCode ? aCode : bCode;
        int32_t x = 0, y = 0;
        if (outside & 8) {
            if (by == ay) return false;
            x = ax + (bx - ax) * (bottom - ay) / (by - ay); y = bottom;
        } else if (outside & 4) {
            if (by == ay) return false;
            x = ax + (bx - ax) * (top - ay) / (by - ay); y = top;
        } else if (outside & 2) {
            if (bx == ax) return false;
            y = ay + (by - ay) * (right - ax) / (bx - ax); x = right;
        } else {
            if (bx == ax) return false;
            y = ay + (by - ay) * (left - ax) / (bx - ax); x = left;
        }
        if (outside == aCode) {
            ax = x; ay = y; aCode = lineCode(ax, ay, left, top, right, bottom);
        } else {
            bx = x; by = y; bCode = lineCode(bx, by, left, top, right, bottom);
        }
    }
}
}

Seeed_ButtonWidget::Seeed_ButtonWidget(Seeed_GFX* gfx)
    : _gfx(gfx), _x(0), _y(0), _w(0), _h(0), _outline(TFT_WHITE),
      _fill(TFT_BLACK), _text(TFT_WHITE), _textSize(1), _current(false),
      _previous(false), _pressTime(0), _releaseTime(0),
      _pressAction(nullptr), _releaseAction(nullptr) {}

void Seeed_ButtonWidget::initButtonUL(int16_t x, int16_t y, uint16_t w,
                                      uint16_t h, uint16_t outline,
                                      uint16_t fill, uint16_t text,
                                      const char* label, uint8_t textSize) {
    _x = x; _y = y; _w = w; _h = h; _outline = outline; _fill = fill;
    _text = text; _label = label ? label : "";
    _textSize = textSize ? textSize : 1;
    _current = _previous = false;
}

void Seeed_ButtonWidget::draw(bool inverted, uint8_t outlineWidth,
                              uint16_t surrounding, const char* label) {
    if (!_gfx || !_w || !_h) return;
    const uint16_t fill = inverted ? _text : _fill;
    const uint16_t text = inverted ? _fill : _text;
    const int16_t radius = static_cast<int16_t>((_w < _h ? _w : _h) / 4U);
    if (!outlineWidth) outlineWidth = 1;
    const uint8_t maxOutline = static_cast<uint8_t>((_w < _h ? _w : _h) / 2U);
    if (outlineWidth > maxOutline) outlineWidth = maxOutline;
    _gfx->fillSmoothRoundRect(_x, _y, _w, _h, radius,
                              _outline, surrounding);
    if (_w > 2U * outlineWidth && _h > 2U * outlineWidth) {
        const int16_t innerRadius = radius > outlineWidth
            ? radius - outlineWidth : 0;
        _gfx->fillSmoothRoundRect(_x + outlineWidth, _y + outlineWidth,
                                  _w - 2U * outlineWidth,
                                  _h - 2U * outlineWidth,
                                  innerRadius, fill, _outline);
    }
    const uint8_t oldDatum = _gfx->getTextDatum();
    const uint8_t oldSize = _gfx->textsize;
    const uint32_t oldColor = _gfx->textcolor;
    const uint32_t oldBackground = _gfx->textbgcolor;
    _gfx->setTextDatum(MC_DATUM);
    _gfx->setTextSize(_textSize);
    _gfx->setTextColor(text, fill);
    _gfx->drawString(label ? label : _label.c_str(),
                     _x + static_cast<int16_t>(_w / 2U),
                     _y + static_cast<int16_t>(_h / 2U));
    _gfx->setTextColor(oldColor, oldBackground);
    _gfx->setTextSize(oldSize);
    _gfx->setTextDatum(oldDatum);
}

bool Seeed_ButtonWidget::contains(int16_t x, int16_t y) const {
    return x >= _x && y >= _y && x < _x + static_cast<int16_t>(_w) &&
           y < _y + static_cast<int16_t>(_h);
}

void Seeed_ButtonWidget::press(bool pressed) {
    _previous = _current;
    _current = pressed;
    if (justPressed()) _pressTime = millis();
    if (justReleased()) _releaseTime = millis();
}

Seeed_SliderWidget::Seeed_SliderWidget(Seeed_GFX* gfx)
    : _gfx(gfx), _x(0), _y(0), _w(0), _h(0), _value(0),
      _configured(false) {}

bool Seeed_SliderWidget::draw(int16_t x, int16_t y,
                              const Seeed_SliderConfig& config) {
    if (!_gfx || !config.slotLength || config.minimum == config.maximum) return false;
    _config = config; _x = x; _y = y;
    if (_config.orientation == Seeed_SliderOrientation::Horizontal) {
        _w = _config.slotLength;
        _h = _config.knobHeight > _config.slotWidth ? _config.knobHeight : _config.slotWidth;
    } else {
        _w = _config.knobWidth > _config.slotWidth ? _config.knobWidth : _config.slotWidth;
        _h = _config.slotLength;
    }
    _configured = true;
    _value = clampValue(_config.startValue);
    render();
    return true;
}

int32_t Seeed_SliderWidget::clampValue(int32_t value) const {
    return clamp32(value, _config.minimum, _config.maximum);
}

int16_t Seeed_SliderWidget::knobCenter() const {
    const int32_t range = _config.maximum - _config.minimum;
    const int32_t length = (_config.orientation == Seeed_SliderOrientation::Horizontal)
        ? _w : _h;
    if (!range || length <= 1) return 0;
    return static_cast<int16_t>((static_cast<int64_t>(_value - _config.minimum) *
                                 (length - 1)) / range);
}

void Seeed_SliderWidget::render() {
    if (!_gfx || !_configured) return;
    _gfx->fillRect(_x, _y, _w, _h, _config.backgroundColor);
    const int16_t center = knobCenter();
    if (_config.orientation == Seeed_SliderOrientation::Horizontal) {
        const int16_t slotY = _y + static_cast<int16_t>((_h - _config.slotWidth) / 2U);
        _gfx->fillSmoothRoundRect(_x, slotY, _w, _config.slotWidth,
                                  _config.slotWidth / 2U,
                                  _config.slotColor, _config.backgroundColor);
        int16_t knobX = _x + center - static_cast<int16_t>(_config.knobWidth / 2U);
        if (knobX < _x) knobX = _x;
        if (knobX + _config.knobWidth > _x + _w)
            knobX = _x + _w - _config.knobWidth;
        const int16_t knobY = _y + static_cast<int16_t>((_h - _config.knobHeight) / 2U);
        _gfx->fillSmoothRoundRect(knobX, knobY, _config.knobWidth,
                                  _config.knobHeight, _config.knobRadius,
                                  _config.knobColor, _config.backgroundColor);
        _gfx->drawFastVLine(knobX + _config.knobWidth / 2U, knobY + 3,
                            _config.knobHeight > 6 ? _config.knobHeight - 6 : 1,
                            _config.markerColor);
    } else {
        const int16_t slotX = _x + static_cast<int16_t>((_w - _config.slotWidth) / 2U);
        _gfx->fillSmoothRoundRect(slotX, _y, _config.slotWidth, _h,
                                  _config.slotWidth / 2U,
                                  _config.slotColor, _config.backgroundColor);
        const int16_t knobX = _x + static_cast<int16_t>((_w - _config.knobWidth) / 2U);
        int16_t knobY = _y + center - static_cast<int16_t>(_config.knobHeight / 2U);
        if (knobY < _y) knobY = _y;
        if (knobY + _config.knobHeight > _y + _h)
            knobY = _y + _h - _config.knobHeight;
        _gfx->fillSmoothRoundRect(knobX, knobY, _config.knobWidth,
                                  _config.knobHeight, _config.knobRadius,
                                  _config.knobColor, _config.backgroundColor);
        _gfx->drawFastHLine(knobX + 3, knobY + _config.knobHeight / 2U,
                            _config.knobWidth > 6 ? _config.knobWidth - 6 : 1,
                            _config.markerColor);
    }
}

bool Seeed_SliderWidget::setValue(int32_t value, bool redraw) {
    if (!_configured) return false;
    value = clampValue(value);
    if (_value == value) return false;
    _value = value;
    if (redraw) render();
    return true;
}

bool Seeed_SliderWidget::contains(int16_t x, int16_t y) const {
    const int16_t margin = 12;
    return _configured && x >= _x - margin && y >= _y - margin &&
           x < _x + static_cast<int16_t>(_w) + margin &&
           y < _y + static_cast<int16_t>(_h) + margin;
}

bool Seeed_SliderWidget::checkTouch(int16_t x, int16_t y) {
    if (!contains(x, y)) return false;
    const int32_t length = (_config.orientation == Seeed_SliderOrientation::Horizontal)
        ? _w : _h;
    int32_t pixel = (_config.orientation == Seeed_SliderOrientation::Horizontal)
        ? x - _x : y - _y;
    pixel = clamp32(pixel, 0, length - 1);
    const int32_t value = _config.minimum + static_cast<int32_t>(
        (static_cast<int64_t>(pixel) * (_config.maximum - _config.minimum)) /
        (length - 1));
    setValue(value);
    return true;
}

void Seeed_SliderWidget::getBounds(int16_t* x, int16_t* y,
                                   uint16_t* w, uint16_t* h) const {
    if (x) *x = _x;
    if (y) *y = _y;
    if (w) *w = _w;
    if (h) *h = _h;
}

Seeed_GraphWidget::Seeed_GraphWidget(Seeed_GFX* gfx)
    : _gfx(gfx), _x(0), _y(0), _width(0), _height(0),
      _background(TFT_BLACK), _gridColor(TFT_DARKGREY),
      _xLow(0), _xHigh(1), _yLow(0), _yHigh(1),
      _xGridStart(0), _xGridStep(0), _yGridStart(0), _yGridStep(0),
      _configured(false) {}

bool Seeed_GraphWidget::create(uint16_t width, uint16_t height,
                               uint16_t background) {
    if (!_gfx || width < 2 || height < 2) return false;
    _width = width; _height = height; _background = background;
    _configured = true;
    return true;
}

void Seeed_GraphWidget::setScale(float xLow, float xHigh,
                                 float yLow, float yHigh) {
    if (xLow == xHigh || yLow == yHigh) return;
    _xLow = xLow; _xHigh = xHigh; _yLow = yLow; _yHigh = yHigh;
}

void Seeed_GraphWidget::setGrid(float xStart, float xStep, float yStart,
                                float yStep, uint16_t color) {
    _xGridStart = xStart; _xGridStep = xStep;
    _yGridStart = yStart; _yGridStep = yStep; _gridColor = color;
}

int16_t Seeed_GraphWidget::pointX(float value) const {
    if (!_configured || _xHigh == _xLow) return _x;
    return static_cast<int16_t>(_x + (value - _xLow) * (_width - 1) /
                                (_xHigh - _xLow));
}

int16_t Seeed_GraphWidget::pointY(float value) const {
    if (!_configured || _yHigh == _yLow) return _y;
    return static_cast<int16_t>(_y + (_yHigh - value) * (_height - 1) /
                                (_yHigh - _yLow));
}

bool Seeed_GraphWidget::containsPixel(int16_t x, int16_t y) const {
    return _configured && x >= _x && y >= _y &&
           x < _x + static_cast<int16_t>(_width) &&
           y < _y + static_cast<int16_t>(_height);
}

void Seeed_GraphWidget::draw(int16_t x, int16_t y) {
    if (!_gfx || !_configured) return;
    _x = x; _y = y;
    _gfx->fillRect(_x, _y, _width, _height, _background);
    _gfx->drawRect(_x, _y, _width, _height, _gridColor);
    if (_xGridStep != 0) {
        float value = _xGridStart;
        for (uint16_t i = 0; i < 512; ++i, value += _xGridStep) {
            if ((_xGridStep > 0 && value > _xHigh) ||
                (_xGridStep < 0 && value < _xHigh)) break;
            if (value < _xLow && _xGridStep > 0) continue;
            if (value > _xLow && _xGridStep < 0) continue;
            const int16_t px = pointX(value);
            if (px >= _x && px < _x + static_cast<int16_t>(_width))
                _gfx->drawFastVLine(px, _y, _height, _gridColor);
        }
    }
    if (_yGridStep != 0) {
        float value = _yGridStart;
        for (uint16_t i = 0; i < 512; ++i, value += _yGridStep) {
            if ((_yGridStep > 0 && value > _yHigh) ||
                (_yGridStep < 0 && value < _yHigh)) break;
            if (value < _yLow && _yGridStep > 0) continue;
            if (value > _yLow && _yGridStep < 0) continue;
            const int16_t py = pointY(value);
            if (py >= _y && py < _y + static_cast<int16_t>(_height))
                _gfx->drawFastHLine(_x, py, _width, _gridColor);
        }
    }
}

Seeed_TraceWidget::Seeed_TraceWidget(Seeed_GraphWidget* graph)
    : _graph(graph), _color(TFT_WHITE), _lastX(0), _lastY(0),
      _hasPoint(false) {}

void Seeed_TraceWidget::start(uint16_t color) {
    _color = color; _hasPoint = false;
}

bool Seeed_TraceWidget::addPoint(float x, float y) {
    if (!_graph || !_graph->_gfx || !_graph->_configured) return false;
    const int16_t px = _graph->pointX(x), py = _graph->pointY(y);
    if (!_hasPoint) {
        if (_graph->containsPixel(px, py)) _graph->_gfx->drawPixel(px, py, _color);
        _lastX = px; _lastY = py; _hasPoint = true;
        return true;
    }
    int16_t x0 = _lastX, y0 = _lastY, x1 = px, y1 = py;
    _lastX = px; _lastY = py;
    if (!clipLine(x0, y0, x1, y1, _graph->_x, _graph->_y,
                  _graph->_x + _graph->_width - 1,
                  _graph->_y + _graph->_height - 1)) return true;
    _graph->_gfx->drawLine(x0, y0, x1, y1, _color);
    return true;
}

Seeed_MeterWidget::Seeed_MeterWidget(Seeed_GFX* gfx)
    : _gfx(gfx), _x(0), _y(0), _width(0), _height(0), _fullScale(100),
      _value(0), _configured(false) {
    _zones[0] = {75, 100, TFT_RED};
    _zones[1] = {50, 75, TFT_ORANGE};
    _zones[2] = {25, 50, TFT_YELLOW};
    _zones[3] = {0, 25, TFT_GREEN};
}

void Seeed_MeterWidget::setZones(uint8_t redStart, uint8_t redEnd,
                                 uint8_t orangeStart, uint8_t orangeEnd,
                                 uint8_t yellowStart, uint8_t yellowEnd,
                                 uint8_t greenStart, uint8_t greenEnd) {
    _zones[0].start = redStart; _zones[0].end = redEnd;
    _zones[1].start = orangeStart; _zones[1].end = orangeEnd;
    _zones[2].start = yellowStart; _zones[2].end = yellowEnd;
    _zones[3].start = greenStart; _zones[3].end = greenEnd;
}

void Seeed_MeterWidget::setLabels(const char* label1, const char* label2,
                                  const char* label3, const char* label4,
                                  const char* label5) {
    _labels[0] = label1 ? label1 : "";
    _labels[1] = label2 ? label2 : "";
    _labels[2] = label3 ? label3 : "";
    _labels[3] = label4 ? label4 : "";
    _labels[4] = label5 ? label5 : "";
}

bool Seeed_MeterWidget::draw(int16_t x, int16_t y, uint16_t width,
                             uint16_t height, float fullScale,
                             const char* units) {
    if (!_gfx || width < 60 || height < 45 || fullScale <= 0) return false;
    _x = x; _y = y; _width = width; _height = height;
    _fullScale = fullScale; _units = units ? units : "";
    _value = 0; _configured = true;
    drawFace();
    return true;
}

void Seeed_MeterWidget::pointForPercent(float percent, int16_t radius,
                                        int16_t& x, int16_t& y) const {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    const float radians = (135.0f + percent * 2.7f) * 0.01745329252f;
    const int16_t cx = _x + static_cast<int16_t>(_width / 2U);
    const int16_t cy = _y + static_cast<int16_t>(_height * 61U / 100U);
    x = static_cast<int16_t>(cx + cosf(radians) * radius);
    y = static_cast<int16_t>(cy + sinf(radians) * radius);
}

void Seeed_MeterWidget::drawZone(const Zone& zone, int16_t cx, int16_t cy,
                                 int16_t radius) {
    (void)cx; (void)cy;
    if (!_gfx || zone.start >= zone.end) return;
    const int16_t inner = radius > 7 ? radius - 7 : radius;
    for (uint16_t percent = zone.start; percent <= zone.end; ++percent) {
        int16_t x0, y0, x1, y1;
        pointForPercent(percent, inner, x0, y0);
        pointForPercent(percent, radius, x1, y1);
        _gfx->drawLine(x0, y0, x1, y1, zone.color);
    }
}

void Seeed_MeterWidget::drawFace() {
    if (!_gfx || !_configured) return;
    const uint8_t oldDatum = _gfx->getTextDatum();
    const uint32_t oldColor = _gfx->textcolor;
    const uint32_t oldBackground = _gfx->textbgcolor;
    _gfx->fillRoundRect(_x, _y, _width, _height, 8, TFT_DARKGREY);
    _gfx->drawRoundRect(_x, _y, _width, _height, 8, TFT_WHITE);
    const int16_t cx = _x + static_cast<int16_t>(_width / 2U);
    // A 270 degree dial has its end points 0.707 * radius below the
    // centre. Keep those end points inside the requested rectangle.
    const int16_t cy = _y + static_cast<int16_t>(_height * 61U / 100U);
    int16_t radius = static_cast<int16_t>(_height * 54U / 100U);
    const int16_t widthRadius = static_cast<int16_t>(_width / 2U - 8U);
    if (radius > widthRadius) radius = widthRadius;
    for (uint8_t i = 0; i < 4; ++i) drawZone(_zones[i], cx, cy, radius);
    for (uint8_t tick = 0; tick <= 10; ++tick) {
        int16_t x0, y0, x1, y1;
        pointForPercent(tick * 10.0f, radius - 10, x0, y0);
        pointForPercent(tick * 10.0f, radius, x1, y1);
        _gfx->drawLine(x0, y0, x1, y1, TFT_WHITE);
    }
    // Draw scale labels (one per label slot at 0/25/50/75/100 %)
    for (uint8_t i = 0; i < LABEL_COUNT; ++i) {
        if (_labels[i].length() == 0) continue;
        const float percent = i * 25.0f;
        int16_t lx, ly;
        const int16_t labelRadius = radius > 18 ? radius - 18 : radius / 2;
        pointForPercent(percent, labelRadius, lx, ly);
        _gfx->setTextDatum(MC_DATUM);
        _gfx->setTextColor(TFT_WHITE, TFT_DARKGREY);
        _gfx->drawString(_labels[i], lx, ly, 1);
    }
    _gfx->setTextDatum(MC_DATUM);
    _gfx->setTextColor(TFT_WHITE, TFT_DARKGREY);
    _gfx->drawString(_units, cx, _y + _height - 11, 1);
    _gfx->setTextColor(oldColor, oldBackground);
    _gfx->setTextDatum(oldDatum);
}

bool Seeed_MeterWidget::update(float value) {
    if (!_gfx || !_configured) return false;
    if (value < 0) value = 0;
    if (value > _fullScale) value = _fullScale;
    _value = value;
    drawFace();
    int16_t radius = static_cast<int16_t>(_height * 54U / 100U) - 12;
    const int16_t widthRadius = static_cast<int16_t>(_width / 2U - 22U);
    if (radius > widthRadius) radius = widthRadius;
    int16_t nx, ny;
    pointForPercent((_value * 100.0f) / _fullScale, radius, nx, ny);
    const int16_t cx = _x + static_cast<int16_t>(_width / 2U);
    const int16_t cy = _y + static_cast<int16_t>(_height * 61U / 100U);
    _gfx->drawWideLine(cx, cy, nx, ny, 3.0f, TFT_RED, TFT_DARKGREY);
    _gfx->fillCircle(cx, cy, 4, TFT_BLACK);
    return true;
}
