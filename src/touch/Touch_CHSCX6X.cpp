/**
 * @file   Touch_CHSCX6X.cpp
 * @brief  CHSCX6X capacitive touch driver implementation
 *
 * The CHSCX6X is an I2C capacitive touch controller used on the
 * Seeed XIAO Round Display. It communicates at address 0x2E and
 * provides a 5-byte, single-touch report.
 *
 * Protocol:
 *   Byte 0: 0x01 while touched
 *   Byte 2: X coordinate (0..239)
 *   Byte 4: Y coordinate (0..239)
 *
 * Adapted from Seeed_GFX-master Touch_Drivers/CHSCX6X.cpp
 */

#include "Touch_CHSCX6X.h"

namespace {
// D7 normally provides immediate active-low notification. Polling at 62.5 Hz
// is a fallback for boards/controllers whose IRQ line stays high.
const uint32_t kIrqFallbackPollMs = 16;
}

// Constructor

Touch_CHSCX6X::Touch_CHSCX6X(int8_t intPin, TwoWire& wire,
                             uint16_t width, uint16_t height)
    : _intPin(intPin)
    , _wire(wire)
    , _rotation(0)
    , _width(width)
    , _height(height)
    , _gesture(0)
    , _lastPressed(false)
    , _lastPollMs(0)
{
}

// begin

bool Touch_CHSCX6X::begin(IBus& bus) {
    (void)bus; // CHSCX6X uses its own I2C bus, not the display bus

    // Configure interrupt pin if provided
    if (_intPin >= 0) {
        pinMode(_intPin, INPUT_PULLUP);
    }

    // The XIAO board variant supplies the official SDA=D4 / SCL=D5 defaults.
    // CHSC6X is read-only in Seeed's Arduino reference implementation.
    _wire.begin();
    delay(5);
    _lastPressed = false;
    _lastPollMs = millis() - kIrqFallbackPollMs;

    // Do not use beginTransmission()/endTransmission() as a presence probe.
    // Some CHSC6X revisions do not acknowledge an empty write even though
    // requestFrom(0x2E, 5) works, which previously aborted the entire display.
    return true;
}

// read

bool Touch_CHSCX6X::read(TouchPoint& point) {
    // D7 is an active-low hint, not an absolute gate. Some controller firmware
    // or marginal IRQ connections leave it high even though I2C reports are
    // valid. While idle, poll at a modest rate; once pressed, read every call
    // so movement and release remain responsive.
    const uint32_t now = millis();
    if (_intPin >= 0 && digitalRead(_intPin) != LOW && !_lastPressed &&
        static_cast<uint32_t>(now - _lastPollMs) < kIrqFallbackPollMs) {
        point.pressed = false;
        return false;
    }
    _lastPollMs = now;

    uint8_t buf[CHSC6X_READ_POINT_LEN] = {0};

    if (!readRawPacket(buf)) {
        point.pressed = false;
        _lastPressed = false;
        return false;
    }

    // Parse status byte
    bool touched = (buf[0] == 0x01);
    _gesture = CHSC6X_GESTURE_NONE;

    if (!touched) {
        point.pressed = false;
        _lastPressed = false;
        return false;
    }

    // Seeed's CHSC6X implementation uses bytes 2 and 4 directly. Bytes 1 and
    // 3 are not coordinate high nibbles for the Round Display protocol.
    uint16_t x = buf[CHSC6X_POINT1_X];
    uint16_t y = buf[CHSC6X_POINT1_Y];

    // Apply rotation correction (mirrors the original TFT_eSPI logic)
    switch (_rotation % 4) {
        case 0: // 0 degrees
            break;
        case 1: // 90 degrees
        {
            uint16_t tmp = x;
            x = y;
            y = (_width > 0) ? (_width - 1 - tmp) : 0;
            break;
        }
        case 2: // 180 degrees
            x = (_width > 0) ? (_width - 1 - x) : 0;
            y = (_height > 0) ? (_height - 1 - y) : 0;
            break;
        case 3: // 270 degrees
        {
            uint16_t tmp = x;
            x = (_height > 0) ? (_height - 1 - y) : 0;
            y = tmp;
            break;
        }
    }

    // Clamp to display dimensions
    if (x >= _width)  x = _width - 1;
    if (y >= _height) y = _height - 1;

    point.x = x;
    point.y = y;
    point.pressed = true;
    point.id = 0;
    _lastPressed = true;

    return true;
}

// readMulti

uint8_t Touch_CHSCX6X::readMulti(TouchPoint* points, uint8_t maxPts) {
    if (!points || maxPts == 0) return 0;

    uint8_t buf[CHSC6X_READ_POINT_LEN] = {0};

    if (!readRawPacket(buf)) {
        return 0;
    }

    bool touched = (buf[0] == 0x01);
    _gesture = CHSC6X_GESTURE_NONE;

    if (!touched) {
        return 0;
    }

    // CHSCX6X reports one point in the standard 5-byte frame
    uint16_t x = buf[CHSC6X_POINT1_X];
    uint16_t y = buf[CHSC6X_POINT1_Y];

    // Apply rotation
    switch (_rotation % 4) {
        case 0: break;
        case 1: { uint16_t tmp = x; x = y; y = (_width > 0) ? (_width - 1 - tmp) : 0; } break;
        case 2: x = (_width > 0) ? (_width - 1 - x) : 0;
                y = (_height > 0) ? (_height - 1 - y) : 0; break;
        case 3: { uint16_t tmp = x; x = (_height > 0) ? (_height - 1 - y) : 0; y = tmp; } break;
    }

    if (x >= _width)  x = _width - 1;
    if (y >= _height) y = _height - 1;

    points[0].x = x;
    points[0].y = y;
    points[0].pressed = true;
    points[0].id = 0;

    return 1;
}

// isPressed

bool Touch_CHSCX6X::isPressed() {
    TouchPoint pt;
    return read(pt);
}

// setRotation

void Touch_CHSCX6X::setRotation(uint8_t rotation) {
    _rotation = rotation;
}

// setDimensions

void Touch_CHSCX6X::setDimensions(uint16_t width, uint16_t height) {
    _width = width;
    _height = height;
}

// isConnected

bool Touch_CHSCX6X::isConnected() {
    uint8_t scratch[CHSC6X_READ_POINT_LEN] = {0};
    return readRawPacket(scratch);
}

// reset

void Touch_CHSCX6X::reset() {
    // No software-reset register is documented by Seeed for this controller.
    // Reinitialize only the local bus/IRQ state; do not send speculative writes.
    if (_intPin >= 0) pinMode(_intPin, INPUT_PULLUP);
    _wire.begin();
    delay(5);
    _lastPressed = false;
    _lastPollMs = millis() - kIrqFallbackPollMs;
}

// readRaw (private)

bool Touch_CHSCX6X::readRawPacket(uint8_t* buf) {
    size_t readLen = _wire.requestFrom(CHSC6X_I2C_ADDR, CHSC6X_READ_POINT_LEN);

    if (readLen < CHSC6X_READ_POINT_LEN) {
        return false;
    }

    // Read all bytes from the I2C buffer
    for (size_t i = 0; i < CHSC6X_READ_POINT_LEN; i++) {
        buf[i] = _wire.read();
    }

    return true;
}
