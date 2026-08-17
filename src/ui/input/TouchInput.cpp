#if !defined(ARDUINO_ARCH_NRF52)
/* UI layer skipped on nRF52 (Seeed core uses -std=gnu++11; UI needs C++14/17
   aggregate features). Core graphics + ePaper do not depend on the UI. */
#include "TouchInput.h"
#include "../../Seeed_GFX.h"
#include "../../core/Touch.h"

namespace {
void emitPointer(IUiRawEventSink& sink, UiRawType type, uint8_t sourceId,
                 uint32_t nowMs, int16_t x, int16_t y) {
    UiRawEvent event;
    event.type = type; event.code = 0; event.sourceId = sourceId;
    event.timestampMs = nowMs; event.x = x; event.y = y;
    sink.pushRaw(event);
}
bool movedEnough(int16_t x, int16_t y, int16_t oldX, int16_t oldY, int16_t threshold) {
    const int32_t dx = static_cast<int32_t>(x) - oldX;
    const int32_t dy = static_cast<int32_t>(y) - oldY;
    return dx * dx + dy * dy >= static_cast<int32_t>(threshold) * threshold;
}
}

TouchInput::TouchInput(Seeed_GFX& gfx, uint8_t sourceId,
                       uint16_t pressureThreshold, int16_t moveThreshold,
                       uint8_t rotation, bool swapXY)
    : _gfx(gfx), _sourceId(sourceId), _pressureThreshold(pressureThreshold),
      _moveThreshold(moveThreshold), _rotation(rotation), _swapXY(swapXY) {}

UiStatus TouchInput::begin() {
    _pressed = false;
    _pressStreak = 0;
    _releaseStreak = 0;
    _avgCount = 0;
    _lastScanMs = 0;
    return _gfx.hasPanel() ? UiStatus::Ok : UiStatus::NotInitialized;
}

void TouchInput::applyTransform(int16_t& x, int16_t& y) const {
    if (_swapXY) {
        int16_t t = x; x = y; y = t;
    }
    int16_t w = static_cast<int16_t>(_gfx.width()) - 1;
    int16_t h = static_cast<int16_t>(_gfx.height()) - 1;
    switch (_rotation % 4) {
        case 0: break;
        case 1: { int16_t t = x; x = static_cast<int16_t>(h - y); y = t; } break;
        case 2: { x = static_cast<int16_t>(w - x); y = static_cast<int16_t>(h - y); } break;
        case 3: { int16_t t = x; x = y; y = static_cast<int16_t>(w - t); } break;
    }
}

void TouchInput::pushSample(int16_t x, int16_t y) {
    for (uint8_t i = kAvg - 1; i > 0; --i) {
        _avgX[i] = _avgX[i - 1];
        _avgY[i] = _avgY[i - 1];
    }
    _avgX[0] = x;
    _avgY[0] = y;
    if (_avgCount < kAvg) ++_avgCount;
}

int16_t TouchInput::average(const int16_t* values, uint8_t count) const {
    if (count == 0) return 0;
    int32_t sum = 0;
    for (uint8_t i = 0; i < count; ++i) sum += values[i];
    return static_cast<int16_t>(sum / count);
}

void TouchInput::scan(uint32_t nowMs, IUiRawEventSink& sink) {
    // The FT6x36 is polled over the shared I2C bus. Reading it on every
    // CPU loop can starve display transfers and cause missed presses.
    // Cap the touch scan rate to ~100 Hz.
    if (nowMs - _lastScanMs < 10) return;
    _lastScanMs = nowMs;

    int32_t x = 0, y = 0;
    const bool pressed = _gfx.getTouch(&x, &y, _pressureThreshold);
    int16_t sx = uiClamp16(x), sy = uiClamp16(y);
    applyTransform(sx, sy);

    if (pressed) {
        pushSample(sx, sy);
        _pressStreak++;
        _releaseStreak = 0;
    } else {
        _pressStreak = 0;
        _releaseStreak++;
    }

    // Debounce: require a few consecutive stable readings before committing to
    // a press or release. Averaging the last few samples smooths noise.
    if (pressed && !_pressed && _pressStreak >= kDebounce) {
        int16_t ax = average(_avgX, _avgCount);
        int16_t ay = average(_avgY, _avgCount);
        _lastX = ax; _lastY = ay; _pressed = true;
        emitPointer(sink, UiRawType::PointerDown, _sourceId, nowMs, ax, ay);
    } else if (pressed && _pressed) {
        int16_t ax = average(_avgX, _avgCount);
        int16_t ay = average(_avgY, _avgCount);
        if (movedEnough(ax, ay, _lastX, _lastY, _moveThreshold)) {
            _lastX = ax; _lastY = ay;
            emitPointer(sink, UiRawType::PointerMove, _sourceId, nowMs, ax, ay);
        }
    } else if (!pressed && _pressed && _releaseStreak >= kDebounce) {
        emitPointer(sink, UiRawType::PointerUp, _sourceId, nowMs, _lastX, _lastY);
        _pressed = false;
        _avgCount = 0;
    } else if (!pressed) {
        _avgCount = 0;
    }
}

DirectTouchInput::DirectTouchInput(ITouch& touch, uint8_t sourceId,
                                   int16_t moveThreshold)
    : _touch(touch), _sourceId(sourceId), _moveThreshold(moveThreshold) {}

void DirectTouchInput::scan(uint32_t nowMs, IUiRawEventSink& sink) {
    TouchPoint point;
    const bool pressed = _touch.read(point) && point.pressed;
    const int16_t sx = static_cast<int16_t>(point.x);
    const int16_t sy = static_cast<int16_t>(point.y);
    if (pressed && !_pressed) {
        _lastX = sx; _lastY = sy; _pressed = true;
        emitPointer(sink, UiRawType::PointerDown, _sourceId, nowMs, sx, sy);
    } else if (pressed && movedEnough(sx, sy, _lastX, _lastY, _moveThreshold)) {
        _lastX = sx; _lastY = sy;
        emitPointer(sink, UiRawType::PointerMove, _sourceId, nowMs, sx, sy);
    } else if (!pressed && _pressed) {
        emitPointer(sink, UiRawType::PointerUp, _sourceId, nowMs, _lastX, _lastY);
        _pressed = false;
    }
}

#endif // !ARDUINO_ARCH_NRF52 (UI layer)
