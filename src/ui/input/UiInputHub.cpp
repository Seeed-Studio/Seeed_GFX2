#if !defined(ARDUINO_ARCH_NRF52)
/* UI layer skipped on nRF52 (Seeed core uses -std=gnu++11; UI needs C++14/17
   aggregate features). Core graphics + ePaper do not depend on the UI. */
#include "UiInputHub.h"

UiStatus UiInputHub::add(IUiInputSource& source) {
    if (_sourceCount >= SEEED_UI_MAX_INPUT_SOURCES)
        return UiStatus::CapacityExceeded;
    _sources[_sourceCount++] = &source;
    return UiStatus::Ok;
}

UiStatus UiInputHub::begin() {
    for (size_t i = 0; i < _sourceCount; ++i) {
        const UiStatus status = _sources[i]->begin();
        if (!uiOk(status)) return status;
    }
    return UiStatus::Ok;
}

void UiInputHub::scan(uint32_t nowMs) {
    for (size_t i = 0; i < _sourceCount; ++i)
        _sources[i]->scan(nowMs, *this);
}

bool UiInputHub::pushRaw(const UiRawEvent& raw) {
    UiEvent event;
    event.timestampMs = raw.timestampMs;
    event.sourceId = raw.sourceId;
    event.x = raw.x; event.y = raw.y; event.delta = raw.delta;
    event.pointerId = static_cast<uint8_t>(raw.code);

    switch (raw.type) {
    case UiRawType::KeyDown:
    case UiRawType::KeyUp:
    case UiRawType::KeyLongPressed:
    case UiRawType::KeyRepeat: {
        const UiActionBinding* binding = _actionMap.find(raw.code);
        if (!binding || binding->action == UiAction::None) return false;
        if (raw.type == UiRawType::KeyRepeat && !binding->allowRepeat) return true;
        if (raw.type == UiRawType::KeyLongPressed && !binding->allowLongPress) return true;
        event.type = UiEventType::Action;
        event.action = binding->action;
        event.phase = raw.type == UiRawType::KeyDown ? UiActionPhase::Pressed :
                      raw.type == UiRawType::KeyUp ? UiActionPhase::Released :
                      raw.type == UiRawType::KeyLongPressed ? UiActionPhase::LongPressed :
                      UiActionPhase::Repeat;
        _mode = UiInputMode::Keys;
        break;
    }
    case UiRawType::PointerDown: event.type = UiEventType::PointerDown; _mode = UiInputMode::Touch; break;
    case UiRawType::PointerMove: event.type = UiEventType::PointerMove; _mode = UiInputMode::Touch; break;
    case UiRawType::PointerUp: event.type = UiEventType::PointerUp; _mode = UiInputMode::Touch; break;
    case UiRawType::PointerCancel: event.type = UiEventType::PointerCancel; _mode = UiInputMode::Touch; break;
    case UiRawType::Wheel: event.type = UiEventType::Scroll; _mode = UiInputMode::Encoder; break;
    }
    return _queue.push(event);
}

#endif // !ARDUINO_ARCH_NRF52 (UI layer)
