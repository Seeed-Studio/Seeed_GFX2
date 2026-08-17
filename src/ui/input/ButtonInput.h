#ifndef SEEED_UI_BUTTON_INPUT_H
#define SEEED_UI_BUTTON_INPUT_H

#include "IUiInputSource.h"
#include "UiButtonScanner.h"
#include "../UiConfig.h"

/**
 * Fixed-capacity GPIO button configuration for any Arduino-compatible board.
 * The configuration is copied by ButtonInput, so it may be local to setup().
 */
struct ButtonInputConfig {
    ButtonInputConfig();

    UiStatus add(int16_t pin, uint16_t code, uint8_t activeLevel = LOW,
                 uint8_t pinModeValue = INPUT_PULLUP);
    UiStatus add(const UiButtonConfig& button);

    UiButtonConfig buttons[SEEED_UI_MAX_BUTTONS_PER_SOURCE];
    size_t count = 0;
    UiButtonTiming timing;
    uint8_t sourceId = 1;
};

/**
 * Generic, allocation-free GPIO button input source.
 *
 * ButtonInput emits physical key codes. Bind those codes to semantic actions
 * with UiInputHub::actionMap(), keeping board pins separate from UI behavior.
 */
class ButtonInput : public IUiInputSource {
public:
    explicit ButtonInput(const ButtonInputConfig& config,
                         UiDigitalReadFn reader = nullptr,
                         void* readerContext = nullptr);
    ButtonInput(const ButtonInput&) = delete;
    ButtonInput& operator=(const ButtonInput&) = delete;

    UiStatus begin() override;
    void scan(uint32_t nowMs, IUiRawEventSink& sink) override;

private:
    ButtonInputConfig _config;
    UiButtonState _states[SEEED_UI_MAX_BUTTONS_PER_SOURCE] = {};
    UiButtonScanner _scanner;
};

#endif
