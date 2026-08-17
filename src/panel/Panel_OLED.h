/**
 * @file   Panel_OLED.h
 * @brief  OLED panel implementation for Seeed_GFX v2.0
 *
 * Handles OLED-specific behavior: brightness control,
 * burn-in prevention, and I2C/SPI communication.
 */

#ifndef SEEED_GFX_PANEL_OLED_H
#define SEEED_GFX_PANEL_OLED_H

#include <Arduino.h>
#include "../core/Panel.h"

class Panel_OLED : public IPanel {
public:
    Panel_OLED(IDriver& driver, IBus& bus);

    virtual ~Panel_OLED();

    bool begin() override;
    GfxResult end() override;
    GfxResult lastResult() const override { return _lastResult; }
    uint16_t width() const override { return _driver.width(); }
    uint16_t height() const override { return _driver.height(); }
    uint8_t colorDepth() const override { return _driver.colorDepth(); }
    void setRotation(uint8_t r) override { _driver.setRotation(r); }
    uint8_t rotation() const override { return _driver.rotation(); }
    void invertDisplay(bool invert) override { _driver.invertDisplay(invert); }
    void setAddrWindow(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye) override
        { _driver.setAddrWindow(xs, ys, xe, ye); }
    void writePixel(uint16_t color) override { _driver.writePixel(color); }
    void writePixels(const uint16_t* data, size_t len) override { _driver.writePixels(data, len); }
    void writeFill(uint16_t color, size_t len) override { _driver.writeFill(color, len); }
    uint16_t readPixel(uint16_t x, uint16_t y) override { return _driver.readPixel(x, y); }
    void sleep() override;
    void wake() override;
    void setBacklight(uint8_t brightness) override;
    uint8_t backlight() const override { return _brightness; }
    IDriver& driver() override { return _driver; }
    DisplayCapabilities capabilities() const override {
        DisplayCapabilities caps;
        caps.technology = DisplayTechnology::OLED;
        caps.nativeFormat = colorDepth() == 16 ? PixelFormat::RGB565 : PixelFormat::Mono1;
        caps.readback = _driver.supportsReadback();
        caps.deepSleep = _driver.supportsDeepSleep();
        return caps;
    }

protected:
    IDriver& _driver;
    IBus&    _bus;
    uint8_t  _brightness;
    bool     _initialized;
    GfxResult _lastResult;
};

#endif // SEEED_GFX_PANEL_OLED_H
