/**
 * @file   Touch_CST9217.cpp
 * @brief  CST9217 capacitive touch driver implementation
 *
 * Every I2C sequence, settle time and retry policy mirrors the official
 * Espressif/Waveshare esp_lcd_touch_cst9217 driver that the board BSP links
 * (managed_components/waveshare__esp_lcd_touch_cst9217):
 *
 *   read data  : write pointer 0xD000 -> 2 ms -> read 10 bytes;
 *                data[6] must be 0xAB, point count = data[5] & 0x7F,
 *                point "hold" event = 0x06, coords are 12-bit split
 *                x = (b1<<4)|(b3>>4), y = (b2<<4)|(b3&0x0F).
 *   read/write : up to 5 tries, 3 ms back-off; on final failure pulse the
 *                reset line (10 ms low / 100 ms high) when one is wired.
 *   begin()    : (optional hw reset 10/50 ms) -> write 0xD101 {0xD1,0x01}
 *                (enter command mode) -> 10 ms -> read 0xD1FC (checkcode,
 *                ignored), 0xD1F8 (resolution), 0xD204 (project + chip ID).
 *
 * The CST92xx INT line is a short pulse (the chip reports "level once per
 * second"), so it is used only as a trigger hint; polling is throttled on a
 * timer.  No gesture decode - the official driver ignores byte 4 too.
 */

#include "Touch_CST9217.h"

// Constructor

Touch_CST9217::Touch_CST9217(int8_t intPin, TwoWire& wire,
                             uint8_t addr, uint16_t width, uint16_t height,
                             int8_t rstPin, int8_t sda, int8_t scl,
                             uint32_t frequency, bool manageWire)
    : _intPin(intPin)
    , _rstPin(rstPin)
    , _wire(wire)
    , _addr(addr)
    , _rotation(0)
    , _width(width)
    , _height(height)
    , _sda(sda)
    , _scl(scl)
    , _frequency(frequency)
    , _manageWire(manageWire)
    , _lastPressed(false)
    , _lastPollMs(0)
    , _resX(0)
    , _resY(0)
    , _chipType(0)
    , _projectId(0)
{
}

// begin

bool Touch_CST9217::begin(IBus& bus) {
    (void)bus; // CST9217 uses its own I2C bus, not the display bus

    if (_manageWire) {
        if (_sda >= 0 && _scl >= 0) {
            _wire.begin(_sda, _scl, _frequency);
        } else {
            _wire.begin();
        }
    }

    // INT is a pulse hint, not a level we can gate on - but still configure
    // it as an input so the pin is not left floating.
    if (_intPin >= 0) {
        pinMode(_intPin, INPUT);
    }

    // Optional hardware reset (official cst9217_reset: 10 ms low, 50 ms high).
    // Default -1: on this panel RST is shared with the LCD reset line and
    //         display.begin() already pulsed it.
    if (_rstPin >= 0) {
        pinMode(_rstPin, OUTPUT);
        hwReset();
    }

    // Identity + resolution via the official command-mode config read.
    if (!readConfig()) {
        return false;
    }

    _lastPressed = false;
    _lastPollMs  = millis() - CST9217_POLL_MS;
    return true;
}

// read

bool Touch_CST9217::read(TouchPoint& point) {
    // Idle poll throttle: 16 ms while untouched, read every call once pressed
    // so movement and release stay responsive.  The INT pulse merely skips a
    // poll wait right after a press.
    const uint32_t now = millis();
    if (!_lastPressed && static_cast<uint32_t>(now - _lastPollMs) < CST9217_POLL_MS) {
        point.pressed = false;
        return false;
    }
    _lastPollMs = now;

    uint8_t data[CST9217_READ_LEN] = {0};

    // Official esp_lcd_touch_cst9217_read_data
    if (!readReg(CST9217_REG_DATA, data, sizeof(data))) {
        point.pressed = false;
        _lastPressed = false;
        return false;
    }

    if (data[6] != CST9217_ACK_VALUE) {
        point.pressed = false;
        _lastPressed = false;
        return false;
    }

    // Point count with the official cap at 1 point.
    uint8_t points = data[5] & 0x7F;
    if (points == 0) {
        point.pressed = false;
        _lastPressed = false;
        return false;
    }

    // Single-point report: point 0 starts at offset 0 (header is 5 bytes,
    // the (i ? 2 : 0) skip in the official loop only matters for point 1+).
    const uint8_t status = data[0] & 0x0F;
    if (status != CST9217_EVENT_HOLD) {
        point.pressed = false;   // release / other event code
        _lastPressed = false;
        return false;
    }

    uint16_t x = static_cast<uint16_t>((data[1] << 4) | (data[3] >> 4));
    uint16_t y = static_cast<uint16_t>((data[2] << 4) | (data[3] & 0x0F));

    _lastPressed = true;

    // Board-native mounting transform.  The Waveshare BSP for this panel
    // configures esp_lcd_touch with swap_xy=1, mirror_x=0, mirror_y=1 and
    // x_max = y_max = 480 at display rotation 0 (verified config, used by
    // the working 12_weather_pet demo), and esp_lcd_touch applies the
    // mirrors BEFORE the swap:
    //     display_x = y_max - raw_y ;  display_y = raw_x
    // Without this the raw report is transposed/mirrored against the
    // rotation-0 framebuffer (dots land away from the finger, in-pad
    // touches report out-of-pad).  applyRotation() below then composes any
    // additional display rotation on top of this native space and clamps.
    const uint16_t rx = x, ry = y;
    const uint16_t yMax = (_resY > 0) ? _resY : _height;
    x = static_cast<uint16_t>(yMax - ry);   // ry=0 -> yMax, clamped below
    y = rx;

    applyRotation(x, y);

    point.x = x;
    point.y = y;
    point.pressed = true;
    point.id = 0;         // single point, no ID tracking
    point.strength = 1;   // official get_xy reports strength = 1
    return true;
}

