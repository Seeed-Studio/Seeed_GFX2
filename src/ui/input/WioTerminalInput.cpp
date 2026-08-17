#if !defined(ARDUINO_ARCH_NRF52)
/* UI layer skipped on nRF52 (Seeed core uses -std=gnu++11; UI needs C++14/17
   aggregate features). Core graphics + ePaper do not depend on the UI. */
#include "WioTerminalInput.h"

namespace {
inline UiButtonConfig button(int16_t pin, WioTerminalKey code) {
    UiButtonConfig result;
    result.pin = pin;
    result.code = static_cast<uint16_t>(code);
    result.activeLevel = LOW;
    result.pinModeValue = INPUT_PULLUP;
    return result;
}

ButtonInputConfig genericConfig(const WioTerminalInputConfig& config) {
    ButtonInputConfig result;
    result.timing = config.timing;
    result.sourceId = config.sourceId;
    for (uint8_t i = 0; i < 8; ++i) {
        if (config.buttons[i].pin >= 0) result.add(config.buttons[i]);
    }
    return result;
}
}

WioTerminalInputConfig wioDefaultInputConfig() {
    WioTerminalInputConfig config;
    for (uint8_t i = 0; i < 8; ++i) config.buttons[i].pin = -1;
#if defined(WIO_5S_UP)
    config.buttons[0] = button(WIO_5S_UP, WioTerminalKey::Up);
#endif
#if defined(WIO_5S_DOWN)
    config.buttons[1] = button(WIO_5S_DOWN, WioTerminalKey::Down);
#endif
#if defined(WIO_5S_LEFT)
    config.buttons[2] = button(WIO_5S_LEFT, WioTerminalKey::Left);
#endif
#if defined(WIO_5S_RIGHT)
    config.buttons[3] = button(WIO_5S_RIGHT, WioTerminalKey::Right);
#endif
#if defined(WIO_5S_PRESS)
    config.buttons[4] = button(WIO_5S_PRESS, WioTerminalKey::Press);
#endif
#if defined(WIO_KEY_A)
    config.buttons[5] = button(WIO_KEY_A, WioTerminalKey::A);
#endif
#if defined(WIO_KEY_B)
    config.buttons[6] = button(WIO_KEY_B, WioTerminalKey::B);
#endif
#if defined(WIO_KEY_C)
    config.buttons[7] = button(WIO_KEY_C, WioTerminalKey::C);
#endif
    return config;
}

void installWioDefaultActionMap(UiActionMap& map) {
    map.add({static_cast<uint16_t>(WioTerminalKey::Up), UiAction::NavigateUp, true, false});
    map.add({static_cast<uint16_t>(WioTerminalKey::Down), UiAction::NavigateDown, true, false});
    map.add({static_cast<uint16_t>(WioTerminalKey::Left), UiAction::NavigateLeft, true, false});
    map.add({static_cast<uint16_t>(WioTerminalKey::Right), UiAction::NavigateRight, true, false});
    map.add({static_cast<uint16_t>(WioTerminalKey::Press), UiAction::Activate, false, true});
    map.add({static_cast<uint16_t>(WioTerminalKey::A), UiAction::Back, false, true});
    map.add({static_cast<uint16_t>(WioTerminalKey::B), UiAction::Menu, false, true});
    map.add({static_cast<uint16_t>(WioTerminalKey::C), UiAction::Home, false, true});
}

WioTerminalInput::WioTerminalInput(const WioTerminalInputConfig& config,
                                   UiDigitalReadFn reader, void* readerContext)
    : _input(genericConfig(config), reader, readerContext) {}

UiStatus WioTerminalInput::begin() { return _input.begin(); }

void WioTerminalInput::scan(uint32_t nowMs, IUiRawEventSink& sink) {
    _input.scan(nowMs, sink);
}

#endif // !ARDUINO_ARCH_NRF52 (UI layer)
