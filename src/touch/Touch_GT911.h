/**
 * @file   Touch_GT911.h
 * @brief  GT911 capacitive touch driver for Seeed_GFX v2.0
 *
 * Goodix GT911 (and GT9271-family compatible) I2C capacitive touch controller,
 * 16-bit register addressing and up to 5 simultaneous touch points — the touch
 * chip on the XIAO ESP32-C5 "console for CAN" ST7789 carrier.
 *
 * Register map (Goodix GT911 datasheet / esp_lcd_touch_gt911):
 *   Transfers address the register as TWO bytes big-endian (high then low),
 *   then a repeated start for the data phase.
 *   0x8140 : chip/product ID string, reads "911"
 *   0x814E : status: bit7 = buffer ready, bits3:0 = number of points
 *   0x814F : point records, 8 bytes each:
 *              [0] track id, [1..2] X (low,high), [3..4] Y (low,high),
 *              [5..6] touch size, [7] reserved
 *   After reading, the buffer is cleared by writing 0x00 to 0x814E.
 *
 * Reset / address selection: the controller samples INT at the RESET release
 * edge to pick its I2C address (INT low -> 0x5D primary, INT high -> 0x14
 * backup).  On this carrier the reset line lives behind a PCA9535 expander and
 * INT is a GPIO owned by the MCU, so pass rstPin = -1 and let the board pulse
 * it (Board_XIAO_Console_CAN::resetTouch()) right before begin()/attachTouch().
 */

#ifndef SEEED_GFX_TOUCH_GT911_H
#define SEEED_GFX_TOUCH_GT911_H

#include <Arduino.h>
#include <Wire.h>
#include "../core/Touch.h"

#define GT911_I2C_ADDR          0x5D   /* primary   (INT low  at reset) */
#define GT911_I2C_ADDR_BACKUP   0x14   /* backup    (INT high at reset) */
#define GT911_REG_PRODUCT_ID    0x8140 /* reads "911"                  */
#define GT911_REG_STATUS        0x814E /* buffer status                */
#define GT911_REG_POINT1        0x814F /* first point record           */
#define GT911_MAX_POINTS        5

/* Native mounting transform toggles — confirm on hardware, then bake in. */
#ifndef GT911_SWAP_XY
#define GT911_SWAP_XY       0
#endif
#ifndef GT911_MIRROR_X
#define GT911_MIRROR_X      0
#endif
#ifndef GT911_MIRROR_Y
#define GT911_MIRROR_Y      0
#endif

class Touch_GT911 : public ITouch {
public:
    /**
     * @param intPin    Touch INT pin, -1 to disable (reports are always polled)
     * @param wire      I2C bus (shared Wire on the carrier)
     * @param addr      I2C address (default 0x5D primary)
     * @param width     Display width in pixels
     * @param height    Display height in pixels
     * @param rstPin    Reset pin, -1 when reset lives behind an IO expander
     * @param sda/scl   Explicit I2C pins
     * @param frequency I2C clock
     * @param manageWire Call begin() on the supplied TwoWire instance
     */
    Touch_GT911(int8_t intPin = -1, TwoWire& wire = Wire,
                uint8_t addr = GT911_I2C_ADDR,
                uint16_t width = 240, uint16_t height = 320,
                int8_t rstPin = -1,
                int8_t sda = -1, int8_t scl = -1,
                uint32_t frequency = 400000, bool manageWire = true);

    const char* name() const override { return "GT911"; }

    bool begin(IBus& bus) override;
    bool read(TouchPoint& point) override;
    bool isPressed() override;
    uint8_t maxPoints() const override { return GT911_MAX_POINTS; }
    void setRotation(uint8_t rotation) override;

    uint8_t readMulti(TouchPoint* points, uint8_t maxPts) override;

    /** Read the 3-byte chip ID at 0x8140 (verification logging). */
    bool readProductId(char id[3]);

    void setDimensions(uint16_t width, uint16_t height)
        { _width = width; _height = height; }

private:
    int8_t   _intPin;
    int8_t   _rstPin;
    TwoWire& _wire;
    uint8_t  _addr;
    uint8_t  _rotation;
    uint16_t _width;
    uint16_t _height;
    int8_t   _sda;
    int8_t   _scl;
    uint32_t _frequency;
    bool     _manageWire;
    bool     _initialized;

    bool readReg(uint16_t reg, uint8_t* buf, size_t len);
    bool writeReg(uint16_t reg, const uint8_t* data, size_t len);
    void hwReset();
    uint8_t readPoints(TouchPoint* points, uint8_t maxPts);
    void applyRotation(uint16_t& x, uint16_t& y);
};

#endif // SEEED_GFX_TOUCH_GT911_H