/**
 * @file   Touch_AXS5106L.cpp
 * @brief  AXS5106L capacitive touch driver implementation
 *
 * I2C touch controller (0x63) on the Seeed XIAO Display Board 1.47". Reads a
 * 14-byte report from register 0x01: byte 1 = touch count, then up to two
 * 6-byte points (X/Y are 12-bit, split as high nibble + low byte). Adapted
 * from XIAO-Display-Board-main axs5106l_device.cpp.
 */

#include "Touch_AXS5106L.h"

namespace {
// INT is an active-low hint. Poll at ~62 Hz as a fallback when the IRQ line
// stays high; once pressed, read every call so movement/release stay responsive.
const uint32_t kPollMs = 16;
}

// Constructor

Touch_AXS5106L::Touch_AXS5106L(int8_t rstPin, int8_t intPin, TwoWire& wire,
                               uint16_t width, uint16_t height)
    : _rstPin(rstPin)
    , _intPin(intPin)
    , _wire(wire)
    , _rotation(0)
    , _width(width)
    , _height(height)
    , _lastPressed(false)
    , _lastPollMs(0)
{
}

// begin

bool Touch_AXS5106L::begin(IBus& bus) {
    (void)bus; // AXS5106L uses its own I2C bus, not the display bus

    if (_intPin >= 0) {
        pinMode(_intPin, INPUT_PULLUP);
    }
    _wire.begin();
    delay(5);

    // Hardware reset (RST is shared with the LCD RST line on the XIAO Display
    // Board). The reference driver holds RST low 200 ms then high 300 ms.
    if (_rstPin >= 0) {
        pinMode(_rstPin, OUTPUT);
        digitalWrite(_rstPin, LOW);
        delay(200);
        digitalWrite(_rstPin, HIGH);
        delay(300);
    }

    _lastPressed = false;
    _lastPollMs = millis() - kPollMs;
    // Do not probe presence via beginTransmission: some revisions do not ACK an
    // empty write even though register reads work.
    return true;
}

// read

bool Touch_AXS5106L::read(TouchPoint& point) {
    const uint32_t now = millis();
    if (_intPin >= 0 && digitalRead(_intPin) != LOW && !_lastPressed &&
        static_cast<uint32_t>(now - _lastPollMs) < kPollMs) {
        point.pressed = false;
        return false;
    }
    _lastPollMs = now;

    uint8_t buf[AXS5106L_REPORT_LEN] = {0};
    if (!readReport(buf, AXS5106L_REPORT_LEN)) {
        point.pressed = false;
        _lastPressed = false;
        return false;
    }

    const uint8_t touchNum = buf[1];
    if (touchNum == 0) {
        point.pressed = false;
        _lastPressed = false;
        return false;
    }

    // Point 0: X = high nibble of buf[2] (<<8) | buf[3]; Y likewise from [4]/[5].
    uint16_t x = static_cast<uint16_t>((buf[2] & 0x0f) << 8) | buf[3];
    uint16_t y = static_cast<uint16_t>((buf[4] & 0x0f) << 8) | buf[5];
    applyRotation(x, y);
    if (_width > 0 && x >= _width)  x = _width - 1;
    if (_height > 0 && y >= _height) y = _height - 1;

    point.x = x;
    point.y = y;
    point.pressed = true;
    point.id = 0;
    _lastPressed = true;
    return true;
}

// readMulti

uint8_t Touch_AXS5106L::readMulti(TouchPoint* points, uint8_t maxPts) {
    if (!points || maxPts == 0) return 0;

    uint8_t buf[AXS5106L_REPORT_LEN] = {0};
    if (!readReport(buf, AXS5106L_REPORT_LEN)) return 0;

    uint8_t touchNum = buf[1];
    if (touchNum == 0) return 0;
    if (touchNum > AXS5106L_MAX_POINTS) touchNum = AXS5106L_MAX_POINTS;
    if (touchNum > maxPts) touchNum = maxPts;

    for (uint8_t i = 0; i < touchNum; ++i) {
        uint16_t x = static_cast<uint16_t>((buf[2 + i * 6] & 0x0f) << 8) | buf[3 + i * 6];
        uint16_t y = static_cast<uint16_t>((buf[4 + i * 6] & 0x0f) << 8) | buf[5 + i * 6];
        applyRotation(x, y);
        if (_width > 0 && x >= _width)  x = _width - 1;
        if (_height > 0 && y >= _height) y = _height - 1;
        points[i].x = x;
        points[i].y = y;
        points[i].pressed = true;
        points[i].id = i;
    }
    return touchNum;
}

// isPressed

bool Touch_AXS5106L::isPressed() {
    TouchPoint pt;
    return read(pt);
}

// setRotation / isConnected

void Touch_AXS5106L::setRotation(uint8_t rotation) {
    _rotation = rotation;
}

bool Touch_AXS5106L::isConnected() {
    uint8_t scratch[AXS5106L_REPORT_LEN] = {0};
    return readReport(scratch, AXS5106L_REPORT_LEN);
}

// readReport (private)

bool Touch_AXS5106L::readReport(uint8_t* buf, uint8_t len) {
    // Select the touch-data register, then fetch the report.
    _wire.beginTransmission(AXS5106L_I2C_ADDR);
    _wire.write(AXS5106L_TOUCH_DATA_REG);
    if (_wire.endTransmission() != 0) return false;

    const size_t n = _wire.requestFrom(AXS5106L_I2C_ADDR, len);
    if (n < len) return false;
    for (size_t i = 0; i < len; ++i) buf[i] = _wire.read();
    return true;
}

// applyRotation (private)

void Touch_AXS5106L::applyRotation(uint16_t& x, uint16_t& y) const {
    // _width/_height are the native (portrait) dimensions; the controller
    // reports in that range. The 1.47" Touch Display pairs this controller
    // with the JD9853A LCD, whose Driver_JD9853A sets MADCTL=MX (X mirrored)
    // at rotation 0 — so the touch X must be mirrored to match the displayed X.
    // (AXS5106L is 1.47-Touch-only in this library, so this is safe.)
    switch (_rotation % 4) {
        case 0: // 0 degrees — JD9853A MADCTL=MX: mirror X to match the LCD
            x = (_width > 0) ? static_cast<uint16_t>(_width - 1 - x) : x;
            break;
        case 1: { // 90 degrees
            uint16_t tmp = x;
            x = y;
            y = (_width > 0) ? static_cast<uint16_t>(_width - 1 - tmp) : 0;
            break;
        }
        case 2: // 180 degrees
            x = (_width > 0) ? static_cast<uint16_t>(_width - 1 - x) : 0;
            y = (_height > 0) ? static_cast<uint16_t>(_height - 1 - y) : 0;
            break;
        case 3: { // 270 degrees
            uint16_t tmp = x;
            x = (_height > 0) ? static_cast<uint16_t>(_height - 1 - y) : 0;
            y = tmp;
            break;
        }
    }
}
