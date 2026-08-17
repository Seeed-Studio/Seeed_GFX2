/**
 * @file   Driver_JD79676.h
 * @brief  JD79676 ePaper display driver for Seeed_GFX v2.0
 *
 * Seeed-compatible driver for the 2.13" GDEY0213F51-class BWRY panel.
 * The library uses a 4bpp staging buffer, then converts each palette nibble
 * to the controller's 2-bit Black/White/Red/Yellow transfer code. This is
 * not a 16-level grayscale driver.
 */

#ifndef SEEED_GFX_DRIVER_JD79676_H
#define SEEED_GFX_DRIVER_JD79676_H

#include <Arduino.h>
#include "../../core/Driver.h"

class Driver_JD79676 : public IDriver {
public:
    Driver_JD79676(uint16_t w = 800, uint16_t h = 480, int8_t busyPin = -1);

    const char* name() const override { return "JD79676"; }
    uint16_t width() const override { return _width; }
    uint16_t height() const override { return _height; }
    uint8_t colorDepth() const override { return 4; }
    bool supportsBWRYEPaper() const override { return true; }
    bool supportsDeepSleep() const override { return true; }

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
    void sleep() override;
    void wake() override;
    IBus& bus() override { return *_bus; }

    // ePaper-specific
    void setBusyPin(int pin) override { _busyPin = static_cast<int8_t>(pin); }
    void update() override;
    void pushColors(const uint8_t* data, uint16_t w, uint16_t h);
    void pushNewColors(const uint8_t* data, size_t len) override {
        (void)len; pushColors(data, _init_width, _init_height);
    }
    void pushOldColors(const uint8_t* data, size_t len) override {
        (void)len; pushOldColors(data, _init_width, _init_height);
    }
    void pushColorsFlip(const uint8_t* data, uint16_t w, uint16_t h);
    void pushOldColors(const uint8_t* data, uint16_t w, uint16_t h);
    void pushOldColorsFlip(const uint8_t* data, uint16_t w, uint16_t h);
    void setTemp(uint8_t temp) { (void)temp; }
    void setTemperature(int8_t temp) override { setTemp(static_cast<uint8_t>(temp)); }

private:
    void busyWait();
    uint8_t colorGet(uint8_t color);
    uint16_t _init_width, _init_height;
    int8_t _busyPin;
};

#endif // SEEED_GFX_DRIVER_JD79676_H
