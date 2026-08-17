/**
 * @file SenseCAPWatcherInput.h
 * @brief SenseCAP Watcher rotary wheel and push-button UI adapter.
 */

#ifndef SEEED_UI_SENSECAP_WATCHER_INPUT_H
#define SEEED_UI_SENSECAP_WATCHER_INPUT_H

#include "EncoderInput.h"
#include "UiActionMap.h"

class Seeed_GFX;
class Board_SenseCAP_Watcher;

enum class SenseCAPWatcherKey : uint16_t {
    KnobPress = 1,
};

/** Map the Watcher knob push switch to UiAction::Activate. */
void installSenseCAPWatcherDefaultActionMap(UiActionMap& map);

/**
 * Connect the product-created Watcher board controls to UiInputHub.
 *
 * Rotation emits UiEventType::Scroll. The active-low knob push switch emits
 * the physical SenseCAPWatcherKey::KnobPress key code.
 */
class SenseCAPWatcherInput : public IUiInputSource {
public:
    explicit SenseCAPWatcherInput(Seeed_GFX& gfx, uint8_t sourceId = 4,
                                  uint16_t debounceMs = 25);

    UiStatus begin() override;
    void scan(uint32_t nowMs, IUiRawEventSink& sink) override;

private:
    static int16_t readDelta(void* context);
    static bool readPressed(void* context);

    Seeed_GFX& _gfx;
    Board_SenseCAP_Watcher* _board = nullptr;
    bool _lastPressed = false;
    bool _pollButtonNow = false;
    uint32_t _nextButtonPollMs = 0;
    EncoderInput _encoder;
};

#endif // SEEED_UI_SENSECAP_WATCHER_INPUT_H
