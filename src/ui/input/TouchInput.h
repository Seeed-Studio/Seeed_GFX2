#ifndef SEEED_UI_TOUCH_INPUT_H
#define SEEED_UI_TOUCH_INPUT_H

#include "IUiInputSource.h"
#include "../UiTypes.h"

class Seeed_GFX;
class ITouch;

class TouchInput : public IUiInputSource {
public:
    explicit TouchInput(Seeed_GFX& gfx, uint8_t sourceId = 2,
                        uint16_t pressureThreshold = 600,
                        int16_t moveThreshold = 12,
                        uint8_t rotation = 0, bool swapXY = false);
    UiStatus begin() override;
    void scan(uint32_t nowMs, IUiRawEventSink& sink) override;

private:
    Seeed_GFX& _gfx;
    uint8_t _sourceId;
    uint16_t _pressureThreshold;
    int16_t _moveThreshold;
    uint8_t _rotation;
    bool _swapXY;

    bool _pressed = false;
    int16_t _lastX = 0;
    int16_t _lastY = 0;

    static constexpr uint8_t kDebounce = 2;
    static constexpr uint8_t kAvg = 4;
    int16_t _avgX[kAvg] = {};
    int16_t _avgY[kAvg] = {};
    uint8_t _avgCount = 0;
    uint8_t _pressStreak = 0;
    uint8_t _releaseStreak = 0;
    uint32_t _lastScanMs = 0;

    void applyTransform(int16_t& x, int16_t& y) const;
    void pushSample(int16_t x, int16_t y);
    int16_t average(const int16_t* values, uint8_t count) const;
};

class DirectTouchInput : public IUiInputSource {
public:
    explicit DirectTouchInput(ITouch& touch, uint8_t sourceId = 2,
                              int16_t moveThreshold = 12);
    UiStatus begin() override { return UiStatus::Ok; }
    void scan(uint32_t nowMs, IUiRawEventSink& sink) override;

private:
    ITouch& _touch;
    uint8_t _sourceId;
    int16_t _moveThreshold;
    bool _pressed = false;
    int16_t _lastX = 0;
    int16_t _lastY = 0;
};

#endif // SEEED_UI_TOUCH_INPUT_H
