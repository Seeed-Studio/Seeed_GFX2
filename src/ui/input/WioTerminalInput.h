#ifndef SEEED_UI_WIO_TERMINAL_INPUT_H
#define SEEED_UI_WIO_TERMINAL_INPUT_H

#include "IUiInputSource.h"
#include "UiActionMap.h"
#include "ButtonInput.h"

enum class WioTerminalKey : uint16_t {
    Up = 1, Down, Left, Right, Press, A, B, C
};

struct WioTerminalInputConfig {
    UiButtonConfig buttons[8] = {};
    UiButtonTiming timing;
    uint8_t sourceId = 1;
};

WioTerminalInputConfig wioDefaultInputConfig();
void installWioDefaultActionMap(UiActionMap& map);

class WioTerminalInput : public IUiInputSource {
public:
    explicit WioTerminalInput(const WioTerminalInputConfig& config,
                              UiDigitalReadFn reader = nullptr,
                              void* readerContext = nullptr);
    WioTerminalInput(const WioTerminalInput&) = delete;
    WioTerminalInput& operator=(const WioTerminalInput&) = delete;
    UiStatus begin() override;
    void scan(uint32_t nowMs, IUiRawEventSink& sink) override;

private:
    ButtonInput _input;
};

#endif
