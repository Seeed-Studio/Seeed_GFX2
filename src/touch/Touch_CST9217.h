/**
 * @file   Touch_CST9217.h
 * @brief  CST9217 capacitive touch driver for Seeed_GFX v2.0
 *
 * Capacitive touch controller made by Hynitron, used on the CO5300 480x480
 * AMOLED panels (e.g. the ESP32-S3-Touch-AMOLED-2.16 carrier).  All register
 * accesses are 16/32-bit "command pages" addressed as an MSB-first 2-byte
 * pointer (0xD000 data, 0xD101 command mode, 0xD1F8 resolution, 0xD1FC
 * checkcode, 0xD204 project/chip ID).
 *
 * Register/timing flow mirrors the official Espressif/Waveshare
 * esp_lcd_touch_cst9217 driver (Apache-2.0) that the board BSP uses:
 *   - I2C 7-bit address 0x5A (ESP_LCD_TOUCH_IO_I2C_CST9217_ADDRESS);
 *   - every register access = write the 2-byte pointer, settle 2 ms, then
 *     read the payload (or continue writing it);
 *   - up to 5 retries with 3 ms back-off, then a hardware reset pulse
 *     (10 ms low / 100 ms high) when a reset pin is wired;
 *   - touch read = 10-byte block from 0xD000; data[6] must equal the ACK
 *     0xAB; point count in data[5] bit7/6:0; point record: status in
 *     low nibble of byte0 (0x06 = hold), x = (b1<<4)|(b3>>4),
 *     y = (b2<<4)|(b3&0x0F);
 *   - configuration is read after entering command mode (write 0xD101 with
 *     payload {0xD1,0x01}, settle 10 ms): checkcode <- 0xD1FC,
 *     resolution <- 0xD1F8, project/chip ID <- 0xD204.
 *
 * The datasheet register layout in the Arduino reference driver writes a
 * follow-up ACK byte after reading; the official esp_lcd_touch_cst9217 does
 * NOT - it validates data[6] instead.  This driver follows the official one.
 */

#ifndef SEEED_GFX_TOUCH_CST9217_H
#define SEEED_GFX_TOUCH_CST9217_H

#include <Arduino.h>
#include <Wire.h>
#include "../core/Touch.h"

// CST9217 constants (from esp_lcd_touch_cst9217.h)

/** Default I2C 7-bit address */
#define CST9217_I2C_ADDR        0x5A

/** Command pages ("registers") */
#define CST9217_REG_DATA        0xD000   /* touch report: 1 point * 5 + 5 bytes */
#define CST9217_REG_CMD_MODE    0xD101   /* enter command mode: write {0xD1,0x01} */
#define CST9217_REG_CHECKCODE   0xD1FC
#define CST9217_REG_RESOLUTION  0xD1F8
#define CST9217_REG_PROJECT_ID  0xD204

/** Report framing */
#define CST9217_ACK_VALUE       0xAB     /* expected at report byte 6 */
#define CST9217_READ_LEN        10       /* 1 * 5 + 5 */
#define CST9217_MAX_TOUCH_POINTS 1       /* official driver caps at 1 */
#define CST9217_EVENT_HOLD      0x06     /* point event: finger down */

/** Idle poll interval (ms).  The CST92xx INT is a pulse, not a level hold,
 *  so waits are throttled on a timer instead of on the IRQ line. */
#define CST9217_POLL_MS         16

// Touch_CST9217 class

class Touch_CST9217 : public ITouch {
public:
    /**
     * @param intPin    Interrupt pin from touch controller, -1 to poll
     *                  (used only as a read-hint: the part emits a pulse)
     * @param wire      I2C bus reference (default Wire)
     * @param addr      I2C address (default 0x5A)
     * @param width     Display width in pixels
     * @param height    Display height in pixels
     * @param rstPin    Optional reset pin; -1 when the LCD line is shared and
     *                  display.begin() already pulsed reset (this panel:
     *                  RST=GPIO40 shared with the LCD)
     * @param sda       Optional explicit I2C SDA pin
     * @param scl       Optional explicit I2C SCL pin
     * @param frequency I2C clock when explicit pins are used
     * @param manageWire Call begin() on the supplied TwoWire instance
     */
    Touch_CST9217(int8_t intPin = -1, TwoWire& wire = Wire,
                  uint8_t addr = CST9217_I2C_ADDR,
                  uint16_t width = 480, uint16_t height = 480,
                  int8_t rstPin = -1,
                  int8_t sda = -1, int8_t scl = -1,
                  uint32_t frequency = 400000, bool manageWire = true);

    // ITouch interface

    const char* name() const override { return "CST9217"; }

    /** Initialize the touch controller over I2C.
     *  @param bus  Display bus (unused; CST9217 uses its own I2C)
     *  @return true when configured (command mode + config reads succeeded)
     */
    bool begin(IBus& bus) override;

    /** Read a single touch point (official 0xD000 report flow). */
    bool read(TouchPoint& point) override;

    /** Check if the screen is currently being pressed. */
    bool isPressed() override;

    uint8_t maxPoints() const override { return CST9217_MAX_TOUCH_POINTS; }

    /** Apply a display rotation (0-3) to the reported coordinates. */
    void setRotation(uint8_t rotation) override;

    /** No gesture decode in the official driver. */
    uint8_t gesture() const override { return 0; }

    // Extended API

    /** Read up to maxPoints touch points (single point on this part). */
    uint8_t readMulti(TouchPoint* points, uint8_t maxPts) override;

    /** Set the display dimensions (used for rotation clamping). */
    void setDimensions(uint16_t width, uint16_t height);

    /** Check the controller responds on the I2C bus (performs a report read). */
    bool isConnected();

    /** Resolution read from the chip at begin() (0xD1F8). */
    uint16_t resolutionX() const { return _resX; }
    uint16_t resolutionY() const { return _resY; }

    /** Chip type (0x9217) / project ID read at begin() from 0xD204. */
    uint16_t chipType() const { return _chipType; }
    uint16_t projectId() const { return _projectId; }

    /** Set the I2C address. */
    void setAddress(uint8_t addr) { _addr = addr; }

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
    uint16_t _resX;
    uint16_t _resY;
    uint16_t _chipType;
    uint16_t _projectId;

    /** Official 5-try command-page read (3 ms back-off, hw-reset fallback). */
    bool readReg(uint16_t reg, uint8_t* buf, size_t len);

    /** Official 5-try command-page write (pointer, 2 ms, then payload). */
    bool writeReg(uint16_t reg, const uint8_t* data, size_t len);

    /** Write the 2-byte MSB-first register pointer, then settle 2 ms. */
    bool writePointer(uint16_t reg);

    /** Optional hardware reset pulse: low 10 ms, high 50 ms. */
    void hwReset();

    /** Enter command mode and read resolution / checkcode / IDs. */
    bool readConfig();

    /** Apply rotation + clamp to display dimensions. */
    void applyRotation(uint16_t& x, uint16_t& y);
};

#endif // SEEED_GFX_TOUCH_CST9217_H