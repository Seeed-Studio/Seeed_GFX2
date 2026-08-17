#if !defined(ARDUINO_ARCH_NRF52)
/* UI layer skipped on nRF52 (Seeed core uses -std=gnu++11; UI needs C++14/17
   aggregate features). Core graphics + ePaper do not depend on the UI. */
#include "EncoderInput.h"

UiStatus EncoderInput::begin() {
    if (!_deltaReader && !_pressReader) return UiStatus::InvalidArgument;
    _rawPressed = _stablePressed = _pressReader ? _pressReader(_context) : false;
    _pressActive = false;
    _longPressEmitted = false;
    _changedAt = 0;
    _pressedAt = 0;
    return UiStatus::Ok;
}

void EncoderInput::scan(uint32_t nowMs, IUiRawEventSink& sink) {
    if (_deltaReader) {
        const int16_t delta = _deltaReader(_context);
        if (delta) {
            UiRawEvent event;
            event.type = UiRawType::Wheel; event.delta = delta;
            event.sourceId = _sourceId; event.timestampMs = nowMs;
            sink.pushRaw(event);
        }
    }
    if (!_pressReader) return;
    const bool pressed = _pressReader(_context);
    if (pressed != _rawPressed) { _rawPressed = pressed; _changedAt = nowMs; }
    if (_rawPressed != _stablePressed &&
        static_cast<uint32_t>(nowMs - _changedAt) >= _debounceMs) {
        _stablePressed = _rawPressed;
        if (pressed) {
            _pressActive = true;
            _longPressEmitted = false;
            _pressedAt = nowMs;
            UiRawEvent event;
            event.type = UiRawType::KeyDown;
            event.code = _pressCode; event.sourceId = _sourceId;
            event.timestampMs = nowMs;
            sink.pushRaw(event);
        } else {
            // Suppress a lone KeyUp when the switch was held during begin().
            if (_pressActive) {
                UiRawEvent event;
                event.type = UiRawType::KeyUp;
                event.code = _pressCode; event.sourceId = _sourceId;
                event.timestampMs = nowMs;
                sink.pushRaw(event);
            }
            _pressActive = false;
            _longPressEmitted = false;
        }
    }
    if (_pressActive && _longPressMs && !_longPressEmitted &&
        static_cast<uint32_t>(nowMs - _pressedAt) >= _longPressMs) {
        _longPressEmitted = true;
        UiRawEvent event;
        event.type = UiRawType::KeyLongPressed;
        event.code = _pressCode; event.sourceId = _sourceId;
        event.timestampMs = nowMs;
        sink.pushRaw(event);
    }
}

#endif // !ARDUINO_ARCH_NRF52 (UI layer)
