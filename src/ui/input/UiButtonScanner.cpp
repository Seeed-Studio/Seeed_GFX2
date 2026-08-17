#if !defined(ARDUINO_ARCH_NRF52)
/* UI layer skipped on nRF52 (Seeed core uses -std=gnu++11; UI needs C++14/17
   aggregate features). Core graphics + ePaper do not depend on the UI. */
#include "UiButtonScanner.h"

bool UiButtonScanner::readPressed(const UiButtonConfig& config) const {
    const int value = _reader ? _reader(config.pin, _readerContext)
                              : digitalRead(config.pin);
    return value == config.activeLevel;
}

UiStatus UiButtonScanner::begin(uint32_t nowMs) {
    if (!_configs || !_states || !_count) return UiStatus::InvalidArgument;
    for (size_t i = 0; i < _count; ++i) {
        if (_configs[i].pin < 0) continue;
        pinMode(_configs[i].pin, _configs[i].pinModeValue);
        const bool pressed = readPressed(_configs[i]);
        _states[i] = UiButtonState();
        _states[i].rawPressed = pressed;
        _states[i].stablePressed = pressed;
        _states[i].armed = !pressed;
        _states[i].rawChangedAt = nowMs;
        _states[i].pressedAt = nowMs;
        _states[i].nextRepeatAt = nowMs + _timing.repeatDelayMs;
    }
    return UiStatus::Ok;
}

void UiButtonScanner::emit(UiRawType type, const UiButtonConfig& config,
                           uint8_t sourceId, uint32_t nowMs,
                           IUiRawEventSink& sink) const {
    UiRawEvent event;
    event.type = type;
    event.code = config.code;
    event.sourceId = sourceId;
    event.timestampMs = nowMs;
    sink.pushRaw(event);
}

void UiButtonScanner::scan(uint32_t nowMs, uint8_t sourceId,
                           IUiRawEventSink& sink) {
    if (!_configs || !_states) return;
    for (size_t i = 0; i < _count; ++i) {
        const UiButtonConfig& config = _configs[i];
        UiButtonState& state = _states[i];
        if (config.pin < 0) continue;

        const bool pressed = readPressed(config);
        if (pressed != state.rawPressed) {
            state.rawPressed = pressed;
            state.rawChangedAt = nowMs;
        }

        if (state.stablePressed != state.rawPressed &&
            static_cast<uint32_t>(nowMs - state.rawChangedAt) >= _timing.debounceMs) {
            state.stablePressed = state.rawPressed;
            if (state.stablePressed) {
                state.pressedAt = nowMs;
                state.nextRepeatAt = nowMs + _timing.repeatDelayMs;
                state.longSent = false;
                if (state.armed)
                    emit(UiRawType::KeyDown, config, sourceId, nowMs, sink);
            } else {
                if (state.armed) emit(UiRawType::KeyUp, config, sourceId, nowMs, sink);
                else state.armed = true;
                state.longSent = false;
            }
        }

        if (!state.stablePressed || !state.armed) continue;
        if (!state.longSent &&
            static_cast<uint32_t>(nowMs - state.pressedAt) >= _timing.longPressMs) {
            state.longSent = true;
            emit(UiRawType::KeyLongPressed, config, sourceId, nowMs, sink);
        }
        if (_timing.repeatRateMs && uiTimeReached(nowMs, state.nextRepeatAt)) {
            emit(UiRawType::KeyRepeat, config, sourceId, nowMs, sink);
            do { state.nextRepeatAt += _timing.repeatRateMs; }
            while (uiTimeReached(nowMs, state.nextRepeatAt));
        }
    }
}

#endif // !ARDUINO_ARCH_NRF52 (UI layer)
