/**
 * @file   Driver_ST7735.h
 * @brief  ST7735 display driver for Seeed_GFX v2.0
 *
 * Common small TFT LCD controller. Typical resolutions: 128x160, 128x128, 80x160.
 */

#ifndef SEEED_GFX_DRIVER_ST7735_H
#define SEEED_GFX_DRIVER_ST7735_H

#include <Arduino.h>
#include "../../core/Driver.h"

class Driver_ST7735 : public IDriver {
public:
    enum Variant { GREEN_TAB, RED_TAB, BLACK_TAB, INITB, GREEN_TAB2, GREEN_TAB3,
                   GREEN_TAB128, GREEN_TAB160x80, RED_TAB160x80 };

    Driver_ST7735(uint16_t w = 128, uint16_t h = 160, Variant v = GREEN_TAB);

    const char* name() const override { return "ST7735"; }
    uint16_t width() const override { return _width; }
    uint16_t height() const override { return _height; }
    uint8_t colorDepth() const override { return 16; }
    bool supportsReadback() const override { return true; }

    bool init(IBus& bus) override;
    void setRotation(uint8_t rotation) override;
    uint8_t rotation() const override { return _rotation; }
    void invertDisplay(bool invert) override;
    void displayOn() override;
    void displayOff() override;
    void setAddrWindow(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye) override;
    void writePixel(uint16_t color) override;
    void writePixels(const uint16_t* data, size_t len) override;
    void writeFill(uint16_t color, size_t len) override;
    uint16_t readPixel(uint16_t x, uint16_t y) override;
    void sleep() override;
    void wake() override;
    IBus& bus() override { return *_bus; }

private:
    uint16_t _init_width, _init_height;
    Variant  _variant;
    int16_t  _colstart, _rowstart, _colstart2, _rowstart2;
};

#endif // SEEED_GFX_DRIVER_ST7735_H
