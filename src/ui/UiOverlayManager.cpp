#if !defined(ARDUINO_ARCH_NRF52)
/* UI layer skipped on nRF52 (Seeed core uses -std=gnu++11; UI needs C++14/17
   aggregate features). Core graphics + ePaper do not depend on the UI. */
#include "UiOverlayManager.h"
#include "widget/UiWidget.h"
#include "render/UiDirtyRegion.h"

UiStatus UiOverlayManager::showModal(UiWidget& widget) {
    if (_modal) return UiStatus::Busy;
    _savedFocus = _focus.focused();
    _modal = &widget;
    _modal->setInvalidationSink(&_invalidation);
    _modal->invalidate();
    _focus.clearFocus();
    if (!_focus.focusFirst(*_modal) && _modal->focusable()) _focus.setFocus(_modal);
    return UiStatus::Ok;
}

UiStatus UiOverlayManager::dismiss() {
    if (!_modal) return UiStatus::NotInitialized;
    _modal->invalidate();
    _modal->setInvalidationSink(nullptr);
    _modal = nullptr;
    _focus.clearFocus();
    if (_savedFocus && _savedFocus->visible() && _savedFocus->enabled())
        _focus.setFocus(_savedFocus);
    _savedFocus = nullptr;
    return UiStatus::Ok;
}

#endif // !ARDUINO_ARCH_NRF52 (UI layer)
