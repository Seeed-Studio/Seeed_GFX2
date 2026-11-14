#include "Touch_GT911.h"

Touch_GT911::Touch_GT911(int8_t intPin, TwoWire& wire, uint8_t addr,
                         uint16_t width, uint16_t height,
                         int8_t rstPin, int8_t sda, int8_t scl,
                         uint32_t frequency, bool manageWire)
    : _intPin(intPin), _rstPin(rstPin), _wire(wire), _addr(addr),
      _rotation(0), _width(width), _height(height),
      _sda(sda), _scl(scl), _frequency(frequency), _manageWire(manageWire),
      _initialized(false)
{
}

bool Touch_GT911::begin(IBus& bus) {
    (void)bus;   /* GT911 talks on its own I2C, not the display bus */

    if (_intPin >= 0) {
        pinMode(_intPin, INPUT);   /* INT is an open-drain output from the chip */
    }

    if (_manageWire) {
#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
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

    if (_rstPin >= 0) {
        hwReset();
    }

    /* Probe: a valid "911" chip ID proves the part is on the bus. */
    char id[3] = {0, 0, 0};
    if (!readProductId(id) || id[0] != '9' || id[1] != '1' || id[2] != '1') {
        return false;
    }

    _initialized = true;
    return true;
}

bool Touch_GT911::read(TouchPoint& point) {
    point = TouchPoint();
    return readPoints(&point, 1) == 1;
}

uint8_t Touch_GT911::readMulti(TouchPoint* points, uint8_t maxPts) {
    return readPoints(points, maxPts);
}

bool Touch_GT911::isPressed() {
    if (!_initialized) return false;
    uint8_t status = 0;
    if (!readReg(GT911_REG_STATUS, &status, 1)) return false;
    return (status & 0x80) && (status & 0x0F);
}

void Touch_GT911::setRotation(uint8_t rotation) {
    _rotation = rotation & 3U;
}

/* ---- register IO: 16-bit big-endian register address --------------------- */

bool Touch_GT911::readReg(uint16_t reg, uint8_t* buf, size_t len) {
    _wire.beginTransmission(_addr);
    _wire.write((uint8_t)(reg >> 8));
    _wire.write((uint8_t)(reg & 0xFF));
    if (_wire.endTransmission(false) != 0) return false;
    if (_wire.requestFrom(_addr, (uint8_t)len) != len) return false;
    for (size_t i = 0; i < len; i++) buf[i] = (uint8_t)_wire.read();
    while (_wire.available()) _wire.read();   /* drain any overrun */
    return true;
}

bool Touch_GT911::writeReg(uint16_t reg, const uint8_t* data, size_t len) {
    _wire.beginTransmission(_addr);
    _wire.write((uint8_t)(reg >> 8));
    _wire.write((uint8_t)(reg & 0xFF));
    for (size_t i = 0; i < len; i++) _wire.write(data[i]);
    return _wire.endTransmission() == 0;
}

bool Touch_GT911::readProductId(char id[3]) {
    uint8_t buf[4] = {0};
    if (!readReg(GT911_REG_PRODUCT_ID, buf, sizeof(buf))) return false;
    id[0] = (char)buf[0];
    id[1] = (char)buf[1];
    id[2] = (char)buf[2];
    return true;
}

/* ---- reset / address select (only used when rstPin is a real GPIO) ------ */

void Touch_GT911::hwReset() {
    if (_rstPin < 0) return;
    /* INT low at the RESET release edge selects the primary 0x5D address. */
    const bool primary = (_addr != GT911_I2C_ADDR_BACKUP);

    if (_intPin >= 0) {
        pinMode(_intPin, OUTPUT);
        digitalWrite(_intPin, HIGH);         /* INT idle high */
    }
    pinMode(_rstPin, OUTPUT);
    digitalWrite(_rstPin, HIGH);
    delay(2);

    digitalWrite(_rstPin, LOW);              /* assert reset      */
    delay(10);
    digitalWrite(_rstPin, HIGH);             /* release (pulse 1) */
    delay(10);

    digitalWrite(_rstPin, LOW);              /* assert again */
    if (_intPin >= 0) digitalWrite(_intPin, LOW);
    delay(20);
    delay(20);                               /* GT_RD_CMD window, INT stays low */

    if (_intPin >= 0)                        /* address select */
        digitalWrite(_intPin, primary ? LOW : HIGH);
    digitalWrite(_rstPin, HIGH);             /* release; INT level latched */
    delay(20);

    if (_intPin >= 0) pinMode(_intPin, INPUT);
}

/* ---- report decode ------------------------------------------------------- */

uint8_t Touch_GT911::readPoints(TouchPoint* points, uint8_t maxPts) {
    if (!_initialized || !points || maxPts == 0) return 0;

    /* Read the one-byte status register first — the canonical GT911 flow, same
       as esp_lcd_touch_gt911.  Only revisit the point records when the
       buffer-ready flag is actually set; this also avoids a blind 41-byte read
       whose register auto-increment is not guaranteed across firmware revs. */
    uint8_t status = 0;
    if (!readReg(GT911_REG_STATUS, &status, 1)) return 0;

    if ((status & 0x80) == 0) return 0;      /* no fresh report */
    uint8_t count = status & 0x0F;
    if (count == 0) {                        /* ready flag set but zero points */
        const uint8_t zero = 0;
        writeReg(GT911_REG_STATUS, &zero, 1);
        return 0;
    }

    if (count > maxPts)            count = maxPts;
    if (count > GT911_MAX_POINTS)  count = GT911_MAX_POINTS;

    uint8_t buf[GT911_MAX_POINTS * 8] = {0};
    if (!readReg(GT911_REG_POINT1, buf, (size_t)count * 8)) {
        const uint8_t zero = 0;
        writeReg(GT911_REG_STATUS, &zero, 1);
        return 0;
    }

    /* Clear the buffer so the controller can queue the next report. */
    const uint8_t zero = 0;
    writeReg(GT911_REG_STATUS, &zero, 1);

    uint8_t valid = 0;
    for (uint8_t i = 0; i < count; i++) {
        const uint8_t* p = &buf[i * 8];
        /* p[0] is the GT911 track ID, 0..15 — 0 is a LIVE touch, not an empty
           slot.  The status byte's low nibble already tells how many slots are
           valid, so decode every reported slot. */
        uint16_t x = (uint16_t)(((uint16_t)(p[2] & 0x0F) << 8) | p[1]);
        uint16_t y = (uint16_t)(((uint16_t)(p[4] & 0x0F) << 8) | p[3]);

        if (x >= _width)  x = (uint16_t)(_width  - 1);
        if (y >= _height) y = (uint16_t)(_height - 1);

#if GT911_SWAP_XY
        { uint16_t t = x; x = y; y = t; }
#endif
#if GT911_MIRROR_X
        x = (uint16_t)(_width  - 1 - x);
#endif
#if GT911_MIRROR_Y
        y = (uint16_t)(_height - 1 - y);
#endif
        applyRotation(x, y);

        TouchPoint& pt = points[valid++];
        pt.x = x;
        pt.y = y;
        pt.pressed = true;
        pt.id = p[0];
        pt.strength = (uint16_t)(((uint16_t)p[6] << 8) | p[5]);
    }
    return valid;
}

void Touch_GT911::applyRotation(uint16_t& x, uint16_t& y) {
    switch (_rotation) {
        case 0: break;
        case 1: {
            uint16_t t = x;
            x = (uint16_t)(_height - 1 - y);
            y = t;
            break;
        }
        case 2:
            x = (uint16_t)(_width  - 1 - x);
            y = (uint16_t)(_height - 1 - y);
            break;
        case 3: {
            uint16_t t = x;
            x = y;
            y = (uint16_t)(_width - 1 - t);
            break;
        }
    }
    if (x >= _width)  x = (uint16_t)(_width  - 1);
    if (y >= _height) y = (uint16_t)(_height - 1);
}