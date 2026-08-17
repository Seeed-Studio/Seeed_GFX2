/**
 * @file   Driver_GC9A01.h
 * @brief  GC9A01 round display driver for Seeed_GFX v2.0
 *
 * Used on Seeed XIAO Round Display (1.28" round TFT, 240x240).
 */

#ifndef SEEED_GFX_DRIVER_GC9A01_H
#define SEEED_GFX_DRIVER_GC9A01_H

#include <Arduino.h>
#include "../../core/Driver.h"

// GC9A01 command constants
#define GC9A01_SWRESET  0x01
#define GC9A01_SLPIN    0x10
#define GC9A01_SLPOUT   0x11
#define GC9A01_INVOFF   0x20
#define GC9A01_INVON    0x21
#define GC9A01_DISPOFF  0x28
#define GC9A01_DISPON   0x29
#define GC9A01_CASET    0x2A
#define GC9A01_RASET    0x2B
#define GC9A01_RAMWR    0x2C
#define GC9A01_MADCTL   0x36
#define GC9A01_COLMOD   0x3A

#define TFT_MAD_MY  0x80
#define TFT_MAD_MX  0x40
#define TFT_MAD_MV  0x20
#define TFT_MAD_BGR 0x08

class Driver_GC9A01 : public IDriver {
public:
    Driver_GC9A01(uint16_t w = 240, uint16_t h = 240);

    const char* name() const override { return "GC9A01"; }
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
    uint16_t _init_width;
    uint16_t _init_height;
};

#endif // SEEED_GFX_DRIVER_GC9A01_H
