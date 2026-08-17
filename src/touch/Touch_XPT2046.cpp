/**
 * @file   Touch_XPT2046.cpp
 * @brief  XPT2046 resistive touch driver implementation
 *
 * The XPT2046 is a 4-wire resistive touch controller using SPI.
 * It provides 12-bit ADC readings for X, Y, and Z (pressure).
 *
 * Key features:
 *   - Multi-sample reading for noise reduction
 *   - Pressure-based touch validation
 *   - Calibration matrix for mapping raw ADC to screen coordinates
 *   - Rotation support
 *
 * Adapted from Seeed_GFX-master Extensions/Touch.cpp
 * Original code by maxpautsch and Bodmer.
 */

#include "Touch_XPT2046.h"

// Constructor

Touch_XPT2046::Touch_XPT2046(int8_t csPin, SPIClass& spi,
                             uint16_t width, uint16_t height)
    : _csPin(csPin)
    , _spi(spi)
    , _width(width)
    , _height(height)
    , _rotation(0)
    , _zThreshold(XPT2046_Z_THRESHOLD)
    , _spiFreq(XPT2046_SPI_FREQ)
    , _pressTime(0)
    , _calibrated(false)
    , _calX0(300), _calX1(3600)
    , _calY0(300), _calY1(3600)
    , _calRotate(true), _calInvertX(true), _calInvertY(false)
{
}

// begin

bool Touch_XPT2046::begin(IBus& bus) {
    (void)bus; // XPT2046 uses its own SPI bus

    if (_csPin < 0 || _width == 0 || _height == 0) return false;

    // Configure chip select pin
    if (_csPin >= 0) {
        pinMode(_csPin, OUTPUT);
        digitalWrite(_csPin, HIGH);
    }

    // Initialize SPI
    _spi.begin();

    return true;
}

// Low-level SPI helpers

void Touch_XPT2046::beginTouch() {
    _spi.beginTransaction(SPISettings(_spiFreq, MSBFIRST, SPI_MODE0));
    digitalWrite(_csPin, LOW);
}

void Touch_XPT2046::endTouch() {
    digitalWrite(_csPin, HIGH);
    _spi.endTransaction();
}

// readADC

uint16_t Touch_XPT2046::readADC(uint8_t cmd) {
    uint16_t val;

    _spi.transfer(cmd);
    val  = (uint16_t)_spi.transfer(0) << 5;
    val |= (uint16_t)_spi.transfer(0) >> 3;

    return val & 0x0FFF; // 12-bit value
}

// readRaw

bool Touch_XPT2046::readRaw(uint16_t* x, uint16_t* y) {
    if (!x || !y) return false;
    uint16_t tmp;

    beginTouch();

    // Read X: sample 4 times and keep the last
    // Each read cycle: command byte, then 2 bytes of data
    _spi.transfer(XPT2046_CMD_READ_Y);  // Start YP conversion
    _spi.transfer16(0);                 // Read/discard first sample
    _spi.transfer(XPT2046_CMD_READ_Y);  // Start YP, read last 8 bits
    _spi.transfer16(0);                 // Read/discard second sample
    _spi.transfer(XPT2046_CMD_READ_Y);  // Start YP, read last 8 bits
    _spi.transfer16(0);                 // Read/discard third sample
    _spi.transfer(XPT2046_CMD_READ_Y);  // Start YP, read last 8 bits

    tmp  = (uint16_t)_spi.transfer(0) << 5;
    tmp |= (uint16_t)_spi.transfer(XPT2046_CMD_READ_X) >> 3;  // Read last 8 bits, start XP

    *x = tmp & 0x0FFF;

    // Read Y: sample 4 times and keep the last
    _spi.transfer(0);                   // Read first 8 bits
    _spi.transfer(XPT2046_CMD_READ_X);  // Read last 8 bits, start XP
    _spi.transfer(0);                   // Read first 8 bits
    _spi.transfer(XPT2046_CMD_READ_X);  // Read last 8 bits, start XP
    _spi.transfer(0);                   // Read first 8 bits
    _spi.transfer(XPT2046_CMD_READ_X);  // Read last 8 bits, start XP

    tmp  = (uint16_t)_spi.transfer(0) << 5;
    tmp |= (uint16_t)_spi.transfer(0) >> 3;  // Read last 8 bits

    *y = tmp & 0x0FFF;

    endTouch();

    return true;
}

// readRawZ

uint16_t Touch_XPT2046::readRawZ() {
    int32_t tz = 0xFFF;

    beginTouch();

    // Z1 conversion
    _spi.transfer(XPT2046_CMD_READ_Z1);
    tz += (int32_t)_spi.transfer16(XPT2046_CMD_READ_Z2) >> 3;  // Read Z1, start Z2
    tz -= (int32_t)_spi.transfer16(0) >> 3;                     // Read Z2

    endTouch();

    if (tz == 4095) tz = 0;

    return (uint16_t)tz;
}

