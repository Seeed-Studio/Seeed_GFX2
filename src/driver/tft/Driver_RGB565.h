/**
 * @file Driver_RGB565.h
 * @brief Framebuffer driver for ESP32-S3 RGB/DPI panels.
 */

#ifndef SEEED_GFX_DRIVER_RGB565_H
#define SEEED_GFX_DRIVER_RGB565_H

#include "../../core/Driver.h"

class Bus_ESP32RGB;

class Driver_RGB565 : public IDriver {
public:
    Driver_RGB565(uint16_t width, uint16_t height,
                  const char* driverName = "RGB565");

    bool init(IBus& bus) override;
    uint16_t width() const override {
        return (_rotation & 1U) ? _height : _width;
    }
    uint16_t height() const override {
        return (_rotation & 1U) ? _width : _height;
    }
    uint16_t nativeWidth() const override { return _width; }
    uint16_t nativeHeight() const override { return _height; }
    uint8_t colorDepth() const override { return 16; }
    const char* name() const override { return _name; }
    bool supportsReadback() const override { return true; }

    void setRotation(uint8_t rotation) override;
    uint8_t rotation() const override { return _rotation; }
    void invertDisplay(bool invert) override;
    void displayOn() override;
    void displayOff() override;
    void setAddrWindow(uint16_t xs, uint16_t ys,
                       uint16_t xe, uint16_t ye) override;
    void writePixel(uint16_t color) override;
    void writePixels(const uint16_t* data, size_t len) override;
    void writeFill(uint16_t color, size_t len) override;
    uint16_t readPixel(uint16_t x, uint16_t y) override;
    void sleep() override { displayOff(); }
    void wake() override { displayOn(); }
    IBus& bus() override { return *_bus; }

private:
    bool mapPoint(uint16_t x, uint16_t y, uint16_t& nativeX,
                  uint16_t& nativeY) const;
    void advanceCursor();
    uint16_t outputColor(uint16_t color) const {
        return _inverted ? static_cast<uint16_t>(~color) : color;
    }

    uint16_t _width;
    uint16_t _height;
    const char* _name;
    Bus_ESP32RGB* _rgbBus;
    uint16_t _windowX0;
    uint16_t _windowY0;
    uint16_t _windowX1;
    uint16_t _windowY1;
    uint16_t _cursorX;
    uint16_t _cursorY;
    bool _inverted;
};

#endif
