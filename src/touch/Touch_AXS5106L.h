/**
 * @file   Touch_AXS5106L.h
 * @brief  AXS5106L capacitive touch driver for Seeed_GFX v2.0
 *
 * I2C capacitive touch controller (address 0x63) used on the Seeed XIAO
 * Display Board 1.47" (the touch variant, 172x320). Reports up to 2 points in a
 * 14-byte frame read from register 0x01. RST is shared with the LCD RST line;
 * INT is a separate pin (D7 on the XIAO Display Board).
 *
 * The reference driver (XIAO-Display-Board-main axs5106l_device.{h,cpp}) returns
 * the controller's raw coordinates, which are already in panel range on the
 * 1.47" board (the nRF dashboard uses them directly as display coords), so this
 * driver clamps to width/height rather than rescaling. VERIFY on hardware that
 * your panel reports in panel range; if not, scale in applyRotation.
 *
 * LCD and touch share RST. After display.begin() has reset and initialized the
 * LCD, construct this driver with rstPin=-1 so touch.begin() does not reset the
 * initialized LCD again.
 */

#ifndef SEEED_GFX_TOUCH_AXS5106L_H
#define SEEED_GFX_TOUCH_AXS5106L_H

#include <Arduino.h>
#include <Wire.h>
#include "../core/Touch.h"

#define AXS5106L_I2C_ADDR        0x63
#define AXS5106L_ID_REG          0x08
#define AXS5106L_TOUCH_DATA_REG  0x01
#define AXS5106L_MAX_POINTS      2
#define AXS5106L_REPORT_LEN      14   // status + count + up to 2 points (6 bytes each)

class Touch_AXS5106L : public ITouch {
public:
    /** @param rstPin  Reset pin, or -1 when display.begin() already reset the
     *                  shared LCD/touch reset line
     *  @param intPin  Interrupt pin (IRQ), -1 for polling
     *  @param wire    I2C bus reference (default Wire)
     *  @param width   Display width in pixels (for rotation + clamping)
     *  @param height  Display height in pixels
     */
    Touch_AXS5106L(int8_t rstPin = -1, int8_t intPin = -1, TwoWire& wire = Wire,
                   uint16_t width = 172, uint16_t height = 320);

    // ITouch
    const char* name() const override { return "AXS5106L"; }
    bool begin(IBus& bus) override;
    bool read(TouchPoint& point) override;
    bool isPressed() override;
    uint8_t maxPoints() const override { return AXS5106L_MAX_POINTS; }
    void setRotation(uint8_t rotation) override;

    // Extended API (mirrors Touch_CHSCX6X).
    uint8_t readMulti(TouchPoint* points, uint8_t maxPts) override;
    void setDimensions(uint16_t width, uint16_t height) {
        _width = width; _height = height;
    }
    bool isConnected();

private:
    int8_t   _rstPin;
    int8_t   _intPin;
    TwoWire& _wire;
    uint8_t  _rotation;
    uint16_t _width;
    uint16_t _height;
    bool     _lastPressed;
    uint32_t _lastPollMs;

    bool readReport(uint8_t* buf, uint8_t len);
    void applyRotation(uint16_t& x, uint16_t& y) const;
};

#endif // SEEED_GFX_TOUCH_AXS5106L_H