// readMulti

uint8_t Touch_CST9217::readMulti(TouchPoint* points, uint8_t maxPts) {
    if (!points || maxPts == 0) return 0;
    return read(points[0]) ? 1U : 0U;
}

// isPressed

bool Touch_CST9217::isPressed() {
    TouchPoint pt;
    return read(pt);
}

// setRotation

void Touch_CST9217::setRotation(uint8_t rotation) {
    _rotation = rotation & 3;
}

// setDimensions

void Touch_CST9217::setDimensions(uint16_t width, uint16_t height) {
    _width  = width;
    _height = height;
}

// isConnected

bool Touch_CST9217::isConnected() {
    uint8_t data[CST9217_READ_LEN] = {0};
    return readReg(CST9217_REG_DATA, data, sizeof(data));
}

// ---------------------------------------------------------------------------
// Register access - official timing (esp_lcd_touch_cst9217.c)
// ---------------------------------------------------------------------------

bool Touch_CST9217::writePointer(uint16_t reg) {
    _wire.beginTransmission(_addr);
    _wire.write(reg >> 8);
    _wire.write(reg & 0xFF);
    if (_wire.endTransmission() != 0) {
        return false;
    }
    delay(2);   // official: settle before the payload/read phase
    return true;
}

bool Touch_CST9217::readReg(uint16_t reg, uint8_t* buf, size_t len) {
    for (int retry = 0; retry < 5; retry++) {
        if (!writePointer(reg)) {
            delay(3);
            continue;
        }

        const size_t got = _wire.requestFrom(static_cast<int>(_addr), static_cast<int>(len));
        if (got >= len) {
            for (size_t i = 0; i < len; i++) {
                buf[i] = static_cast<uint8_t>(_wire.read());
            }
            return true;
        }

        // Drain whatever came back so a partial read does not poison the bus.
        while (_wire.available()) _wire.read();
        delay(3);
    }

    // Official failure path: pulse reset when a line is wired.
    if (_rstPin >= 0) {
        hwReset();
    }
    return false;
}

bool Touch_CST9217::writeReg(uint16_t reg, const uint8_t* data, size_t len) {
    for (int retry = 0; retry < 5; retry++) {
        if (!writePointer(reg)) {
            delay(3);
            continue;
        }

        _wire.beginTransmission(_addr);
        for (size_t i = 0; i < len; i++) {
            _wire.write(data[i]);
        }
        if (_wire.endTransmission() == 0) {
            return true;
        }
        delay(3);
    }

    if (_rstPin >= 0) {
        hwReset();
    }
    return false;
}

void Touch_CST9217::hwReset() {
    if (_rstPin < 0) return;
    pinMode(_rstPin, OUTPUT);
    _wire.flush();
    digitalWrite(_rstPin, LOW);
    delay(10);
    digitalWrite(_rstPin, HIGH);
    delay(50);
}

bool Touch_CST9217::readConfig() {
    // Enter command mode (official: write 0xD101 with {0xD1,0x01}).
    const uint8_t cmdMode[2] = {0xD1, 0x01};
    if (!writeReg(CST9217_REG_CMD_MODE, cmdMode, sizeof(cmdMode))) {
        return false;
    }
    delay(10);

    // Checkcode (0xD1FC) - logged/ignored by the official driver.
    uint8_t data[4] = {0};
    if (!readReg(CST9217_REG_CHECKCODE, data, sizeof(data))) {
        return false;
    }

    // Resolution (0xD1F8): little-endian 16-bit X then Y.
    if (!readReg(CST9217_REG_RESOLUTION, data, sizeof(data))) {
        return false;
    }
    _resX = static_cast<uint16_t>((data[1] << 8) | data[0]);
    _resY = static_cast<uint16_t>((data[3] << 8) | data[2]);

    // Project ID + chip type (0xD204).
    if (!readReg(CST9217_REG_PROJECT_ID, data, sizeof(data))) {
        return false;
    }
    _projectId = static_cast<uint16_t>((data[1] << 8) | data[0]);
    _chipType  = static_cast<uint16_t>((data[3] << 8) | data[2]);

    return true;
}

void Touch_CST9217::applyRotation(uint16_t& x, uint16_t& y) {
    switch (_rotation % 4) {
        case 0:  break;
        case 1: { uint16_t t = x; x = y; y = static_cast<uint16_t>(_width  > 0 ? _width  - 1 - t : 0); } break;
        case 2:  x = static_cast<uint16_t>(_width  > 0 ? _width  - 1 - x : 0);
                 y = static_cast<uint16_t>(_height > 0 ? _height - 1 - y : 0); break;
        case 3: { uint16_t t = x; x = static_cast<uint16_t>(_height > 0 ? _height - 1 - y : 0); y = t; } break;
    }
    if (x >= _width)  x = static_cast<uint16_t>(_width  > 0 ? _width  - 1 : 0);
    if (y >= _height) y = static_cast<uint16_t>(_height > 0 ? _height - 1 : 0);
}