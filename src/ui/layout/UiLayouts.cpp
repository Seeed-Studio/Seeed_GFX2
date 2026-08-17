#if !defined(ARDUINO_ARCH_NRF52)
/* UI layer skipped on nRF52 (Seeed core uses -std=gnu++11; UI needs C++14/17
   aggregate features). Core graphics + ePaper do not depend on the UI. */
#include "UiLayouts.h"

void UiLinearLayout::layout(const UiRect& value) {
    UiContainer::layout(value);
    int16_t cursorX = value.x + _padding.left;
    int16_t cursorY = value.y + _padding.top;
    const int16_t innerW = value.w - _padding.left - _padding.right;
    const int16_t innerH = value.h - _padding.top - _padding.bottom;
    for (UiWidget* child = firstChild(); child; child = child->nextSibling()) {
        if (!child->visible()) continue;
        UiRect childRect;
        childRect.x = cursorX; childRect.y = cursorY;
        if (_orientation == UiOrientation::Vertical) {
            childRect.w = innerW; childRect.h = _itemExtent;
            cursorY += _itemExtent + _spacing;
        } else {
            childRect.w = _itemExtent; childRect.h = innerH;
            cursorX += _itemExtent + _spacing;
        }
        child->layout(childRect);
    }
}

void UiStackLayout::layout(const UiRect& value) {
    UiContainer::layout(value);
    for (UiWidget* child = firstChild(); child; child = child->nextSibling())
        if (child->visible()) child->layout(value);
}

#endif // !ARDUINO_ARCH_NRF52 (UI layer)
