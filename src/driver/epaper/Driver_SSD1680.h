/**
 * @file   Driver_SSD1680.h
 * @brief  SSD1680 ePaper display driver for Seeed_GFX v2.0
 *
 * Complete driver for the SSD1680 ePaper controller, commonly used on
 * Seeed XIAO 2.9" ePaper displays (128x296).
 *
 * Supports: full refresh, fast refresh, partial refresh, and 4-level grayscale.
 *
 * Adapted from Seeed_GFX-master:
 *   TFT_Drivers/SSD1680_Defines.h, SSD1680_Init.h, SSD1680_Rotation.h
 */

#ifndef SEEED_GFX_DRIVER_SSD1680_H
#define SEEED_GFX_DRIVER_SSD1680_H

#include <Arduino.h>
#include "../../core/Driver.h"

// SSD1680 Command Constants
#define SSD1680_DRVOUT     0x01  // Driver output control
#define SSD1680_GSCAN      0x0F  // Gate scan start position
#define SSD1680_SLPIN      0x10  // Deep sleep mode
#define SSD1680_DTM        0x11  // Data entry mode
#define SSD1680_SWRESET    0x12  // Software reset
#define SSD1680_TEMP       0x18  // Temperature sensor control
#define SSD1680_MASTER     0x20  // Master activation
#define SSD1680_DISPCTRL   0x22  // Display update control
#define SSD1680_RAMBW      0x24  // Write B/W RAM
#define SSD1680_RAMRED     0x26  // Write Red RAM
#define SSD1680_LUTOPT     0x1A  // LUT option / temperature
#define SSD1680_PTLIN      0x3C  // Border waveform / partial in
#define SSD1680_SETX       0x44  // Set RAM X start/end
#define SSD1680_SETY       0x45  // Set RAM Y start/end
#define SSD1680_RAMXCNT    0x4E  // Set RAM X counter
#define SSD1680_RAMYCNT    0x4F  // Set RAM Y counter

// Data entry mode flags for setRotation
#define SSD1680_DTM_YDEC_XINC  0x01  // Y decrement, X increment (0 deg)
#define SSD1680_DTM_YINC_XDEC  0x02  // Y increment, X decrement (180 deg)

class Driver_SSD1680 : public IDriver {
public:
    /**
     * @param w  Display width in pixels (default 128 for 2.9" panel)
     * @param h  Display height in pixels (default 296 for 2.9" panel)
     */
    Driver_SSD1680(uint16_t w = 128, uint16_t h = 296);

    // ---- IDriver interface ------------------------------------------------
    const char* name() const override { return "SSD1680"; }
    uint16_t width() const override { return _width; }
    uint16_t height() const override { return _height; }
    uint16_t nativeWidth() const override { return _init_width; }
    uint16_t nativeHeight() const override { return _init_height; }
    uint8_t  colorDepth() const override { return 1; }
    bool supportsDeepSleep() const override { return true; }

    bool     init(IBus& bus) override;
    void     setRotation(uint8_t rotation) override;
    uint8_t  rotation() const override { return _rotation; }
    void     invertDisplay(bool invert) override;
    void     displayOn() override;
    void     displayOff() override;
    void     setAddrWindow(uint16_t xs, uint16_t ys,
                           uint16_t xe, uint16_t ye) override;
    void     writePixel(uint16_t color) override;
    void     writePixels(const uint16_t* data, size_t len) override;
    void     writeFill(uint16_t color, size_t len) override;
    void     sleep() override;
    void     wake() override;
    IBus&    bus() override { return *_bus; }

    // ---- Pin configuration (called by Panel/Board before init) -------------
    void setBusyPin(int pin) override   { _busy_pin = static_cast<int8_t>(pin); }
    void setResetPin(int pin) override  { _rst_pin = static_cast<int8_t>(pin); }
    void setEnablePin(int8_t pin) { _enable_pin = pin; }

    // ---- ePaper-specific public API ---------------------------------------
    // (Original macro equivalents)

    /// Full display update (slow, lowest ghosting)
    bool supportsPartialRefresh() const override { return true; }
    bool supportsFastRefresh() const override { return true; }
    bool supportsGrayRefresh(uint8_t levels) const override { return levels == 4; }
    void update() override;

    /// Fast display update (uses LUT, more ghosting)
    void updateFast() override;

    /// Partial update (only refreshes the current window)
    void updatePartial() override;

    /// Grayscale update (4-level gray)
    void updateGray() override;

    /// Initialize for partial update mode
    void initPartial();

    /// Initialize for 4-level grayscale mode
    void initGray();

    /// Wake up and initialize for partial mode
    void wakePartial() override;

    /// Wake up and initialize for grayscale mode
    void wakeGray() override;

    /// Push B/W pixel data to new-color RAM (0x24)
    void pushNewColors(uint16_t w, uint16_t h, const uint8_t* colors);
    void pushNewColors(const uint8_t* colors, size_t len) override {
        if (!_bus || !colors) return;
        _bus->writeCommand(SSD1680_RAMBW); _bus->writeData(colors, len);
    }

    /// Push B/W pixel data to old-color RAM (0x26)
    void pushOldColors(uint16_t w, uint16_t h, const uint8_t* colors);
    void pushOldColors(const uint8_t* colors, size_t len) override {
        if (!_bus || !colors) return;
        _bus->writeCommand(SSD1680_RAMRED); _bus->writeData(colors, len);
    }

    /// Push B/W pixel data to new-color RAM with horizontal bit-flip per byte
    void pushNewColorsFlip(uint16_t w, uint16_t h, const uint8_t* colors);

    /// Push B/W pixel data to old-color RAM with horizontal bit-flip per byte
    void pushOldColorsFlip(uint16_t w, uint16_t h, const uint8_t* colors);

    /// Push 4-level grayscale data to both RAM planes (0x24 + 0x26)
    /// @param colors  Input buffer: 4 bytes per 8 pixels (2-bit gray per pixel)
    ///                 Total size = 4 * (w * h / 8) = w * h / 2 bytes
    void pushNewGrayColors(uint16_t w, uint16_t h, const uint8_t* colors);
    void pushGrayColors(const uint8_t* colors, size_t len) override {
        (void)len;
        pushNewGrayColors(_init_width, _init_height, colors);
    }

    /// Push 4-level grayscale data (same as pushNewGrayColors; flip not
    /// implemented for gray mode)
    void pushNewGrayColorsFlip(uint16_t w, uint16_t h, const uint8_t* colors);

private:
    /// Poll the BUSY pin until the controller is ready (or no-op if no pin)
    void checkBusy();

    /// Hardware + software reset sequence
    void reset();

    uint16_t _init_width  = 128;
    uint16_t _init_height = 296;
    int8_t   _busy_pin    = -1;
    int8_t   _rst_pin     = -1;
    int8_t   _enable_pin  = -1;
};

#endif // SEEED_GFX_DRIVER_SSD1680_H
