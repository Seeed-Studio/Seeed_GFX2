#if !defined(ARDUINO_ARCH_NRF52)
/* UI layer skipped on nRF52 (Seeed core uses -std=gnu++11; UI needs C++14/17
   aggregate features). Core graphics + ePaper do not depend on the UI. */
#include "ButtonInput.h"

ButtonInputConfig::ButtonInputConfig() {
    for (size_t i = 0; i < SEEED_UI_MAX_BUTTONS_PER_SOURCE; ++i)
        buttons[i].pin = -1;
}

UiStatus ButtonInputConfig::add(int16_t pin, uint16_t code,
                                uint8_t activeLevel, uint8_t pinModeValue) {
    UiButtonConfig button;
    button.pin = pin;
    button.code = code;
    button.activeLevel = activeLevel;
    button.pinModeValue = pinModeValue;
    return add(button);
}

UiStatus ButtonInputConfig::add(const UiButtonConfig& button) {
    if (button.pin < 0) return UiStatus::InvalidArgument;
    if (count >= SEEED_UI_MAX_BUTTONS_PER_SOURCE)
        return UiStatus::CapacityExceeded;
    buttons[count++] = button;
    return UiStatus::Ok;
}

ButtonInput::ButtonInput(const ButtonInputConfig& config,
                         UiDigitalReadFn reader, void* readerContext)
    : _config(config),
      _scanner(_config.buttons, _states,
               _config.count <= SEEED_UI_MAX_BUTTONS_PER_SOURCE
                   ? _config.count : 0,
               _config.timing,
               reader, readerContext) {}

UiStatus ButtonInput::begin() { return _scanner.begin(millis()); }

void ButtonInput::scan(uint32_t nowMs, IUiRawEventSink& sink) {
    _scanner.scan(nowMs, _config.sourceId, sink);
}

#endif // !ARDUINO_ARCH_NRF52 (UI layer)
