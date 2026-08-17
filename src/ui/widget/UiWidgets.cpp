#if !defined(ARDUINO_ARCH_NRF52)
/* UI layer skipped on nRF52 (Seeed core uses -std=gnu++11; UI needs C++14/17
   aggregate features). Core graphics + ePaper do not depend on the UI. */
#include "UiWidgets.h"
#include <stdio.h>
#include "../render/UiCanvas.h"
#include "../theme/UiTheme.h"

void UiLabel::render(UiCanvas& canvas, const UiTheme& theme) {
    if (!visible() || !_text) return;
    canvas.drawText(bounds().x, bounds().y, _text,
                    _customColor ? _color : theme.colors.textPrimary,
                    _textSize ? _textSize : theme.bodyTextSize);
}

UiButton::UiButton(const char* text, UiCommand command, UiId id)
    : UiWidget(id), _text(text), _command(command) { setFocusable(true); }

void UiButton::render(UiCanvas& canvas, const UiTheme& theme) {
    if (!visible()) return;
    const uint16_t background = !enabled() ? theme.colors.disabled :
        (_pressed || focused() ? theme.colors.accent : theme.colors.surface);
    canvas.fillRect(bounds(), background);
    if (focused()) canvas.drawRect(bounds(), theme.colors.focus, theme.metrics.borderWidth);
    canvas.drawText(bounds().x + theme.metrics.spacingMd,
                    bounds().y + theme.metrics.spacingSm, _text,
                    theme.colors.textPrimary, theme.bodyTextSize);
}

bool UiButton::onEvent(UiEvent& event) {
    if (!enabled()) return false;
    if (event.type == UiEventType::Action && event.action == UiAction::Activate) {
        if (event.phase == UiActionPhase::Pressed) { _pressed = true; invalidate(); return true; }
        if (event.phase == UiActionPhase::Released) {
            const bool activate = _pressed; _pressed = false; invalidate();
            if (activate) _command.invoke();
            return true;
        }
    }
    if (event.type == UiEventType::PointerDown) { _pressed = true; invalidate(); return true; }
    if (event.type == UiEventType::PointerMove) {
        const bool pressed = bounds().contains({event.x, event.y});
        if (pressed != _pressed) { _pressed = pressed; invalidate(); }
        return true;
    }
    if (event.type == UiEventType::PointerUp) {
        const bool activate = _pressed && bounds().contains({event.x, event.y});
        _pressed = false; invalidate(); if (activate) _command.invoke(); return true;
    }
    if (event.type == UiEventType::PointerCancel) { _pressed = false; invalidate(); return true; }
    return false;
}

UiMenu::UiMenu(const UiMenuItem* items, uint16_t count, UiId id)
    : UiWidget(id), _items(items), _count(count) { setFocusable(true); ensureVisible(); }

void UiMenu::setItems(const UiMenuItem* items, uint16_t count) {
    _items = items; _count = count; _current = _firstVisible = 0;
    ensureVisible(); invalidate();
}

void UiMenu::setCurrentIndex(uint16_t index) {
    if (!_count) return;
    if (index >= _count) index = _count - 1;
    _current = index; ensureVisible(); invalidate();
}

void UiMenu::ensureVisible() {
    if (!_items || !_count) return;
    if (!_items[_current].visible || !_items[_current].enabled) move(1);
    const int16_t row = _rowHeight > 0 ? _rowHeight : 36;
    uint16_t visible = bounds().h > 0 ? static_cast<uint16_t>(bounds().h / row) : 1;
    if (!visible) visible = 1;
    if (_current < _firstVisible) _firstVisible = _current;
    if (_current >= _firstVisible + visible) _firstVisible = _current - visible + 1;
}

bool UiMenu::move(int direction) {
    if (!_items || !_count) return false;
    uint16_t index = _current;
    for (uint16_t attempt = 0; attempt < _count; ++attempt) {
        if (direction > 0) {
            if (index + 1 < _count) ++index;
            else if (_wrap) index = 0; else return false;
        } else {
            if (index > 0) --index;
            else if (_wrap) index = _count - 1; else return false;
        }
        if (_items[index].visible && _items[index].enabled) {
            _current = index; ensureVisible(); invalidate(); return true;
        }
    }
    return false;
}

void UiMenu::activate() {
    if (_items && _current < _count && _items[_current].enabled && _items[_current].visible)
        _items[_current].command.invoke();
}

int16_t UiMenu::indexAt(int16_t y) const {
    const int16_t row = _rowHeight > 0 ? _rowHeight : 36;
    if (y < bounds().y || y >= bounds().bottom()) return -1;
    const int32_t index = _firstVisible + (y - bounds().y) / row;
    return index < _count ? static_cast<int16_t>(index) : -1;
}