// validTouch

bool Touch_XPT2046::validTouch(uint16_t* x, uint16_t* y, uint16_t threshold) {
    uint16_t x_tmp, y_tmp, x_tmp2, y_tmp2;

    // Wait until pressure stops increasing (debounce)
    uint16_t z1 = 1;
    uint16_t z2 = 0;
    uint8_t settleSamples = 32;
    while (z1 > z2 && settleSamples--) {
        z2 = z1;
        z1 = readRawZ();
        delay(1);
    }

    if (z1 <= threshold) return false;

    readRaw(&x_tmp, &y_tmp);

    delay(1);
    if (readRawZ() <= threshold) return false;

    delay(2);
    readRaw(&x_tmp2, &y_tmp2);

    // Check that successive samples are within the deadband
    if (abs((int16_t)x_tmp - (int16_t)x_tmp2) > XPT2046_RAW_ERR) return false;
    if (abs((int16_t)y_tmp - (int16_t)y_tmp2) > XPT2046_RAW_ERR) return false;

    *x = x_tmp;
    *y = y_tmp;

    return true;
}

// read

bool Touch_XPT2046::read(TouchPoint& point) {
    uint16_t x_tmp, y_tmp;
    uint16_t threshold = _zThreshold;

    if (threshold < 20) threshold = 20;

    // Hold-off: if we recently detected a press, lower the threshold
    // to avoid flickering during continuous touch
    if (static_cast<int32_t>(_pressTime - millis()) > 0) threshold = 20;

    // Try up to 5 times to get a valid touch
    uint8_t n = 5;
    uint8_t valid = 0;
    while (n--) {
        if (validTouch(&x_tmp, &y_tmp, threshold)) valid++;
    }

    if (valid < 1) {
        _pressTime = 0;
        point.pressed = false;
        return false;
    }

    // Set hold-off timer
    _pressTime = millis() + 50;

    // Convert raw to screen coordinates
    convertRawXY(&x_tmp, &y_tmp);

    // Reject off-screen coordinates
    if (x_tmp >= _width || y_tmp >= _height) {
        point.pressed = false;
        return false;
    }

    point.x = x_tmp;
    point.y = y_tmp;
    point.pressed = true;
    point.id = 0;

    return true;
}

// isPressed

bool Touch_XPT2046::isPressed() {
    uint16_t z = readRawZ();
    return (z > _zThreshold);
}

// setRotation

void Touch_XPT2046::setRotation(uint8_t rotation) {
    _rotation = rotation;
}

// setDimensions

void Touch_XPT2046::setDimensions(uint16_t width, uint16_t height) {
    _width = width ? width : 1;
    _height = height ? height : 1;
}

// convertRawXY

void Touch_XPT2046::convertRawXY(uint16_t* x, uint16_t* y) {
    if (!x || !y) return;
    const int32_t rawX = *x;
    const int32_t rawY = *y;
    const int32_t sourceX = _calRotate ? rawY : rawX;
    const int32_t sourceY = _calRotate ? rawX : rawY;
    int32_t xx = (sourceX - _calX0) * (int32_t)(_width - 1U) / _calX1;
    int32_t yy = (sourceY - _calY0) * (int32_t)(_height - 1U) / _calY1;
    if (xx < 0) xx = 0;
    if (yy < 0) yy = 0;
    if (xx >= _width) xx = _width - 1U;
    if (yy >= _height) yy = _height - 1U;
    if (_calInvertX) xx = (_width - 1U) - xx;
    if (_calInvertY) yy = (_height - 1U) - yy;
    *x = static_cast<uint16_t>(xx);
    *y = static_cast<uint16_t>(yy);
}

// setCalibration

void Touch_XPT2046::setCalibration(const uint16_t* data) {
    if (!data) return;
    _calX0 = data[0];
    _calX1 = data[1];
    _calY0 = data[2];
    _calY1 = data[3];

    // Prevent division by zero
    if (_calX1 == 0) _calX1 = 1;
    if (_calY1 == 0) _calY1 = 1;

    _calRotate  = (data[4] & 0x01) != 0;
    _calInvertX = (data[4] & 0x02) != 0;
    _calInvertY = (data[4] & 0x04) != 0;

    _calibrated = true;
}

// getCalibration

void Touch_XPT2046::getCalibration(uint16_t* data) const {
    if (!data) return;
    data[0] = _calX0;
    data[1] = _calX1;
    data[2] = _calY0;
    data[3] = _calY1;
    data[4] = (_calRotate ? 0x01 : 0x00) |
              (_calInvertX ? 0x02 : 0x00) |
              (_calInvertY ? 0x04 : 0x00);
}
