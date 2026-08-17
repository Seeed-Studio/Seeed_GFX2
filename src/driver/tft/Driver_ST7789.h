/**
 * @file   Driver_ST7789.h
 * @brief  ST7789 display driver for Seeed_GFX v2.0
 *
 * Supports ST7789 TFT LCD controller.
 * Common resolutions: 80x160, 240x240, 240x320, 172x320, 135x240, 240x280.
 * The XIAO 0.96" 80x160 panel uses its verified Arduino_GFX initialization
 * profile; other sizes use the generic Seeed/TFT_eSPI ST7789 profile.
 */

#ifndef SEEED_GFX_DRIVER_ST7789_H
#define SEEED_GFX_DRIVER_ST7789_H

#include <Arduino.h>
#include "../../core/Driver.h"

// ST7789 command constants
#define ST7789_NOP          0x00
#define ST7789_SWRESET      0x01
#define ST7789_SLPIN        0x10
#define ST7789_SLPOUT       0x11
#define ST7789_NORON        0x13
#define ST7789_INVOFF       0x20
#define ST7789_INVON        0x21
#define ST7789_DISPOFF      0x28
#define ST7789_DISPON       0x29
#define ST7789_CASET        0x2A
#define ST7789_RASET        0x2B
#define ST7789_RAMWR        0x2C
#define ST7789_MADCTL       0x36
#define ST7789_COLMOD       0x3A
#define ST7789_RAMCTRL      0xB0
#define ST7789_PORCTRL      0xB2
#define ST7789_GCTRL        0xB7
#define ST7789_VCOMS        0xBB
#define ST7789_LCMCTRL      0xC0
#define ST7789_VDVVRHEN     0xC2
#define ST7789_VRHS         0xC3
#define ST7789_VDVSET       0xC4
#define ST7789_FRCTR2       0xC6
#define ST7789_PWCTRL1      0xD0
#define ST7789_PVGAMCTRL    0xE0
#define ST7789_NVGAMCTRL    0xE1

// MADCTL flags
#define TFT_MAD_MY  0x80
#define TFT_MAD_MX  0x40
#define TFT_MAD_MV  0x20
#define TFT_MAD_ML  0x10
#define TFT_MAD_RGB 0x00
#define TFT_MAD_BGR 0x08
#define TFT_MAD_MH  0x04
#define TFT_MAD_SS  0x02
#define TFT_MAD_GS  0x01

class Driver_ST7789 : public IDriver {
public:
    /** Constructor
     *  @param w         Display width (portrait mode)
     *  @param h         Display height (portrait mode)
     *  @param rgbOrder  Color order: TFT_RGB or TFT_BGR
     */
    Driver_ST7789(uint16_t w = 240, uint16_t h = 320, uint8_t rgbOrder = TFT_MAD_RGB);

    // IDriver implementation
    const char* name() const override { return "ST7789"; }
    uint8_t rgbOrder() const { return _rgbOrder; }
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
    int16_t  _colstart;
    int16_t  _rowstart;
};

#endif // SEEED_GFX_DRIVER_ST7789_H
