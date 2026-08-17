/**
 * @file   Touch_FT6x36.cpp
 * @brief  FT6x36 capacitive touch driver implementation
 *
 * The FT6x36 (and compatible FT5x06 series) is an I2C capacitive
 * touch controller supporting up to 2 simultaneous touch points
 * with gesture detection.
 *
 * Protocol:
 *   Register 0x02: TD_STATUS (number of touch points, bits 3:0)
 *   Register 0x03-0x06: Point 1 coordinates (XH, XL, YH, YL)
 *   Register 0x09-0x0C: Point 2 coordinates (XH, XL, YH, YL)
 *   Register 0x01: Gesture ID
 *
 * The coordinate format is 12-bit:
 *   X = ((XH & 0x0F) << 8) | XL
 *   Y = ((YH & 0x0F) << 8) | YL
 *   Event flag: XH bit 6 (0 = press, 1 = release)
 */

#include "Touch_FT6x36.h"

// Constructor

Touch_FT6x36::Touch_FT6x36(int8_t intPin, TwoWire& wire,
                           uint8_t addr, uint16_t width, uint16_t height,
                           int8_t sda, int8_t scl, uint32_t frequency,
                           bool manageWire, bool indicatorTuning,
                           uint8_t mountingRotation)
    : _intPin(intPin)
    , _wire(wire)
    , _addr(addr)
    , _rotation(0)
    , _width(width)
    , _height(height)
    , _gesture(0)
    , _initialized(false)
    , _sda(sda)
    , _scl(scl)
    , _frequency(frequency)
    , _manageWire(manageWire)
    , _indicatorTuning(indicatorTuning)
    , _mountingRotation(mountingRotation & 3U)
{
}

// begin

bool Touch_FT6x36::begin(IBus& bus) {
    (void)bus; // FT6x36 uses its own I2C bus

    // Configure interrupt pin if provided
    if (_intPin >= 0) {
        pinMode(_intPin, INPUT_PULLUP);
    }

    // Initialize I2C. Integrated products use non-default ESP32-S3 pins.
    if (_manageWire) {
#if defined(ARDUINO_ARCH_ESP32)
        if (_sda >= 0 && _scl >= 0) {
            if (!_wire.begin(_sda, _scl, _frequency)) return false;
        } else {
            if (!_wire.begin()) return false;
            _wire.setClock(_frequency);
        }
#else
        _wire.begin();
        _wire.setClock(_frequency);
#endif
    }

    // Check if the controller is present
    if (!isConnected()) {
        _initialized = false;
        return false;
    }

    if (_indicatorTuning) {
        const struct { uint8_t reg; uint8_t value; } settings[] = {
            {0x80, 70}, {0x81, 60}, {0x82, 16}, {0x83, 60},
            {0x84, 10}, {0x85, 20}, {0x87, 2},  {0x88, 12},
            {0x89, 40},
        };
        for (const auto& setting : settings) {
            if (!writeRegister(setting.reg, setting.value)) return false;
        }
    }

    _initialized = true;
    return true;
}

bool Touch_FT6x36::writeRegister(uint8_t reg, uint8_t value) {
    _wire.beginTransmission(_addr);
    _wire.write(reg);
    _wire.write(value);
    return _wire.endTransmission() == 0;
}

// readRegister

uint8_t Touch_FT6x36::readRegister(uint8_t reg) {
    _wire.beginTransmission(_addr);
    _wire.write(reg);
    if (_wire.endTransmission(false) != 0) {
        return 0xFF;
    }

    if (_wire.requestFrom(_addr, (uint8_t)1) != 1) {
        return 0xFF;
    }

    return _wire.read();
}

// readRegisters

bool Touch_FT6x36::readRegisters(uint8_t reg, uint8_t* buf, size_t len) {
    _wire.beginTransmission(_addr);
    _wire.write(reg);
    if (_wire.endTransmission(false) != 0) {
        return false;
    }

    size_t readLen = _wire.requestFrom(_addr, len);
    if (readLen < len) {
        return false;
    }

    for (size_t i = 0; i < len; i++) {
        buf[i] = _wire.read();
    }

    return true;
}

// read

bool Touch_FT6x36::read(TouchPoint& point) {
    point = TouchPoint();
    if (!_initialized) {
        return false;
    }

    // Check interrupt pin first (active low for most modules)
    if (_intPin >= 0) {
        if (digitalRead(_intPin) != LOW) {
            return false;
        }
    }

    // Start at GEST_ID so the cached gesture and first touch point are updated
    // in one I2C transaction rather than adding a second register read.
    uint8_t buf[6] = {0};
    if (!readRegisters(FT6X36_REG_GEST_ID, buf, sizeof(buf))) {
        _gesture = FT6X36_GESTURE_NONE;
        return false;
    }
    _gesture = buf[0];

    // Number of touch points in bits 3:0 of TD_STATUS
    uint8_t numPoints = buf[1] & 0x0F;

    if (numPoints == 0) {
        return false;
    }

    // buf[0] = GEST_ID, buf[1] = TD_STATUS, buf[2..5] = point 1.
    uint8_t p1_xh = buf[2];
    uint8_t p1_xl = buf[3];
    uint8_t p1_yh = buf[4];
    uint8_t p1_yl = buf[5];

    // Check event flag: bit 6 of XH (0 = press down, 1 = lift up)
    uint8_t event = (p1_xh >> 6) & 0x03;
    if (event == 1) {
        return false;
    }

    // Extract 12-bit coordinates
    uint16_t x = ((p1_xh & 0x0F) << 8) | p1_xl;
    uint16_t y = ((p1_yh & 0x0F) << 8) | p1_yl;

    // Map to display dimensions (FT6x36 reports coordinates based on
    // its configured resolution; we assume the touch resolution matches
    // the display resolution)
    // Clamp to display dimensions
    if (x >= _width)  x = _width - 1;
    if (y >= _height) y = _height - 1;

    // Apply rotation
    switch (_rotation % 4) {
        case 0: break;
        case 1: { uint16_t tmp = x; x = static_cast<uint16_t>(_height - 1U - y); y = tmp; } break;
        case 2: { x = static_cast<uint16_t>(_width - 1U - x); y = static_cast<uint16_t>(_height - 1U - y); } break;
        case 3: { uint16_t tmp = x; x = y; y = static_cast<uint16_t>(_width - 1U - tmp); } break;
    }

    point.x = x;
    point.y = y;
    point.pressed = true;
    point.id = 0;
    // The short single-point transaction stops at P1_YL. A non-zero value
    // records that strength is available only through readMulti().
    point.strength = 1;

    return true;
}

