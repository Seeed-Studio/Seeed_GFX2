#ifndef SEEED_GFX_CLASSIC_WIDGETS_H
#define SEEED_GFX_CLASSIC_WIDGETS_H

#include <Arduino.h>

class Seeed_GFX;

/** Immediate-mode button used by small sketches and direct touch polling. */
class Seeed_ButtonWidget {
public:
    typedef void (*Action)();

    explicit Seeed_ButtonWidget(Seeed_GFX* gfx = nullptr);
    void attach(Seeed_GFX& gfx) { _gfx = &gfx; }
    void initButtonUL(int16_t x, int16_t y, uint16_t w, uint16_t h,
                      uint16_t outline, uint16_t fill, uint16_t text,
                      const char* label, uint8_t textSize = 1);
    void draw(bool inverted = false, uint8_t outlineWidth = 2,
              uint16_t surrounding = 0x0000, const char* label = nullptr);
    bool contains(int16_t x, int16_t y) const;
    void press(bool pressed);
    bool isPressed() const { return _current; }
    bool justPressed() const { return _current && !_previous; }
    bool justReleased() const { return !_current && _previous; }
    void setPressAction(Action action) { _pressAction = action; }
    void setReleaseAction(Action action) { _releaseAction = action; }
    void runPressAction() { if (_pressAction) _pressAction(); }
    void runReleaseAction() { if (_releaseAction) _releaseAction(); }
    uint32_t pressTime() const { return _pressTime; }
    uint32_t releaseTime() const { return _releaseTime; }

private:
    Seeed_GFX* _gfx;
    int16_t _x, _y;
    uint16_t _w, _h, _outline, _fill, _text;
    uint8_t _textSize;
    String _label;
    bool _current, _previous;
    uint32_t _pressTime, _releaseTime;
    Action _pressAction, _releaseAction;
};

enum class Seeed_SliderOrientation : uint8_t { Horizontal, Vertical };

struct Seeed_SliderConfig {
    uint16_t slotWidth = 8;
    uint16_t slotLength = 180;
    uint16_t slotColor = 0x001F;
    uint16_t backgroundColor = 0x0000;
    Seeed_SliderOrientation orientation = Seeed_SliderOrientation::Horizontal;
    uint16_t knobWidth = 18;
    uint16_t knobHeight = 28;
    uint16_t knobRadius = 5;
    uint16_t knobColor = 0xFFFF;
    uint16_t markerColor = 0xF800;
    int32_t minimum = 0;
    int32_t maximum = 100;
    int32_t startValue = 50;
};

/** Touch-polled slider. No Sprite or third-party widget library is required. */
class Seeed_SliderWidget {
public:
    explicit Seeed_SliderWidget(Seeed_GFX* gfx = nullptr);
    void attach(Seeed_GFX& gfx) { _gfx = &gfx; }
    bool draw(int16_t x, int16_t y, const Seeed_SliderConfig& config);
    bool setValue(int32_t value, bool redraw = true);
    int32_t value() const { return _value; }
    bool checkTouch(int16_t x, int16_t y);
    bool contains(int16_t x, int16_t y) const;
    void getBounds(int16_t* x, int16_t* y, uint16_t* w, uint16_t* h) const;

private:
    int32_t clampValue(int32_t value) const;
    int16_t knobCenter() const;
    void render();

    Seeed_GFX* _gfx;
    Seeed_SliderConfig _config;
    int16_t _x, _y;
    uint16_t _w, _h;
    int32_t _value;
    bool _configured;
};

class Seeed_TraceWidget;

/** Scaled graph background shared by one or more Seeed_TraceWidget objects. */
class Seeed_GraphWidget {
public:
    explicit Seeed_GraphWidget(Seeed_GFX* gfx = nullptr);
    void attach(Seeed_GFX& gfx) { _gfx = &gfx; }
    bool create(uint16_t width, uint16_t height, uint16_t background);
    void setScale(float xLow, float xHigh, float yLow, float yHigh);
    void setGrid(float xStart, float xStep, float yStart, float yStep,
                 uint16_t color);
    void draw(int16_t x, int16_t y);
    int16_t pointX(float value) const;
    int16_t pointY(float value) const;
    bool containsPixel(int16_t x, int16_t y) const;

private:
    friend class Seeed_TraceWidget;
    Seeed_GFX* _gfx;
    int16_t _x, _y;
    uint16_t _width, _height, _background, _gridColor;
    float _xLow, _xHigh, _yLow, _yHigh;
    float _xGridStart, _xGridStep, _yGridStart, _yGridStep;
    bool _configured;
};

/** One clipped trace attached to a Seeed_GraphWidget. */
class Seeed_TraceWidget {
public:
    explicit Seeed_TraceWidget(Seeed_GraphWidget* graph = nullptr);
    void attach(Seeed_GraphWidget& graph) { _graph = &graph; _hasPoint = false; }
    void start(uint16_t color);
    bool addPoint(float x, float y);

private:
    Seeed_GraphWidget* _graph;
    uint16_t _color;
    int16_t _lastX, _lastY;
    bool _hasPoint;
};

/** Compact analogue meter that scales to the requested rectangle. */
class Seeed_MeterWidget {
public:
    static constexpr uint8_t LABEL_COUNT = 5;

    explicit Seeed_MeterWidget(Seeed_GFX* gfx = nullptr);
    void attach(Seeed_GFX& gfx) { _gfx = &gfx; }
    void setZones(uint8_t redStart, uint8_t redEnd,
                  uint8_t orangeStart, uint8_t orangeEnd,
                  uint8_t yellowStart, uint8_t yellowEnd,
                  uint8_t greenStart, uint8_t greenEnd);
    void setLabels(const char* label1, const char* label2,
                   const char* label3, const char* label4,
                   const char* label5);
    bool draw(int16_t x, int16_t y, uint16_t width, uint16_t height,
              float fullScale, const char* units);
    bool update(float value);
    float value() const { return _value; }

private:
    struct Zone { uint8_t start, end; uint16_t color; };
    void drawFace();
    void drawZone(const Zone& zone, int16_t cx, int16_t cy, int16_t radius);
    void pointForPercent(float percent, int16_t radius,
                         int16_t& x, int16_t& y) const;

    Seeed_GFX* _gfx;
    int16_t _x, _y;
    uint16_t _width, _height;
    float _fullScale, _value;
    String _units;
    String _labels[LABEL_COUNT];
    Zone _zones[4];
    bool _configured;
};

#endif