void UiMenu::render(UiCanvas& canvas, const UiTheme& theme) {
    if (!visible()) return;
    canvas.fillRect(bounds(), theme.colors.background);
    if (!_items) return;
    const int16_t rowHeight = _rowHeight > 0 ? _rowHeight : theme.metrics.rowHeight;
    int16_t y = bounds().y;
    for (uint16_t i = _firstVisible; i < _count && y < bounds().bottom(); ++i) {
        if (!_items[i].visible) continue;
        UiRect row = {bounds().x, y, bounds().w, rowHeight};
        const bool selected = i == _current;
        canvas.fillRect(row, selected ? theme.colors.accent : theme.colors.surface);
        if (selected && focused()) canvas.drawRect(row, theme.colors.focus, theme.metrics.borderWidth);
        canvas.drawText(row.x + theme.metrics.spacingMd, row.y + theme.metrics.spacingSm,
                        _items[i].title,
                        _items[i].enabled ? theme.colors.textPrimary : theme.colors.disabled,
                        theme.bodyTextSize);
        y += rowHeight;
    }
}

bool UiMenu::onEvent(UiEvent& event) {
    if (!enabled()) return false;
    if (event.type == UiEventType::Action &&
        (event.phase == UiActionPhase::Pressed || event.phase == UiActionPhase::Repeat)) {
        if (event.action == UiAction::NavigateDown) { move(1); return true; }
        if (event.action == UiAction::NavigateUp) { move(-1); return true; }
    }
    if (event.type == UiEventType::Action && event.action == UiAction::Activate &&
        event.phase == UiActionPhase::Released) { activate(); return true; }
    if (event.type == UiEventType::Scroll && event.delta) { move(event.delta > 0 ? 1 : -1); return true; }
    if (event.type == UiEventType::PointerDown) {
        _pressedIndex = indexAt(event.y);
        // Commit the visible selection on PointerUp. PointerDown and
        // PointerUp normally arrive in separate ticks; selecting here would
        // publish an unnecessary intermediate menu frame before navigation.
        return true;
    }
    if (event.type == UiEventType::PointerUp) {
        const int16_t index = indexAt(event.y);
        if (index >= 0 && index == _pressedIndex) { setCurrentIndex(index); activate(); }
        _pressedIndex = -1; return true;
    }
    if (event.type == UiEventType::PointerCancel) { _pressedIndex = -1; return true; }
    return false;
}

void UiScrollView::setOffset(int16_t offset) {
    int32_t maximum = _contentExtent - bounds().h;
    if (maximum < 0) maximum = 0;
    if (offset < 0) offset = 0;
    if (offset > maximum) offset = static_cast<int16_t>(maximum);
    if (_offset == offset) return;
    _offset = offset;
    layout(bounds());
    invalidate();
}

bool UiScrollView::onEvent(UiEvent& event) {
    if (event.type == UiEventType::PointerDown) {
        _lastPointerY = event.y; _dragging = true; return true;
    }
    if (event.type == UiEventType::PointerMove && _dragging) {
        const int16_t delta = static_cast<int16_t>(_lastPointerY - event.y);
        _lastPointerY = event.y; setOffset(_offset + delta); return true;
    }
    if (event.type == UiEventType::PointerUp || event.type == UiEventType::PointerCancel) {
        _dragging = false; return true;
    }
    if (event.type == UiEventType::Scroll && event.delta) {
        setOffset(_offset + (event.delta > 0 ? _step : -_step));
        return true;
    }
    if (event.type == UiEventType::Action &&
        (event.phase == UiActionPhase::Pressed || event.phase == UiActionPhase::Repeat)) {
        if (event.action == UiAction::NavigateDown || event.action == UiAction::PageNext) {
            setOffset(_offset + _step); return true;
        }
        if (event.action == UiAction::NavigateUp || event.action == UiAction::PagePrevious) {
            setOffset(_offset - _step); return true;
        }
    }
    return false;
}

void UiScrollView::layout(const UiRect& value) {
    UiContainer::layout(value);
    for (UiWidget* child = firstChild(); child; child = child->nextSibling()) {
        UiRect childRect = child->bounds();
        childRect.x = value.x;
        childRect.y = value.y - _offset;
        childRect.w = value.w;
        childRect.h = _contentExtent > 0 ? _contentExtent : value.h;
        child->layout(childRect);
    }
}

void UiScrollView::render(UiCanvas& canvas, const UiTheme& theme) {
    if (!visible()) return;
    if (!canvas.save()) return;
    canvas.clipRect(bounds());
    UiContainer::render(canvas, theme);
    canvas.restore();
}

UiToggle::UiToggle(const char* text, bool checked, UiCommand command, UiId id)
    : UiWidget(id), _text(text), _command(command), _checked(checked) { setFocusable(true); }
