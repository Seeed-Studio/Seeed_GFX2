#ifndef SEEED_UI_TYPES_H
#define SEEED_UI_TYPES_H

#include <stdint.h>
#include <limits.h>

using UiId = uint16_t;
static constexpr UiId UI_ID_NONE = 0;

struct UiPoint {
    int16_t x;
    int16_t y;
    constexpr UiPoint(int16_t xValue = 0, int16_t yValue = 0)
        : x(xValue), y(yValue) {}
};

struct UiSize {
    int16_t w;
    int16_t h;
    constexpr UiSize(int16_t width = 0, int16_t height = 0)
        : w(width), h(height) {}
};

struct UiInsets {
    int16_t left;
    int16_t top;
    int16_t right;
    int16_t bottom;
    constexpr UiInsets(int16_t leftValue = 0, int16_t topValue = 0,
                       int16_t rightValue = 0, int16_t bottomValue = 0)
        : left(leftValue), top(topValue), right(rightValue),
          bottom(bottomValue) {}
};

struct UiRect {
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
    constexpr UiRect(int16_t xValue = 0, int16_t yValue = 0,
                     int16_t width = 0, int16_t height = 0)
        : x(xValue), y(yValue), w(width), h(height) {}

    bool empty() const { return w <= 0 || h <= 0; }
    int32_t right() const { return static_cast<int32_t>(x) + w; }
    int32_t bottom() const { return static_cast<int32_t>(y) + h; }
    bool contains(UiPoint p) const {
        return !empty() && p.x >= x && p.y >= y &&
               static_cast<int32_t>(p.x) < right() &&
               static_cast<int32_t>(p.y) < bottom();
    }
    bool intersects(const UiRect& other) const {
        return !empty() && !other.empty() && x < other.right() &&
               other.x < right() && y < other.bottom() && other.y < bottom();
    }
    UiRect intersection(const UiRect& other) const;
    UiRect united(const UiRect& other) const;
};

inline int16_t uiClamp16(int32_t value) {
    if (value < INT16_MIN) return INT16_MIN;
    if (value > INT16_MAX) return INT16_MAX;
    return static_cast<int16_t>(value);
}

inline UiRect UiRect::intersection(const UiRect& other) const {
    const int32_t nx = x > other.x ? x : other.x;
    const int32_t ny = y > other.y ? y : other.y;
    const int32_t nr = right() < other.right() ? right() : other.right();
    const int32_t nb = bottom() < other.bottom() ? bottom() : other.bottom();
    if (nr <= nx || nb <= ny) return UiRect();
    UiRect result;
    result.x = uiClamp16(nx); result.y = uiClamp16(ny);
    result.w = uiClamp16(nr - nx); result.h = uiClamp16(nb - ny);
    return result;
}

inline UiRect UiRect::united(const UiRect& other) const {
    if (empty()) return other;
    if (other.empty()) return *this;
    const int32_t nx = x < other.x ? x : other.x;
    const int32_t ny = y < other.y ? y : other.y;
    const int32_t nr = right() > other.right() ? right() : other.right();
    const int32_t nb = bottom() > other.bottom() ? bottom() : other.bottom();
    UiRect result;
    result.x = uiClamp16(nx); result.y = uiClamp16(ny);
    result.w = uiClamp16(nr - nx); result.h = uiClamp16(nb - ny);
    return result;
}

inline bool uiTimeReached(uint32_t now, uint32_t deadline) {
    return static_cast<int32_t>(now - deadline) >= 0;
}

#endif
