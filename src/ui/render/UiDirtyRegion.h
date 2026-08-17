#ifndef SEEED_UI_DIRTY_REGION_H
#define SEEED_UI_DIRTY_REGION_H

#include <stddef.h>
#include "../UiConfig.h"
#include "../UiTypes.h"

class IUiInvalidationSink {
public:
    virtual ~IUiInvalidationSink() = default;
    virtual void invalidateRect(const UiRect& rect) = 0;
};

class UiDirtyRegion {
public:
    explicit UiDirtyRegion(UiRect screen = UiRect()) : _screen(screen) {}
    void setScreen(const UiRect& screen) { _screen = screen; clear(); }
    void add(const UiRect& rect);
    void invalidateAll();
    void clear() { _count = 0; }
    size_t count() const { return _count; }
    const UiRect& at(size_t index) const { return _rects[index]; }

private:
    UiRect _screen;
    UiRect _rects[SEEED_UI_MAX_DIRTY_RECTS] = {};
    size_t _count = 0;
};

#endif
