#ifndef SEEED_UI_IMAGE_WIDGET_H
#define SEEED_UI_IMAGE_WIDGET_H

#include "UiWidget.h"

class UiImage : public UiWidget {
public:
    UiImage(const uint16_t* pixels = nullptr, uint16_t width = 0,
            uint16_t height = 0, UiId id = UI_ID_NONE)
        : UiWidget(id), _pixels(pixels), _width(width), _height(height) {}
    void setImage(const uint16_t* pixels, uint16_t width, uint16_t height) {
        _pixels = pixels; _width = width; _height = height; invalidate();
    }
    void render(UiCanvas& canvas, const UiTheme& theme) override;
private:
    const uint16_t* _pixels;
    uint16_t _width, _height;
};

#endif
