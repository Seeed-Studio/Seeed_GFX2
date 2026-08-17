/**
 * @file   Touch_XPT2046.h
 * @brief  XPT2046 resistive touch driver for Seeed_GFX v2.0
 *
 * SPI resistive touch controller. Provides raw coordinate reading,
 * pressure (Z) detection, calibration support, and rotation.
 *
 * Adapted from Seeed_GFX-master Extensions/Touch.h and
 * Extensions/Touch.cpp (Bodmer / maxpautsch).
 */

#ifndef SEEED_GFX_TOUCH_XPT2046_H
#define SEEED_GFX_TOUCH_XPT2046_H

#include <Arduino.h>
#include <SPI.h>
#include "../core/Touch.h"

// XPT2046 Constants

/** Default touch pressure threshold */
#ifndef XPT2046_Z_THRESHOLD
  #define XPT2046_Z_THRESHOLD  350
#endif

/** Default SPI clock frequency for touch (Hz) */
#ifndef XPT2046_SPI_FREQ
  #define XPT2046_SPI_FREQ     2000000UL
#endif

/** Deadband error in successive position samples for validation */
#define XPT2046_RAW_ERR         20

/** XPT2046 commands */
#define XPT2046_CMD_READ_X      0x90  // Read X position (XP), 12-bit, differential
#define XPT2046_CMD_READ_Y      0xD0  // Read Y position (YP), 12-bit, differential
#define XPT2046_CMD_READ_Z1     0xB0  // Read Z1 pressure
#define XPT2046_CMD_READ_Z2     0xC0  // Read Z2 pressure

// Touch_XPT2046 class

class Touch_XPT2046 : public ITouch {
public:
    /**
     * @param csPin      Chip select pin for the touch controller
     * @param spi        SPI bus reference (default SPI)
     * @param width      Display width in pixels
     * @param height     Display height in pixels
     */
    Touch_XPT2046(int8_t csPin, SPIClass& spi = SPI,
                  uint16_t width = 320, uint16_t height = 240);

    // ITouch interface

    const char* name() const override { return "XPT2046"; }

    /** Initialize the touch controller over SPI
     *  @param bus  Display bus (unused; XPT2046 uses its own SPI)
     *  @return true on success
     */
    bool begin(IBus& bus) override;

    /** Read a calibrated touch point
     *  @param point  Output: touch point data
     *  @return true if a touch was detected
     */
    bool read(TouchPoint& point) override;

    /** Check if the screen is currently being pressed */
    bool isPressed() override;

    /** Set the display rotation (0-3) */
    void setRotation(uint8_t rotation) override;

    // Raw reading

    /** Read raw touch position (ADC values, 0-4095)
     *  @param x  Output: raw X ADC value
     *  @param y  Output: raw Y ADC value
     *  @return true (always succeeds)
     */
    bool supportsRawRead() const override { return true; }
    bool readRaw(uint16_t* x, uint16_t* y) override;

    /** Read raw pressure (Z) ADC value
     *  @return raw Z value (0-4095, 0 = not pressed)
     */
    uint16_t readRawZ();
    uint16_t rawPressure() override { return readRawZ(); }

    /** Validate a touch by double-sampling and checking pressure
     *  @param x         Output: validated raw X
     *  @param y         Output: validated raw Y
     *  @param threshold Z pressure threshold
     *  @return true if the touch is valid
     */
    bool validTouch(uint16_t* x, uint16_t* y, uint16_t threshold = XPT2046_Z_THRESHOLD);

    // Calibration

    /** Set calibration parameters
     *  @param data  Array of 5 uint16_t values:
     *               [0] cal_x0  - raw X minimum
     *               [1] cal_x1  - raw X range (max - min)
     *               [2] cal_y0  - raw Y minimum
     *               [3] cal_y1  - raw Y range (max - min)
     *               [4] flags   - bit 0: rotate, bit 1: invert_x, bit 2: invert_y
     */
    bool supportsCalibration() const override { return true; }
    void setCalibration(const uint16_t* data) override;

    /** Get calibration parameters
     *  @param data  Output: array of 5 uint16_t values (same format as setCalibration)
     */
    void getCalibration(uint16_t* data) const override;

    /** Check if calibration has been set */
    bool isCalibrated() const { return _calibrated; }

    /** Convert raw coordinates to calibrated screen coordinates
     *  @param x  Input: raw X, Output: screen X
     *  @param y  Input: raw Y, Output: screen Y
     */
    void convertRawXY(uint16_t* x, uint16_t* y) override;

    // Configuration

    /** Set the Z (pressure) threshold for touch detection */
    void setThreshold(uint16_t threshold) { _zThreshold = threshold; }

    /** Get the current Z threshold */
    uint16_t threshold() const { return _zThreshold; }

    /** Set the display dimensions */
    void setDimensions(uint16_t width, uint16_t height);

    /** Set the SPI clock frequency for touch communication */
    void setSPIFrequency(uint32_t freq) { _spiFreq = freq; }

private:
    int8_t   _csPin;
    SPIClass& _spi;
    uint16_t _width;
    uint16_t _height;
    uint8_t  _rotation;
    uint16_t _zThreshold;
    uint32_t _spiFreq;
    uint32_t _pressTime;       // For debounce hold-off

    // Calibration data
    bool     _calibrated;
    uint16_t _calX0, _calX1;   // Raw X range: min and (max - min)
    uint16_t _calY0, _calY1;   // Raw Y range: min and (max - min)
    uint8_t  _calRotate;       // 0 = normal, 1 = axes swapped
    uint8_t  _calInvertX;      // 0 = normal, 1 = inverted
    uint8_t  _calInvertY;      // 0 = normal, 1 = inverted

    /** Low-level SPI transfer for touch */
    void beginTouch();
    void endTouch();

    /** Read a single 12-bit value from the XPT2046 */
    uint16_t readADC(uint8_t cmd);
};

#endif // SEEED_GFX_TOUCH_XPT2046_H
