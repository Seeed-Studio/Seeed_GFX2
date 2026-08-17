#ifndef SEEED_UI_LAYOUTS_H
#define SEEED_UI_LAYOUTS_H

#include "../widget/UiWidget.h"

enum class UiOrientation : uint8_t { Horizontal = 0, Vertical };

class UiLinearLayout : public UiContainer {
public:
    UiLinearLayout(UiOrientation orientation = UiOrientation::Vertical,
                   int16_t itemExtent = 36, int16_t spacing = 0,
                   UiId id = UI_ID_NONE)
        : UiContainer(id), _orientation(orientation), _itemExtent(itemExtent),
          _spacing(spacing) {}
    void setPadding(UiInsets padding) { _padding = padding; }
    void layout(const UiRect& bounds) override;

private:
    UiOrientation _orientation;
    int16_t _itemExtent;
    int16_t _spacing;
    UiInsets _padding;
};

class UiStackLayout : public UiContainer {
public:
    explicit UiStackLayout(UiId id = UI_ID_NONE) : UiContainer(id) {}
    void layout(const UiRect& bounds) override;
};

#endif
