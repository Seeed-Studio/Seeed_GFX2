/**
 * @file   Driver_ILI9341.h
 * @brief  ILI9341 display driver for Seeed_GFX v2.0
 *
 * One of the most common TFT LCD controllers.
 * Default resolution: 320x240 (QVGA).
 */

#ifndef SEEED_GFX_DRIVER_ILI9341_H
#define SEEED_GFX_DRIVER_ILI9341_H

#include <Arduino.h>
#include "../../core/Driver.h"

// ILI9341 command constants
#define ILI9341_NOP     0x00
#define ILI9341_SWRESET 0x01
#define ILI9341_RDDID   0x04
#define ILI9341_SLPIN   0x10
#define ILI9341_SLPOUT  0x11
#define ILI9341_NORON   0x13
#define ILI9341_INVOFF  0x20
#define ILI9341_INVON   0x21
#define ILI9341_DISPOFF 0x28
#define ILI9341_DISPON  0x29
#define ILI9341_CASET   0x2A
#define ILI9341_PASET   0x2B
#define ILI9341_RAMWR   0x2C
#define ILI9341_MADCTL  0x36
#define ILI9341_COLMOD  0x3A

// ILI9341 specific
#define ILI9341_PWCTR1  0xC0
#define ILI9341_PWCTR2  0xC1
#define ILI9341_VMCTR1  0xC5
#define ILI9341_VMCTR2  0xC7
#define ILI9341_GMCTRP1 0xE0
#define ILI9341_GMCTRN1 0xE1

// MADCTL flags
#define TFT_MAD_MY  0x80
#define TFT_MAD_MX  0x40
#define TFT_MAD_MV  0x20
#define TFT_MAD_ML  0x10
#define TFT_MAD_RGB 0x00
#define TFT_MAD_BGR 0x08
#define TFT_MAD_MH  0x04

class Driver_ILI9341 : public IDriver {
public:
    Driver_ILI9341(uint16_t w = 320, uint16_t h = 240, uint8_t rgbOrder = TFT_MAD_BGR);

    const char* name() const override { return "ILI9341"; }
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
    uint8_t  _rgbOrder;
};

#endif // SEEED_GFX_DRIVER_ILI9341_H
