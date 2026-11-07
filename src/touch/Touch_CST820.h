/**
 * @file   Touch_CST820.h
 * @brief  CST820 / CST820B capacitive touch driver for Seeed_GFX v2.0
 *
 * The 2.05" and 1.75" round AMOLED panels (H0205S003AMT003A / H0175Y003AMT003V1)
 * carry a Hynitron CST820 (2.05") or CST820B (1.75") touch IC on the panel's own
 * glass, NOT the CST9217 that the square 2.16" panel uses.  The carrier FPC
 * routes the panel TPIC I2C straight through, so the touch address and register
 * map change with the panel:
 *
 *   2.16" 480x480 square -> CST9217  @0x5A  (command-page protocol, see Touch_CST9217)
 *   2.05" 368x448 round   -> CST820   @0x15
 *   1.75" 466x466 round   -> CST820B  @0x15
 *
 * Register map mirrors the CST8xx family shared layout (verified against the
 * working esp_lcd_touch_cst820 component / Lewis-he SensorLib CST816):
 *   - touch data starts at 0x02, 5 bytes:
 *         [0] = touch count (LOW nibble; 0xFF = invalid frame, 0 = released)
 *         [1] = x[11:8] in bits 3..0 (high nibble reserved)
 *         [2] = x[7:0]
 *         [3] = y[11:8] in bits 3..0 (high nibble reserved)
 *         [4] = y[7:0]
 *         x = ((xh & 0x0F) << 8) | xl ;  y = ((yh & 0x0F) << 8) | yl
 *   - chip/product id (verification only) at 0xA7, expected 0xB7.
 *   - INT is a short pulse, used only as a poll trigger hint (poll-throttled on
 *     a timer, same as Touch_CST9217).  No gesture decode.
 *
 * Native mounting transform: the panel reports coordinates in its own glass
 * frame.  The square panel's CST9217 needed swap_xy + mirror; the round panels
 * default to identity + clamp and expose compile-time swap/mirror toggles so the
 * orientation can be confirmed against hardware in one flash.
 */

#ifndef SEEED_GFX_TOUCH_CST820_H
#define SEEED_GFX_TOUCH_CST820_H

#include <Arduino.h>
#include <Wire.h>
#include "../core/Touch.h"

#define CST820_I2C_ADDR       0x15   /* CST820 / CST820B 7-bit I2C address  */
#define CST820_REG_DATA       0x02   /* touch report: num, x_h, x_l, y_h, y_l */
#define CST820_CHIP_ID_REG    0xA7   /* chip/product id (verification)        */
#define CST820_DATA_LEN       5      /* 1 + 4 bytes (single point)           */
#define CST820_POLL_MS        16     /* idle poll throttle                    */

/* Native orientation toggles — confirm on hardware, then bake in.   */
#ifndef CST820_SWAP_XY
#define CST820_SWAP_XY       0
#endif
#ifndef CST820_MIRROR_X
#define CST820_MIRROR_X      0
#endif
#ifndef CST820_MIRROR_Y
#define CST820_MIRROR_Y      0
#endif

class Touch_CST820 : public ITouch {
public:
    /**
     * @param intPin    Touch INT pulse pin, -1 to poll
     * @param wire      I2C bus (shared Wire on the carrier)
     * @param addr      I2C address (default 0x15)
     * @param width     Display width in pixels
     * @param height    Display height in pixels
     * @param rstPin    Optional reset pin, -1 when reset goes through the
     *                  carrier TP-RST (expander P01, pulsed by the sketch)
     * @param sda/scl   Explicit I2C pins
     * @param frequency I2C clock
     * @param manageWire Call begin() on the TwoWire instance
     */
    Touch_CST820(int8_t intPin = -1, TwoWire& wire = Wire,
                 uint8_t addr = CST820_I2C_ADDR,
                 uint16_t width = 466, uint16_t height = 466,
                 int8_t rstPin = -1,
                 int8_t sda = -1, int8_t scl = -1,
                 uint32_t frequency = 400000, bool manageWire = true);

    const char* name() const override { return "CST820"; }

    bool begin(IBus& bus) override;
    bool read(TouchPoint& point) override;
    bool isPressed() override;
    uint8_t maxPoints() const override { return 1; }
    void setRotation(uint8_t rotation) override;

    uint8_t readMulti(TouchPoint* points, uint8_t maxPts) override;

    /** Read the chip/product id byte (0xA7) for verification logging. */
    bool readChipId(uint8_t& id);

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
    bool     _lastPressed;
    uint32_t _lastPollMs;

    bool readReg(uint8_t reg, uint8_t* buf, size_t len);
    void hwReset();
    void applyRotation(uint16_t& x, uint16_t& y);
};

#endif // SEEED_GFX_TOUCH_CST820_H