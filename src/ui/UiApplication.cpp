#if !defined(ARDUINO_ARCH_NRF52)
/* UI layer skipped on nRF52 (Seeed core uses -std=gnu++11; UI needs C++14/17
   aggregate features). Core graphics + ePaper do not depend on the UI. */
#include "UiApplication.h"
#include "../Seeed_GFX.h"
#include "widget/UiWidget.h"

UiApplication::UiApplication(Seeed_GFX& gfx, UiInputHub& input, UiTheme& theme)
    : _gfx(gfx), _input(input), _theme(theme), _focus(), _navigator(_focus),
      _renderer(gfx), _overlays(_focus, _renderer) {}

UiStatus UiApplication::begin(UiScreen& root) {
    if (!_gfx.hasPanel()) return UiStatus::NotInitialized;
    UiStatus status = _input.begin();
    if (!uiOk(status)) return status;
    _renderer.begin();
    status = _navigator.push(root);
    if (!uiOk(status)) return status;
    bindCurrentScreen();
    _diagnostics.lastStatus = UiStatus::Ok;
    return UiStatus::Ok;
}

void UiApplication::bindCurrentScreen() {
    UiScreen* screen = _navigator.current();
    if (!screen) return;
    if (_boundScreen == screen && _boundGeneration == _navigator.generation()) return;
    if (_pointerCapture) {
        UiEvent cancel; cancel.type = UiEventType::PointerCancel;
        _pointerCapture->onEvent(cancel); _pointerCapture = nullptr;
    }
    _dragCandidate = nullptr;
    _boundScreen = screen;
    _boundGeneration = _navigator.generation();
    UiWidget& root = screen->root();
    root.setInvalidationSink(&_renderer);
    UiRect screenRect; screenRect.w = _gfx.width(); screenRect.h = _gfx.height();
    root.layout(screenRect);
    if (!_focus.focused()) _focus.focusFirst(root);
    _renderer.invalidateAll();
}

UiWidget* UiApplication::focusableAncestor(UiWidget* widget) const {
    while (widget && !widget->focusable()) widget = widget->parent();
    return widget;
}

UiWidget* UiApplication::dragAncestor(UiWidget* widget) const {
    while (widget && !widget->acceptsPointerDrag()) widget = widget->parent();
    return widget;
}

bool UiApplication::bubble(UiWidget* target, UiEvent& event) {
    for (UiWidget* widget = target; widget; widget = widget->parent()) {
        if (widget->onEvent(event)) { event.handled = true; return true; }
    }
    return false;
}

bool UiApplication::dispatch(UiEvent& event) {
    UiScreen* screen = _navigator.current();
    if (!screen) return false;
    UiWidget& root = screen->root();
    UiWidget* eventRoot = _overlays.active() ? _overlays.modal() : &root;

    if (event.type == UiEventType::PointerDown) {
        _pointerCapture = eventRoot->hitTest({event.x, event.y});
        _dragCandidate = dragAncestor(_pointerCapture);
        _pointerDown = {event.x, event.y};
        UiWidget* focusTarget = focusableAncestor(_pointerCapture);
        if (focusTarget) _focus.setFocus(focusTarget);
        if (bubble(_pointerCapture, event)) return true;
    } else if (event.type == UiEventType::PointerMove ||
               event.type == UiEventType::PointerUp ||
               event.type == UiEventType::PointerCancel) {
        if (event.type == UiEventType::PointerMove && _dragCandidate &&
            _pointerCapture != _dragCandidate) {
            const int32_t dx = static_cast<int32_t>(event.x) - _pointerDown.x;
            const int32_t dy = static_cast<int32_t>(event.y) - _pointerDown.y;
            const int32_t slop = _theme.metrics.touchSlop;
            if (dx * dx + dy * dy >= slop * slop) {
                UiEvent cancel = event; cancel.type = UiEventType::PointerCancel;
                if (_pointerCapture) _pointerCapture->onEvent(cancel);
                _pointerCapture = _dragCandidate;
                UiEvent down = event; down.type = UiEventType::PointerDown;
                down.x = _pointerDown.x; down.y = _pointerDown.y;
                _pointerCapture->onEvent(down);
            }
        }
        UiWidget* target = _pointerCapture;
        const bool handled = bubble(target, event);
        if (event.type == UiEventType::PointerUp || event.type == UiEventType::PointerCancel) {
            _pointerCapture = nullptr;
            _dragCandidate = nullptr;
        }
        if (handled) return true;
    } else if (event.type == UiEventType::Action || event.type == UiEventType::Scroll) {
        if (bubble(_focus.focused(), event)) return true;
    }

    if (!_overlays.active() && screen->onEvent(event)) {
        event.handled = true; return true;
    }
    if (event.type == UiEventType::Action &&
        (event.phase == UiActionPhase::Pressed || event.phase == UiActionPhase::Repeat)) {
        if (event.action == UiAction::NavigateDown || event.action == UiAction::NavigateRight) {
            event.handled = _focus.moveNext(*eventRoot); return event.handled;
        }
        if (event.action == UiAction::NavigateUp || event.action == UiAction::NavigateLeft) {
            event.handled = _focus.movePrevious(*eventRoot); return event.handled;
        }
    }
    if (event.type == UiEventType::Action && event.phase == UiActionPhase::Released) {
        if (event.action == UiAction::Back && _overlays.active()) {
            event.handled = uiOk(_overlays.dismiss()); return event.handled;
        }
        if (event.action == UiAction::Back) {
            event.handled = uiOk(_navigator.pop()); return event.handled;
        }
        if (event.action == UiAction::Home) {
            event.handled = uiOk(_navigator.popToRoot()); return event.handled;
        }
    }
    return false;
}

UiStatus UiApplication::tick(uint32_t nowMs) {
    if (!_navigator.current()) return UiStatus::NotInitialized;
    ++_diagnostics.ticks;
    _input.scan(nowMs);
    UiEvent event;
    uint16_t processed = 0;
    while (processed < SEEED_UI_MAX_EVENTS_PER_TICK && _input.poll(event)) {
        dispatch(event); ++processed;
        bindCurrentScreen();
    }
    _navigator.current()->update(nowMs);
    bindCurrentScreen();
    const UiStatus status = _renderer.render(_navigator.current()->root(), _theme,
                                             _overlays.modal());
    _diagnostics.frames = _renderer.frameCount();
    _diagnostics.inputOverflows = _input.overflowCount();
    _diagnostics.lastStatus = status;
    return status;
}

#endif // !ARDUINO_ARCH_NRF52 (UI layer)
