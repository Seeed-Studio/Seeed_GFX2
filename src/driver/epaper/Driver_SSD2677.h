/**
 * @file   Driver_SSD2677.h
 * @brief  SSD2677 ePaper display driver for Seeed_GFX v2.0
 *
 * 800x480 MONOCHROME (1-bit black/white) ePaper driver, aligned with the
 * reTerminal Sticky product firmware (seeed_epaper/driver/ssd2677.c).
 * The panel scans 680 gate lines (RES = 800x680) even though only 800x480
 * are visible; init writes RES=800x680 exactly like the firmware, and each
 * full refresh latches a temperature-selected waveform before streaming the
 * 1bpp framebuffer expanded to the controller's 2bpp data format.
 * Gray4 support mirrors the firmware's gray path: the 4bpp framebuffer is
 * mapped to 2bpp gray pixel codes and latched with the two-bucket gray4
 * waveform; gray refresh reuses wake()/update() because the firmware has no
 * gray-specific init or drive phase. Partial-refresh mode of the firmware
 * is not implemented yet.
 */

#ifndef SEEED_GFX_DRIVER_SSD2677_H
#define SEEED_GFX_DRIVER_SSD2677_H

#include <Arduino.h>
#include "../../core/Driver.h"

class Driver_SSD2677 : public IDriver {
public:
    Driver_SSD2677(uint16_t w = 800, uint16_t h = 480, int8_t busyPin = -1);

    const char* name() const override { return "SSD2677"; }
    uint16_t width() const override { return _width; }
    uint16_t height() const override { return _height; }
    uint8_t colorDepth() const override { return 1; }
    bool supportsBWRYEPaper() const override { return false; }
    bool supportsDeepSleep() const override { return true; }
    bool supportsGrayRefresh(uint8_t levels) const override {
        return levels == 4;
    }

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
    // data: 1bpp packed framebuffer (bit=1 black, bit=0 white, MSB = leftmost)
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
    // Gray4: 4bpp packed framebuffer (two indexes per byte, high nibble =
    // leftmost pixel, 0=black ... 3=white), converted to the controller's
    // 2bpp gray pixel codes and packed row-by-row right-to-left so the net
    // transform matches Driver_SSD1677::pushGrayColors exactly (Sticky mixes
    // both controllers behind one board config). Panel_EPaper applies its
    // horizontal-mirror buffer flip before calling the len-based override,
    // so no flip variant is needed here.
    void pushGrayColors(const uint8_t* data, uint16_t w, uint16_t h);
    void pushGrayColors(const uint8_t* data, size_t len) override {
        (void)len; pushGrayColors(data, _init_width, _init_height);
    }
    void setTemp(uint8_t temp) { (void)temp; }
    void setTemperature(int8_t temp) override { setTemp(static_cast<uint8_t>(temp)); }

private:
    void busyWait();
    uint8_t readPanelTemperature();
    void latchFullWaveform();
    void latchGray4Waveform();
    uint16_t _init_width, _init_height;
    int8_t _busyPin;
    bool _refreshPowered; // panel power (0x04) state across refresh phases
};

#endif // SEEED_GFX_DRIVER_SSD2677_H
