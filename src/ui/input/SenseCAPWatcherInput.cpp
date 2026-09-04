#if !defined(ARDUINO_ARCH_NRF52)
#include "SenseCAPWatcherInput.h"
#include "../../Seeed_GFX.h"
#include "../../board/boards/SenseCAP_Products.h"

void installSenseCAPWatcherDefaultActionMap(UiActionMap& map) {
    map.add({
        static_cast<uint16_t>(SenseCAPWatcherKey::KnobPress),
        UiAction::Activate, false, true
    });
}

SenseCAPWatcherInput::SenseCAPWatcherInput(Seeed_GFX& gfx, uint8_t sourceId,
                                           uint16_t debounceMs)
    : _gfx(gfx),
      _encoder(readDelta, readPressed, this,
               static_cast<uint16_t>(SenseCAPWatcherKey::KnobPress),
               sourceId, debounceMs, 2000) {}

UiStatus SenseCAPWatcherInput::begin() {
    if (_gfx.activeProduct() != Seeed_Product::SenseCAP_Watcher ||
        !_gfx.boardPtr()) {
        return UiStatus::NotInitialized;
    }
    // activeProduct() proves ProductCatalog created this exact board type, so
    // this remains safe on ESP32 builds compiled with RTTI disabled.
    _board = static_cast<Board_SenseCAP_Watcher*>(_gfx.boardPtr());
    if (!_board->beginControls()) return UiStatus::IoError;
    if (!_board->readKnobButton(_lastPressed)) return UiStatus::IoError;
    _nextButtonPollMs = 0;
    return _encoder.begin();
}

void SenseCAPWatcherInput::scan(uint32_t nowMs, IUiRawEventSink& sink) {
    _pollButtonNow =
        static_cast<int32_t>(nowMs - _nextButtonPollMs) >= 0;
    if (_pollButtonNow) _nextButtonPollMs = nowMs + 5U;
    _encoder.scan(nowMs, sink);
    _pollButtonNow = false;
}

int16_t SenseCAPWatcherInput::readDelta(void* context) {
    SenseCAPWatcherInput* self =
        static_cast<SenseCAPWatcherInput*>(context);
    return self && self->_board ? self->_board->readKnobDelta() : 0;
}

bool SenseCAPWatcherInput::readPressed(void* context) {
    SenseCAPWatcherInput* self =
        static_cast<SenseCAPWatcherInput*>(context);
    if (!self || !self->_board) return false;
    if (!self->_pollButtonNow) return self->_lastPressed;
    bool pressed = self->_lastPressed;
    if (self->_board->readKnobButton(pressed)) {
        self->_lastPressed = pressed;
    }
    return self->_lastPressed;
}
#endif // !ARDUINO_ARCH_NRF52
