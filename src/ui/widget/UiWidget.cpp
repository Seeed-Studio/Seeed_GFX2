#if !defined(ARDUINO_ARCH_NRF52)
/* UI layer skipped on nRF52 (Seeed core uses -std=gnu++11; UI needs C++14/17
   aggregate features). Core graphics + ePaper do not depend on the UI. */
#include "UiWidget.h"
#include "../render/UiCanvas.h"
#include "../theme/UiTheme.h"

void UiWidget::setVisible(bool visible) {
    if (_visible == visible) return;
    invalidate(); _visible = visible; invalidate();
}
void UiWidget::setEnabled(bool enabled) {
    if (_enabled == enabled) return;
    _enabled = enabled; invalidate();
}
void UiWidget::setFocusable(bool focusable) { _focusable = focusable; }
void UiWidget::setBounds(const UiRect& bounds) {
    if (_bounds.x == bounds.x && _bounds.y == bounds.y &&
        _bounds.w == bounds.w && _bounds.h == bounds.h) return;
    invalidate(); _bounds = bounds; invalidate();
}
bool UiWidget::onEvent(UiEvent&) { return false; }
UiWidget* UiWidget::hitTest(UiPoint point) {
    return visible() && enabled() && bounds().contains(point) ? this : nullptr;
}
void UiWidget::setInvalidationSink(IUiInvalidationSink* sink) { _sink = sink; }
void UiWidget::invalidate() { if (_sink && !_bounds.empty()) _sink->invalidateRect(_bounds); }
void UiWidget::invalidate(const UiRect& screenRect) { if (_sink) _sink->invalidateRect(screenRect); }
void UiWidget::setFocusedInternal(bool focused) {
    if (_focused == focused) return;
    _focused = focused; onFocusChanged(focused); invalidate();
}

void UiContainer::addChild(UiWidget& child) {
    if (child._parent == this) return;
    child._parent = this; child._nextSibling = nullptr;
    if (_lastChild) _lastChild->_nextSibling = &child;
    else _firstChild = &child;
    _lastChild = &child;
    child.setInvalidationSink(_sink);
    child.invalidate();
}

void UiContainer::render(UiCanvas& canvas, const UiTheme& theme) {
    if (!visible()) return;
    if (_drawBackground) canvas.fillRect(bounds(), _background);
    for (UiWidget* child = _firstChild; child; child = child->nextSibling())
        if (child->visible()) child->render(canvas, theme);
}

UiWidget* UiContainer::hitTest(UiPoint point) {
    if (!visible() || !enabled() || !bounds().contains(point)) return nullptr;
    UiWidget* hit = this;
    for (UiWidget* child = _firstChild; child; child = child->nextSibling()) {
        UiWidget* candidate = child->hitTest(point);
        if (candidate) hit = candidate;
    }
    return hit;
}

void UiContainer::setInvalidationSink(IUiInvalidationSink* sink) {
    UiWidget::setInvalidationSink(sink);
    for (UiWidget* child = _firstChild; child; child = child->nextSibling())
        child->setInvalidationSink(sink);
}

#endif // !ARDUINO_ARCH_NRF52 (UI layer)
