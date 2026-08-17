#if !defined(ARDUINO_ARCH_NRF52)
/* UI layer skipped on nRF52 (Seeed core uses -std=gnu++11; UI needs C++14/17
   aggregate features). Core graphics + ePaper do not depend on the UI. */
#include "UiDirtyRegion.h"

void UiDirtyRegion::add(const UiRect& rect) {
    UiRect clipped = _screen.empty() ? rect : rect.intersection(_screen);
    if (clipped.empty()) return;
    for (size_t i = 0; i < _count; ++i) {
        UiRect expanded = _rects[i];
        expanded.x--; expanded.y--; expanded.w += 2; expanded.h += 2;
        if (expanded.intersects(clipped)) {
            _rects[i] = _rects[i].united(clipped);
            return;
        }
    }
    if (_count < SEEED_UI_MAX_DIRTY_RECTS) {
        _rects[_count++] = clipped;
        return;
    }
    invalidateAll();
}

void UiDirtyRegion::invalidateAll() {
    _count = 0;
    if (!_screen.empty()) _rects[_count++] = _screen;
}

#endif // !ARDUINO_ARCH_NRF52 (UI layer)
