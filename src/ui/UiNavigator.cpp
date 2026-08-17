#if !defined(ARDUINO_ARCH_NRF52)
/* UI layer skipped on nRF52 (Seeed core uses -std=gnu++11; UI needs C++14/17
   aggregate features). Core graphics + ePaper do not depend on the UI. */
#include "UiNavigator.h"
#include "widget/UiWidget.h"

void UiNavigator::saveFocus() {
    if (!_depth) return;
    _stack[_depth - 1].savedFocusId = _focus.focused()
        ? _focus.focused()->id() : UI_ID_NONE;
}

void UiNavigator::restoreFocus() {
    if (!_depth) { _focus.clearFocus(); return; }
    UiRouteEntry& entry = _stack[_depth - 1];
    UiWidget& root = entry.screen->root();
    UiWidget* saved = _focus.findById(root, entry.savedFocusId);
    if (!saved || !_focus.setFocus(saved)) _focus.focusFirst(root);
}

UiStatus UiNavigator::push(UiScreen& screen, const void* params) {
    if (_depth >= SEEED_UI_NAV_STACK_DEPTH) return UiStatus::CapacityExceeded;
    if (_depth) { saveFocus(); current()->onPause(); }
    _focus.clearFocus();
    const UiStatus createStatus = screen.ensureCreated();
    if (!uiOk(createStatus)) {
        if (_depth) current()->onResume();
        return createStatus;
    }
    _stack[_depth++] = {&screen, UI_ID_NONE, 0};
    screen.onEnter(params);
    restoreFocus();
    ++_generation;
    return UiStatus::Ok;
}

UiStatus UiNavigator::pop() {
    if (_depth <= 1) return UiStatus::Unsupported;
    current()->onExit();
    --_depth;
    _focus.clearFocus();
    current()->onResume();
    restoreFocus();
    ++_generation;
    return UiStatus::Ok;
}

UiStatus UiNavigator::replace(UiScreen& screen, const void* params) {
    if (!_depth) return push(screen, params);
    const UiStatus createStatus = screen.ensureCreated();
    if (!uiOk(createStatus)) return createStatus;
    current()->onExit();
    _focus.clearFocus();
    _stack[_depth - 1] = {&screen, UI_ID_NONE, 0};
    screen.onEnter(params);
    restoreFocus();
    ++_generation;
    return UiStatus::Ok;
}

UiStatus UiNavigator::popToRoot() {
    if (!_depth) return UiStatus::NotInitialized;
    while (_depth > 1) {
        current()->onExit();
        --_depth;
    }
    _focus.clearFocus();
    current()->onResume();
    restoreFocus();
    ++_generation;
    return UiStatus::Ok;
}

#endif // !ARDUINO_ARCH_NRF52 (UI layer)
