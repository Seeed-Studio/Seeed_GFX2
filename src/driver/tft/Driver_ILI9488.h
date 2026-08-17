/**
 * @file   Driver_ILI9488.h
 * @brief  ILI9488 display driver for Seeed_GFX v2.0
 *
 * 480x320 TFT LCD controller. Used on reTerminal and some XIAO TFT shields.
 */

#ifndef SEEED_GFX_DRIVER_ILI9488_H
#define SEEED_GFX_DRIVER_ILI9488_H

#include <Arduino.h>
#include "../../core/Driver.h"

class Driver_ILI9488 : public IDriver {
public:
    Driver_ILI9488(uint16_t w = 480, uint16_t h = 320);

    const char* name() const override { return "ILI9488"; }
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
};

#endif // SEEED_GFX_DRIVER_ILI9488_H
