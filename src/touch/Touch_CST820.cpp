/**
 * @file   Touch_CST820.cpp
 * @brief  CST820 / CST820B capacitive touch driver implementation
 *
 * Single-point report, CST8xx family shared layout (verified against the
 * working esp_lcd_touch_cst820 component and lewis-he SensorLib CST816 —
 * see platformio/Espidf_amoled_pio/components/esp_lcd_touch_cst820):
 *
 *     reg 0x02 = touch count (LOW nibble; 0xFF marks an invalid frame)
 *     reg 0x03 = x[11:8] in bits 3..0 (high nibble reserved)
 *     reg 0x04 = x[7:0]
 *     reg 0x05 = y[11:8] in bits 3..0 (high nibble reserved)
 *     reg 0x06 = y[7:0]
 *
 *     x = ((xh & 0x0F) << 8) | xl ;  y = ((yh & 0x0F) << 8) | yl   (12-bit)
 *
 * Coordinates are then run through the native mounting transform
 * (compile-time CST820_SWAP_XY / MIRROR_X / MIRROR_Y toggles) and a display
 * rotation + clamp to the panel dimensions.
 */

#include "Touch_CST820.h"

Touch_CST820::Touch_CST820(int8_t intPin, TwoWire& wire,
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
{
}

bool Touch_CST820::begin(IBus& bus) {
    (void)bus;   /* CST820 uses its own I2C, not the display bus */

    if (_manageWire) {
        if (_sda >= 0 && _scl >= 0) _wire.begin(_sda, _scl, _frequency);
        else                         _wire.begin();
    }

    if (_intPin >= 0) {
        pinMode(_intPin, INPUT);
    }

    if (_rstPin >= 0) {
        pinMode(_rstPin, OUTPUT);
        hwReset();
    }

    /* Probe: a valid chip id read proves the part is on the bus. */
    uint8_t id = 0;
    if (!readChipId(id)) {
        return false;
    }

    _lastPressed = false;
    _lastPollMs  = millis() - CST820_POLL_MS;
    return true;
}

bool Touch_CST820::read(TouchPoint& point) {
    const uint32_t now = millis();
    if (!_lastPressed && static_cast<uint32_t>(now - _lastPollMs) < CST820_POLL_MS) {
        point.pressed = false;
        return false;
    }
    _lastPollMs = now;

    uint8_t data[CST820_DATA_LEN] = {0};
    if (!readReg(CST820_REG_DATA, data, sizeof(data))) {
        point.pressed = false;
        _lastPressed = false;
        return false;
    }

    /* data[0] = touch count (register 0x02, LOW nibble).  0xFF marks an
     * invalid frame (CST8xx autosleep wake glitch), 0 = released. */
    const uint8_t points = data[0] & 0x0F;
    if (data[0] == 0xFF || points == 0) {
        point.pressed = false;
        _lastPressed = false;
        return false;
    }

    /* CST8xx 12-bit coords: x[11:8] live in the LOW nibble of x_high (0x03),
     * x[7:0] in x_low (0x04); same for y at 0x05/0x06.  Matches the proven
     * esp_lcd_touch_cst820 / SensorLib CST816 decode. */
    uint16_t x = static_cast<uint16_t>(((data[1] & 0x0F) << 8) | data[2]);
    uint16_t y = static_cast<uint16_t>(((data[3] & 0x0F) << 8) | data[4]);

    /* Native mounting transform (panel glass frame -> framebuffer frame). */
#if CST820_SWAP_XY
    uint16_t t = x; x = y; y = t;
#endif
#if CST820_MIRROR_X
    x = static_cast<uint16_t>(_width  > 0 ? _width  - 1 - x : 0);
#endif
#if CST820_MIRROR_Y
    y = static_cast<uint16_t>(_height > 0 ? _height - 1 - y : 0);
#endif

    applyRotation(x, y);

    _lastPressed = true;

    point.x = x;
    point.y = y;
    point.pressed = true;
    point.id = 0;
    point.strength = 1;
    return true;
}

uint8_t Touch_CST820::readMulti(TouchPoint* points, uint8_t maxPts) {
    if (!points || maxPts == 0) return 0;
    return read(points[0]) ? 1U : 0U;
}

bool Touch_CST820::isPressed() {
    TouchPoint pt;
    return read(pt);
}

void Touch_CST820::setRotation(uint8_t rotation) {
    _rotation = rotation & 3;
}

bool Touch_CST820::readChipId(uint8_t& id) {
    return readReg(CST820_CHIP_ID_REG, &id, 1);
}

bool Touch_CST820::readReg(uint8_t reg, uint8_t* buf, size_t len) {
    for (int retry = 0; retry < 5; retry++) {
        _wire.beginTransmission(_addr);
        _wire.write(reg);
        if (_wire.endTransmission(false) != 0) {   /* keep the bus held */
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
        while (_wire.available()) _wire.read();   /* drain partial */
        delay(3);
    }
    if (_rstPin >= 0) {
        hwReset();
    }
    return false;
}

void Touch_CST820::hwReset() {
    if (_rstPin < 0) return;
    pinMode(_rstPin, OUTPUT);
    _wire.flush();
    digitalWrite(_rstPin, LOW);
    delay(10);
    digitalWrite(_rstPin, HIGH);
    delay(50);
}

void Touch_CST820::applyRotation(uint16_t& x, uint16_t& y) {
    switch (_rotation % 4) {
        case 0: break;
        case 1: { uint16_t t = x; x = y; y = static_cast<uint16_t>(_width  > 0 ? _width  - 1 - t : 0); } break;
        case 2:  x = static_cast<uint16_t>(_width  > 0 ? _width  - 1 - x : 0);
                 y = static_cast<uint16_t>(_height > 0 ? _height - 1 - y : 0); break;
        case 3: { uint16_t t = x; x = static_cast<uint16_t>(_height > 0 ? _height - 1 - y : 0); y = t; } break;
    }
    if (x >= _width)  x = static_cast<uint16_t>(_width  > 0 ? _width  - 1 : 0);
    if (y >= _height) y = static_cast<uint16_t>(_height > 0 ? _height - 1 : 0);
}