// readMulti

uint8_t Touch_FT6x36::readMulti(TouchPoint* points, uint8_t maxPts) {
    if (!_initialized || !points || maxPts == 0) return 0;
    if (maxPts > maxPoints()) maxPts = maxPoints();
    for (uint8_t i = 0; i < maxPts; ++i) points[i] = TouchPoint();

    // Check interrupt pin
    if (_intPin >= 0) {
        if (digitalRead(_intPin) != LOW) {
            return 0;
        }
    }

    uint8_t buf[FT6X36_READ_LEN] = {0};
    if (!readRegisters(FT6X36_REG_GEST_ID, buf, FT6X36_READ_LEN)) {
        _gesture = FT6X36_GESTURE_NONE;
        return 0;
    }
    _gesture = buf[0];

    uint8_t numPoints = buf[1] & 0x0F;
    if (numPoints == 0) return 0;

    // Limit to requested count
    if (numPoints > maxPts) numPoints = maxPts;
    if (numPoints > FT6X36_MAX_POINTS) numPoints = FT6X36_MAX_POINTS;

    uint8_t activePoints = 0;
    for (uint8_t i = 0; i < numPoints; i++) {
        // Buffer starts at GEST_ID: point 1 is buf[2..7], point 2 buf[8..13].
        uint8_t offset = (i == 0) ? 2 : 8;
        uint8_t xh = buf[offset];
        uint8_t xl = buf[offset + 1];
        uint8_t yh = buf[offset + 2];
        uint8_t yl = buf[offset + 3];

        // Check event flag
        uint8_t event = (xh >> 6) & 0x03;
        if (event == 1) {
            continue;
        }

        uint16_t x = ((xh & 0x0F) << 8) | xl;
        uint16_t y = ((yh & 0x0F) << 8) | yl;

        if (x >= _width)  x = _width - 1;
        if (y >= _height) y = _height - 1;

        // Apply rotation
        switch (_rotation % 4) {
            case 0: break;
            case 1: { uint16_t tmp = x; x = static_cast<uint16_t>(_height - 1U - y); y = tmp; } break;
            case 2: { x = static_cast<uint16_t>(_width - 1U - x); y = static_cast<uint16_t>(_height - 1U - y); } break;
            case 3: { uint16_t tmp = x; x = y; y = static_cast<uint16_t>(_width - 1U - tmp); } break;
        }

        TouchPoint& point = points[activePoints++];
        point.x = x;
        point.y = y;
        point.pressed = true;
        point.id = static_cast<uint8_t>((yh >> 4) & 0x0F);
        point.strength = buf[offset + 4];
    }

    return activePoints;
}

// isPressed

bool Touch_FT6x36::isPressed() {
    if (!_initialized) return false;

    // Fast path: check interrupt pin
    if (_intPin >= 0) {
        if (digitalRead(_intPin) != LOW) return false;
    }

    // Read TD_STATUS to check number of touch points
    uint8_t status = readRegister(FT6X36_REG_TD_STATUS);
    if (status == 0xFF) return false;

    return (status & 0x0F) > 0;
}

// setRotation

void Touch_FT6x36::setRotation(uint8_t rotation) {
    _rotation = rotation & 3U;
}

void Touch_FT6x36::setDisplayRotation(uint8_t displayRotation) {
    setRotation(static_cast<uint8_t>(displayRotation + _mountingRotation));
}

// setDimensions

void Touch_FT6x36::setDimensions(uint16_t width, uint16_t height) {
    _width = width;
    _height = height;
}

// isConnected

bool Touch_FT6x36::isConnected() {
    // Try the configured address first
    _wire.beginTransmission(_addr);
    if (_wire.endTransmission() == 0) {
        return true;
    }

    // Integrated Indicator variants have fixed, board-verified addresses.
    // Do not silently bind GX to the DX address (or vice versa) when another
    // device happens to acknowledge on the shared I2C bus.
    if (_indicatorTuning) return false;

    // Probe all known FT5x06/FT6x36 addresses. SenseCAP Indicator GX uses
    // 0x48 while DX uses 0x38.
    const uint8_t addresses[] = {
        FT6X36_ADDR1, FT6X36_ADDR_FT6336U, FT6X36_ADDR2
    };
    for (uint8_t candidate : addresses) {
        if (candidate == _addr) continue;
        _wire.beginTransmission(candidate);
        if (_wire.endTransmission() == 0) {
            _addr = candidate;
            return true;
        }
    }

    return false;
}

// readChipID

uint8_t Touch_FT6x36::readChipID() {
    return readRegister(FT6X36_REG_CHIP_ID);
}

// readFirmwareID

uint8_t Touch_FT6x36::readFirmwareID() {
    return readRegister(FT6X36_REG_FIRMWARE_ID);
}
