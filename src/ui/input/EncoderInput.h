#ifndef SEEED_UI_ENCODER_INPUT_H
#define SEEED_UI_ENCODER_INPUT_H

#include "IUiInputSource.h"

using UiEncoderDeltaFn = int16_t (*)(void* context);
using UiEncoderPressedFn = bool (*)(void* context);

class EncoderInput : public IUiInputSource {
public:
    EncoderInput(UiEncoderDeltaFn deltaReader, UiEncoderPressedFn pressReader,
                 void* context = nullptr, uint16_t pressCode = 0,
                 uint8_t sourceId = 3, uint16_t debounceMs = 25,
                 uint16_t longPressMs = 0)
        : _deltaReader(deltaReader), _pressReader(pressReader), _context(context),
          _pressCode(pressCode), _sourceId(sourceId), _debounceMs(debounceMs),
          _longPressMs(longPressMs) {}

    UiStatus begin() override;
    void scan(uint32_t nowMs, IUiRawEventSink& sink) override;

private:
    UiEncoderDeltaFn _deltaReader;
    UiEncoderPressedFn _pressReader;
    void* _context;
    uint16_t _pressCode;
    uint8_t _sourceId;
    uint16_t _debounceMs;
    uint16_t _longPressMs;
    bool _rawPressed = false;
    bool _stablePressed = false;
    bool _pressActive = false;
    bool _longPressEmitted = false;
    uint32_t _changedAt = 0;
    uint32_t _pressedAt = 0;
};

#endif
