/**
 * @file   Touch.h
 * @brief  ITouch interface and TouchPoint struct for Seeed_GFX v2.0
 *
 * The Touch layer abstracts touch controller drivers.
 * Supports both resistive (SPI, e.g. XPT2046) and capacitive
 * (I2C, e.g. CHSCX6X, FT6x36) touch controllers.
 */

#ifndef SEEED_GFX_TOUCH_H
#define SEEED_GFX_TOUCH_H

#include <stdint.h>
#include <stddef.h>

// Forward declaration
class IBus;

// TouchPoint struct

/** Represents a single touch point */
struct TouchPoint {
    uint16_t x = 0;       /**< X coordinate */
    uint16_t y = 0;       /**< Y coordinate */
    bool     pressed = false; /**< True if the point is being pressed */
    uint8_t  id = 0;      /**< Touch point ID (for multi-touch) */
    uint16_t strength = 0; /**< Controller-reported touch strength, 0 if unavailable */
};

// ITouch interface

class ITouch {
public:
    virtual ~ITouch() = default;

    // Initialization

    /** Initialize the touch controller
     *  @param bus  The bus to use for communication
     *  @return true on success
     */
    virtual bool begin(IBus& bus) = 0;

    // Reading

    /** Read a touch point
     *  @param point  Output: the touch point data
     *  @return true if a touch was detected
     */
    virtual bool read(TouchPoint& point) = 0;

    /** Read up to maxPoints simultaneous touch points.
     *  Controllers without a native multi-touch implementation fall back to
     *  the first point returned by read().
     */
    virtual uint8_t readMulti(TouchPoint* points, uint8_t maxPoints) {
        if (!points || maxPoints == 0) return 0;
        return read(points[0]) ? 1U : 0U;
    }

    /** Check if the screen is currently being pressed */
    virtual bool isPressed() = 0;

    /** Last controller-reported gesture code, or 0 when unavailable. */
    virtual uint8_t gesture() const { return 0; }

    /** Whether this controller exposes uncalibrated ADC coordinates. */
    virtual bool supportsRawRead() const { return false; }

    /** Read uncalibrated controller coordinates when supported. */
    virtual bool readRaw(uint16_t* x, uint16_t* y) {
        (void)x; (void)y;
        return false;
    }

    /** Read raw pressure. Capacitive controllers normally return 0/1. */
    virtual uint16_t rawPressure() { return isPressed() ? 1U : 0U; }

    /** Whether five-word TFT_eSPI-compatible calibration is supported. */
    virtual bool supportsCalibration() const { return false; }

    /** Convert a raw coordinate in place using the current calibration. */
    virtual void convertRawXY(uint16_t* x, uint16_t* y) {
        (void)x; (void)y;
    }

    /** Set/get [x0, xRange, y0, yRange, flags] calibration data. */
    virtual void setCalibration(const uint16_t* data) { (void)data; }
    virtual void getCalibration(uint16_t* data) const { (void)data; }

    // Capabilities

    /** Maximum number of simultaneous touch points supported */
    virtual uint8_t maxPoints() const { return 1; }

    // Configuration

    /** Set an absolute touch-controller rotation (0-3). */
    virtual void setRotation(uint8_t rotation) = 0;

    /**
     * Synchronize touch coordinates with a display rotation.
     *
     * Most touch panels use the same orientation as their display, so the
     * default implementation is identical to setRotation(). Product-specific
     * drivers may add a fixed mounting offset here without changing the
     * semantics of the explicit setRotation() API.
     */
    virtual void setDisplayRotation(uint8_t displayRotation) {
        setRotation(displayRotation);
    }

    // Information

    /** Human-readable touch controller name */
    virtual const char* name() const = 0;
};

#endif // SEEED_GFX_TOUCH_H
