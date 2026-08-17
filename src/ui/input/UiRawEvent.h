#ifndef SEEED_UI_RAW_EVENT_H
#define SEEED_UI_RAW_EVENT_H

#include <stdint.h>

enum class UiRawType : uint8_t {
    KeyDown = 0,
    KeyUp,
    KeyLongPressed,
    KeyRepeat,
    PointerDown,
    PointerMove,
    PointerUp,
    PointerCancel,
    Wheel
};

struct UiRawEvent {
    UiRawType type = UiRawType::KeyDown;
    uint16_t code = 0;
    int16_t x = 0;
    int16_t y = 0;
    int16_t delta = 0;
    uint8_t sourceId = 0;
    uint32_t timestampMs = 0;
};

class IUiRawEventSink {
public:
    virtual ~IUiRawEventSink() = default;
    virtual bool pushRaw(const UiRawEvent& event) = 0;
};

#endif
