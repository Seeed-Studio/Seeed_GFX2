/**
 * @file   Driver_SSD1351.h
 * @brief  SSD1351 OLED display driver for Seeed_GFX v2.0
 *
 * 128x128 16-bit color OLED controller.
 */

#ifndef SEEED_GFX_DRIVER_SSD1351_H
#define SEEED_GFX_DRIVER_SSD1351_H

#include <Arduino.h>
#include "../../core/Driver.h"

class Driver_SSD1351 : public IDriver {
public:
    Driver_SSD1351(uint16_t w = 128, uint16_t h = 128);

    const char* name() const override { return "SSD1351"; }
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

#endif // SEEED_GFX_DRIVER_SSD1351_H
