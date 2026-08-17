#if !defined(ARDUINO_ARCH_NRF52)
/* UI layer skipped on nRF52 (Seeed core uses -std=gnu++11; UI needs C++14/17
   aggregate features). Core graphics + ePaper do not depend on the UI. */
#include "UiFocusManager.h"
#include "widget/UiWidget.h"

namespace {
bool canFocus(UiWidget* widget) {
    return widget && widget->visible() && widget->enabled() && widget->focusable();
}

void walk(UiWidget& widget, UiWidget* current, UiWidget*& first,
          UiWidget*& previous, UiWidget*& next, bool& seenCurrent) {
    if (canFocus(&widget)) {
        if (!first) first = &widget;
        if (&widget == current) seenCurrent = true;
        else if (!seenCurrent) previous = &widget;
        else if (!next) next = &widget;
    }
    UiContainer* container = widget.asContainer();
    if (!container) return;
    for (UiWidget* child = container->firstChild(); child; child = child->nextSibling())
        walk(*child, current, first, previous, next, seenCurrent);
}

UiWidget* findId(UiWidget& widget, UiId id) {
    if (widget.id() == id) return &widget;
    UiContainer* container = widget.asContainer();
    if (!container) return nullptr;
    for (UiWidget* child = container->firstChild(); child; child = child->nextSibling()) {
        UiWidget* found = findId(*child, id);
        if (found) return found;
    }
    return nullptr;
}
}

bool UiFocusManager::setFocus(UiWidget* widget) {
    if (widget && !canFocus(widget)) return false;
    if (_focused == widget) return true;
    if (_focused) _focused->setFocusedInternal(false);
    _focused = widget;
    if (_focused) _focused->setFocusedInternal(true);
    return true;
}

bool UiFocusManager::focusFirst(UiWidget& root) {
    UiWidget *first = nullptr, *previous = nullptr, *next = nullptr;
    bool seen = false;
    walk(root, nullptr, first, previous, next, seen);
    return setFocus(first);
}

bool UiFocusManager::moveNext(UiWidget& root, bool wrap) {
    UiWidget *first = nullptr, *previous = nullptr, *next = nullptr;
    bool seen = false;
    walk(root, _focused, first, previous, next, seen);
    if (next) return setFocus(next);
    if (wrap && first && first != _focused) return setFocus(first);
    return false;
}

bool UiFocusManager::movePrevious(UiWidget& root, bool wrap) {
    UiWidget *first = nullptr, *previous = nullptr, *next = nullptr;
    bool seen = false;
    walk(root, _focused, first, previous, next, seen);
    if (previous) return setFocus(previous);
    if (!wrap) return false;
    UiWidget* last = nullptr;
    UiWidget *dummyFirst = nullptr, *dummyNext = nullptr;
    bool neverSeen = false;
    walk(root, nullptr, dummyFirst, last, dummyNext, neverSeen);
    if (last && last != _focused) return setFocus(last);
    return false;
}

UiWidget* UiFocusManager::findById(UiWidget& root, UiId id) const {
    return id == UI_ID_NONE ? nullptr : findId(root, id);
}

#endif // !ARDUINO_ARCH_NRF52 (UI layer)