void UiToggle::setChecked(bool checked) { if (_checked != checked) { _checked = checked; invalidate(); } }
void UiToggle::toggle() { _checked = !_checked; invalidate(); _command.invoke(); }
void UiToggle::render(UiCanvas& canvas, const UiTheme& theme) {
    canvas.fillRect(bounds(), focused() ? theme.colors.surface : theme.colors.background);
    canvas.drawText(bounds().x + theme.metrics.spacingMd, bounds().y + theme.metrics.spacingSm,
                    _text, enabled() ? theme.colors.textPrimary : theme.colors.disabled,
                    theme.bodyTextSize);
    UiRect box = {uiClamp16(bounds().right() - 34), uiClamp16(bounds().y + 6), 28, 20};
    canvas.fillRect(box, _checked ? theme.colors.accent : theme.colors.disabled);
    if (focused()) canvas.drawRect(bounds(), theme.colors.focus, theme.metrics.borderWidth);
}
bool UiToggle::onEvent(UiEvent& event) {
    if (!enabled()) return false;
    if (event.type == UiEventType::Action && event.action == UiAction::Activate &&
        event.phase == UiActionPhase::Released) {
        toggle(); return true;
    }
    if (event.type == UiEventType::PointerDown) { _pressed = true; return true; }
    if (event.type == UiEventType::PointerMove) {
        _pressed = bounds().contains({event.x, event.y}); return true;
    }
    if (event.type == UiEventType::PointerUp) {
        const bool activate = _pressed && bounds().contains({event.x, event.y});
        _pressed = false; if (activate) toggle(); return true;
    }
    if (event.type == UiEventType::PointerCancel) { _pressed = false; return true; }
    return false;
}

UiValueItem::UiValueItem(const char* text, int32_t value, int32_t minimum,
                         int32_t maximum, int32_t step, UiCommand command, UiId id)
    : UiWidget(id), _text(text), _value(value), _minimum(minimum), _maximum(maximum),
      _step(step > 0 ? step : 1), _command(command) { setFocusable(true); setValue(value); }
void UiValueItem::setValue(int32_t value) {
    if (value < _minimum) value = _minimum;
    if (value > _maximum) value = _maximum;
    if (_value != value) { _value = value; invalidate(); }
}
bool UiValueItem::adjust(int direction) {
    const int32_t prior = _value; setValue(_value + direction * _step);
    if (_value != prior) { _command.invoke(); return true; } return false;
}
void UiValueItem::render(UiCanvas& canvas, const UiTheme& theme) {
    char value[24]; snprintf(value, sizeof(value), "%ld", static_cast<long>(_value));
    canvas.fillRect(bounds(), focused() ? theme.colors.surface : theme.colors.background);
    canvas.drawText(bounds().x + theme.metrics.spacingMd, bounds().y + theme.metrics.spacingSm,
                    _text, theme.colors.textPrimary, theme.bodyTextSize);
    canvas.drawText(uiClamp16(bounds().right() - 64), bounds().y + theme.metrics.spacingSm,
                    value, theme.colors.accent, theme.bodyTextSize);
    if (focused()) canvas.drawRect(bounds(), theme.colors.focus, theme.metrics.borderWidth);
}
bool UiValueItem::onEvent(UiEvent& event) {
    if (!enabled()) return false;
    if (event.type == UiEventType::Scroll && event.delta) return adjust(event.delta > 0 ? 1 : -1);
    if (event.type != UiEventType::Action ||
        (event.phase != UiActionPhase::Pressed && event.phase != UiActionPhase::Repeat)) return false;
    if (event.action == UiAction::NavigateRight || event.action == UiAction::Increment) return adjust(1);
    if (event.action == UiAction::NavigateLeft || event.action == UiAction::Decrement) return adjust(-1);
    return false;
}

void UiProgressBar::setValue(int32_t value) {
    if (value < _minimum) value = _minimum;
    if (value > _maximum) value = _maximum;
    if (_value != value) { _value = value; invalidate(); }
}
void UiProgressBar::render(UiCanvas& canvas, const UiTheme& theme) {
    canvas.fillRect(bounds(), theme.colors.surface);
    if (_maximum <= _minimum) return;
    UiRect fill = bounds();
    fill.w = static_cast<int16_t>((static_cast<int64_t>(_value - _minimum) * bounds().w) /
                                  (_maximum - _minimum));
    canvas.fillRect(fill, theme.colors.accent);
    canvas.drawRect(bounds(), theme.colors.textSecondary, 1);
}

void UiStatusBar::render(UiCanvas& canvas, const UiTheme& theme) {
    canvas.fillRect(bounds(), theme.colors.surface);
    canvas.drawText(bounds().x + theme.metrics.spacingMd, bounds().y + theme.metrics.spacingSm,
                    _title, theme.colors.textPrimary, theme.titleTextSize);
    UiContainer::render(canvas, theme);
}

void UiDialog::render(UiCanvas& canvas, const UiTheme& theme) {
    canvas.fillRect(bounds(), theme.colors.surface);
    canvas.drawRect(bounds(), theme.colors.focus, theme.metrics.borderWidth);
    canvas.drawText(bounds().x + theme.metrics.spacingMd, bounds().y + theme.metrics.spacingMd,
                    _title, theme.colors.textPrimary, theme.titleTextSize);
    canvas.drawText(bounds().x + theme.metrics.spacingMd,
                    bounds().y + theme.metrics.titleHeight,
                    _message, theme.colors.textSecondary, theme.bodyTextSize);
    UiContainer::render(canvas, theme);
}

#endif // !ARDUINO_ARCH_NRF52 (UI layer)
