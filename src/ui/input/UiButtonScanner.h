#ifndef SEEED_UI_BUTTON_SCANNER_H
#define SEEED_UI_BUTTON_SCANNER_H

#include <Arduino.h>
#include <stddef.h>
#include "../UiStatus.h"
#include "../UiTypes.h"
#include "UiRawEvent.h"

struct UiButtonTiming {
    uint16_t debounceMs = 25;
    uint16_t longPressMs = 600;
    uint16_t repeatDelayMs = 450;
    uint16_t repeatRateMs = 100;
};

struct UiButtonConfig {
    int16_t pin = -1;
    uint16_t code = 0;
    uint8_t activeLevel = LOW;
    uint8_t pinModeValue = INPUT_PULLUP;
};

struct UiButtonState {
    bool rawPressed = false;
    bool stablePressed = false;
    bool longSent = false;
    bool armed = true;
    uint32_t rawChangedAt = 0;
    uint32_t pressedAt = 0;
    uint32_t nextRepeatAt = 0;
};

using UiDigitalReadFn = int (*)(int16_t pin, void* context);

class UiButtonScanner {
public:
    UiButtonScanner(UiButtonConfig* configs, UiButtonState* states, size_t count,
                    UiButtonTiming timing = UiButtonTiming(),
                    UiDigitalReadFn reader = nullptr, void* readerContext = nullptr)
        : _configs(configs), _states(states), _count(count), _timing(timing),
          _reader(reader), _readerContext(readerContext) {}

    UiStatus begin(uint32_t nowMs = 0);
    void scan(uint32_t nowMs, uint8_t sourceId, IUiRawEventSink& sink);

private:
    bool readPressed(const UiButtonConfig& config) const;
    void emit(UiRawType type, const UiButtonConfig& config, uint8_t sourceId,
              uint32_t nowMs, IUiRawEventSink& sink) const;

    UiButtonConfig* _configs;
    UiButtonState* _states;
    size_t _count;
    UiButtonTiming _timing;
    UiDigitalReadFn _reader;
    void* _readerContext;
};

#endif
