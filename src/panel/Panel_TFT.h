/**
 * @file   Panel_TFT.h
 * @brief  TFT LCD panel implementation for Seeed_GFX v2.0
 *
 * The Panel_TFT class bridges the Driver and Graphics layers for TFT LCD displays.
 * It manages backlight control, delegates pixel operations to the driver,
 * and handles initialization of the full hardware stack.
 */

#ifndef SEEED_GFX_PANEL_TFT_H
#define SEEED_GFX_PANEL_TFT_H

#include <Arduino.h>
#include "../core/Panel.h"
#include "../core/Board.h"

class Panel_TFT : public IPanel {
public:
    /** Constructor with explicit components
     *  @param driver  Reference to the display driver IC
     *  @param bus     Reference to the communication bus
     *  @param board   Optional board reference (for backlight control)
     */
    Panel_TFT(IDriver& driver, IBus& bus, IBoard* board = nullptr,
              uint8_t initialRotation = 0);

    virtual ~Panel_TFT();

    // IPanel implementation
    bool begin() override;
    GfxResult end() override;
    GfxResult lastResult() const override { return _lastResult; }

    uint16_t width() const override { return _driver.width(); }
    uint16_t height() const override { return _driver.height(); }
    uint8_t colorDepth() const override { return _driver.colorDepth(); }

    void setRotation(uint8_t r) override { _driver.setRotation(r); }
    uint8_t rotation() const override { return _driver.rotation(); }
    void invertDisplay(bool invert) override { _driver.invertDisplay(invert); }

    void setAddrWindow(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye) override {
        _driver.setAddrWindow(xs, ys, xe, ye);
    }
    void writePixel(uint16_t color) override { _driver.writePixel(color); }
    void writePixels(const uint16_t* data, size_t len) override { _driver.writePixels(data, len); }
    void writeFill(uint16_t color, size_t len) override { _driver.writeFill(color, len); }
    uint16_t readPixel(uint16_t x, uint16_t y) override { return _driver.readPixel(x, y); }

    void sleep() override;
    void wake() override;

    void setBacklight(uint8_t brightness) override;
    uint8_t backlight() const override { return _backlight; }

    IDriver& driver() override { return _driver; }
    DisplayCapabilities capabilities() const override {
        DisplayCapabilities caps;
        caps.technology = DisplayTechnology::TFT;
        caps.nativeFormat = PixelFormat::RGB565;
        caps.readback = _driver.supportsReadback() && _bus.capabilities().readable;
        caps.backlight = (_board != nullptr && _board->pinBacklight() >= 0);
        return caps;
    }

protected:
    IDriver& _driver;
    IBus&    _bus;
    IBoard*  _board;
    uint8_t  _backlight;
    bool     _initialized;
    uint8_t  _initialRotation;
    GfxResult _lastResult;
};

#endif // SEEED_GFX_PANEL_TFT_H
