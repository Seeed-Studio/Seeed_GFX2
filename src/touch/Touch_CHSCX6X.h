/**
 * @file   Touch_CHSCX6X.h
 * @brief  CHSCX6X capacitive touch driver for Seeed_GFX v2.0
 *
 * I2C capacitive touch controller (address 0x2E) used on Seeed
 * XIAO Round Display. The official 5-byte report exposes one touch point.
 *
 * Adapted from Seeed_GFX-master Touch_Drivers/CHSCX6X.h and
 * CHSCX6X_Defines.h
 */

#ifndef SEEED_GFX_TOUCH_CHSCX6X_H
#define SEEED_GFX_TOUCH_CHSCX6X_H

#include <Arduino.h>
#include <Wire.h>
#include "../core/Touch.h"

// CHSCX6X Constants

#define CHSC6X_I2C_ADDR         0x2E
#define CHSC6X_MAX_POINTS_NUM   1
#define CHSC6X_READ_POINT_LEN   5
#define CHSC6X_POINT1_X         2
#define CHSC6X_POINT1_Y         4

// Retained for source compatibility. The documented Seeed 5-byte report does
// not expose gesture codes, so gesture() returns CHSC6X_GESTURE_NONE.
#define CHSC6X_GESTURE_NONE      0x00
#define CHSC6X_GESTURE_SWIPE_UP  0x01
#define CHSC6X_GESTURE_SWIPE_DOWN 0x02
#define CHSC6X_GESTURE_SWIPE_LEFT 0x03
#define CHSC6X_GESTURE_SWIPE_RIGHT 0x04
#define CHSC6X_GESTURE_SINGLE_CLICK 0x05
#define CHSC6X_GESTURE_DOUBLE_CLICK 0x0B
#define CHSC6X_GESTURE_LONG_PRESS 0x0C

// Touch_CHSCX6X class

class Touch_CHSCX6X : public ITouch {
public:
    /**
     * @param intPin  Interrupt pin (IRQ) from touch controller, -1 if unused
     * @param wire    I2C bus reference (default Wire)
     * @param width   Display width in pixels
     * @param height  Display height in pixels
     */
    Touch_CHSCX6X(int8_t intPin = -1, TwoWire& wire = Wire,
                  uint16_t width = 240, uint16_t height = 240);

    // ITouch interface

    const char* name() const override { return "CHSCX6X"; }

    /** Initialize the touch controller over I2C
     *  @param bus  Display bus (unused; CHSCX6X uses its own I2C)
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

    uint8_t maxPoints() const override { return CHSC6X_MAX_POINTS_NUM; }

    /** Set the display rotation (0-3) */
    void setRotation(uint8_t rotation) override;

    // Extended API

    /** Read up to maxPoints touch points
     *  @param points  Array to fill with touch data
     *  @param maxPts  Maximum number of points to read
     *  @return number of touch points detected
     */
    uint8_t readMulti(TouchPoint* points, uint8_t maxPts) override;

    /** Get the last detected gesture code (always NONE for this protocol) */
    uint8_t gesture() const { return _gesture; }

    /** Set the display dimensions (used for rotation clamping) */
    void setDimensions(uint16_t width, uint16_t height);

    /** Check if the controller is connected on the I2C bus */
    bool isConnected();

    /** Reinitialize the I2C-facing driver state (there is no documented
     *  CHSC6X software-reset register in the Seeed reference driver).
     */
    void reset();

private:
    int8_t   _intPin;
    TwoWire& _wire;
    uint8_t  _rotation;
    uint16_t _width;
    uint16_t _height;
    uint8_t  _gesture;
    bool     _lastPressed;
    uint32_t _lastPollMs;

    /** Read raw coordinate bytes from the controller
     *  @param buf  Buffer of at least CHSC6X_READ_POINT_LEN bytes
     *  @return true if enough data was read
     */
    bool readRawPacket(uint8_t* buf);
};

#endif // SEEED_GFX_TOUCH_CHSCX6X_H
