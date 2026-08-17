/**
 * @file Driver_SPD2010.h
 * @brief SPD2010 412x412 QSPI LCD driver used by SenseCAP Watcher.
 */

#ifndef SEEED_GFX_DRIVER_SPD2010_H
#define SEEED_GFX_DRIVER_SPD2010_H

#include "../../core/Driver.h"

class Bus_ESP32QSPI;

class Driver_SPD2010 : public IDriver {
public:
    Driver_SPD2010(uint16_t width = 412, uint16_t height = 412);
    ~Driver_SPD2010() override;

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
    const char* name() const override { return "SPD2010"; }
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
    void sleep() override;
    void wake() override;
    IBus& bus() override { return *_bus; }

private:
    bool runInitSequence();
    bool sendWindow(uint16_t x0, uint16_t y0,
                    uint16_t x1, uint16_t y1);
    bool storeAndFlush(const uint16_t* data, uint16_t count,
                       bool fill, uint16_t fillColor);
    bool flushNativeRect(uint16_t x0, uint16_t y0,
                         uint16_t x1, uint16_t y1);
    void logicalToNative(uint16_t x, uint16_t y,
                         uint16_t& nativeX, uint16_t& nativeY) const;
    void advance(uint16_t count);
    uint16_t contiguousCount(size_t remaining) const;

    uint16_t _width;
    uint16_t _height;
    Bus_ESP32QSPI* _qspi;
    uint16_t* _frameBuffer;
    uint16_t* _transferBuffer;
    size_t _transferCapacityPixels;
    uint16_t _windowX0;
    uint16_t _windowY0;
    uint16_t _windowX1;
    uint16_t _windowY1;
    uint16_t _cursorX;
    uint16_t _cursorY;
    bool _sleeping;
};

#endif
