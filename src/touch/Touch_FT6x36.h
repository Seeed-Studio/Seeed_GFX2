/**
 * @file   Touch_FT6x36.h
 * @brief  FT6x36 capacitive touch driver for Seeed_GFX v2.0
 *
 * I2C capacitive touch controller (common addresses: 0x38, 0x48, 0x2A).
 * Supports up to 2 simultaneous touch points with gesture detection.
 *
 * This driver implements the ITouch interface with full I2C
 * communication, coordinate reading, and rotation support.
 */

#ifndef SEEED_GFX_TOUCH_FT6X36_H
#define SEEED_GFX_TOUCH_FT6X36_H

#include <Arduino.h>
#include <Wire.h>
#include "../core/Touch.h"

// FT6x36 Constants

/** Default I2C addresses for FT6x36 */
#define FT6X36_ADDR1            0x38
#define FT6X36_ADDR2            0x2A
#define FT6X36_ADDR_FT6336U     0x48

/** Number of touch points supported */
#define FT6X36_MAX_POINTS       2

/** Touch data register */
#define FT6X36_REG_TD_STATUS    0x02
#define FT6X36_REG_P1_XH        0x03
#define FT6X36_REG_P1_XL        0x04
#define FT6X36_REG_P1_YH        0x05
#define FT6X36_REG_P1_YL        0x06
#define FT6X36_REG_P2_XH        0x09
#define FT6X36_REG_P2_XL        0x0A
#define FT6X36_REG_P2_YH        0x0B
#define FT6X36_REG_P2_YL        0x0C

/** Gesture ID register */
#define FT6X36_REG_GEST_ID      0x01

/** Chip vendor/version registers */
#define FT6X36_REG_CHIP_ID      0xA3
#define FT6X36_REG_FIRMWARE_ID  0xA6

/** Gesture codes */
#define FT6X36_GESTURE_NONE         0x00
#define FT6X36_GESTURE_SWIPE_UP     0x10
#define FT6X36_GESTURE_SWIPE_LEFT   0x14
#define FT6X36_GESTURE_SWIPE_DOWN   0x18
#define FT6X36_GESTURE_SWIPE_RIGHT  0x1C
#define FT6X36_GESTURE_ZOOM_IN      0x48
#define FT6X36_GESTURE_ZOOM_OUT     0x49

/** Gesture + status + both point records through register 0x11. */
#define FT6X36_READ_LEN         17

// Touch_FT6x36 class

class Touch_FT6x36 : public ITouch {
public:
    /**
     * @param intPin  Interrupt pin from touch controller, -1 if unused
     * @param wire    I2C bus reference (default Wire)
     * @param addr    I2C address (default 0x38)
     * @param width   Display width in pixels
     * @param height  Display height in pixels
     * @param sda     Optional explicit I2C SDA pin
     * @param scl     Optional explicit I2C SCL pin
     * @param frequency I2C clock when explicit pins are used
     * @param manageWire Call begin() on the supplied TwoWire instance
     * @param indicatorTuning Apply Seeed Indicator FT5x06 thresholds
     * @param mountingRotation Fixed touch-to-display mounting offset (0-3)
     */
    Touch_FT6x36(int8_t intPin = -1, TwoWire& wire = Wire,
                 uint8_t addr = FT6X36_ADDR1,
                 uint16_t width = 320, uint16_t height = 480,
                 int8_t sda = -1, int8_t scl = -1,
                 uint32_t frequency = 400000, bool manageWire = true,
                 bool indicatorTuning = false,
                 uint8_t mountingRotation = 0);

    // ITouch interface

    const char* name() const override { return "FT6x36"; }

    /** Initialize the touch controller over I2C
     *  @param bus  Display bus (unused; FT6x36 uses its own I2C)
     *  @return true on success
     */
    bool begin(IBus& bus) override;

    /** Read a single touch point
     *  @param point  Output: touch point data
     *  @return true if a touch was detected
     */
    bool read(TouchPoint& point) override;

    /** Check if the screen is currently being pressed */
    bool isPressed() override;

    uint8_t maxPoints() const override { return FT6X36_MAX_POINTS; }

    /** Set an absolute touch-controller rotation (0-3). */
    void setRotation(uint8_t rotation) override;

    /** Apply the display rotation plus this panel's mounting offset. */
    void setDisplayRotation(uint8_t displayRotation) override;

    // Extended API

    /** Read up to maxPoints touch points
     *  @param points  Array to fill with touch data
     *  @param maxPts  Maximum number of points to read
     *  @return number of touch points detected
     */
    uint8_t readMulti(TouchPoint* points, uint8_t maxPts) override;

    /** Get the last detected gesture code */
    uint8_t gesture() const override { return _gesture; }

    /** Set the display dimensions */
    void setDimensions(uint16_t width, uint16_t height);

    /** Check if the controller is connected on the I2C bus
     *  Tries both common addresses (0x38 and 0x2A)
     *  @return true if the controller responds
     */
    bool isConnected();

    /** Get the chip vendor ID */
    uint8_t readChipID();

    /** Get the firmware version */
    uint8_t readFirmwareID();

    /** Set the I2C address */
    void setAddress(uint8_t addr) { _addr = addr; }

private:
    int8_t   _intPin;
    TwoWire& _wire;
    uint8_t  _addr;
    uint8_t  _rotation;
    uint16_t _width;
    uint16_t _height;
    uint8_t  _gesture;
    bool     _initialized;
    int8_t   _sda;
    int8_t   _scl;
    uint32_t _frequency;
    bool     _manageWire;
    bool     _indicatorTuning;
    uint8_t  _mountingRotation;

    /** Read a register from the FT6x36
     *  @param reg  Register address
     *  @return register value, or 0xFF on error
     */
    uint8_t readRegister(uint8_t reg);

    /** Read multiple registers into a buffer
     *  @param reg   Starting register address
     *  @param buf   Output buffer
     *  @param len   Number of bytes to read
     *  @return true on success
     */
    bool readRegisters(uint8_t reg, uint8_t* buf, size_t len);
    bool writeRegister(uint8_t reg, uint8_t value);
};

#endif // SEEED_GFX_TOUCH_FT6X36_H
