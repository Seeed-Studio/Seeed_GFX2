#include "Touch_SPD2010.h"
#include "../core/Bus.h"

Touch_SPD2010::Touch_SPD2010(TwoWire& wire, int8_t sda, int8_t scl,
                             uint16_t width, uint16_t height,
                             uint8_t address, uint32_t frequency)
    : _wire(wire), _sda(sda), _scl(scl), _width(width), _height(height),
      _address(address), _frequency(frequency), _rotation(0),
      _gesture(0), _initialized(false) {}

bool Touch_SPD2010::writeBytes(const uint8_t* data, size_t len) {
    _wire.beginTransmission(_address);
    if (_wire.write(data, len) != len) {
        (void)_wire.endTransmission();
        return false;
    }
    return _wire.endTransmission() == 0;
}

bool Touch_SPD2010::readBytes(uint8_t* data, size_t len) {
    const size_t received = _wire.requestFrom(_address, len);
    if (received != len) return false;
    for (size_t i = 0; i < len; ++i) {
        data[i] = static_cast<uint8_t>(_wire.read());
    }
    return true;
}

bool Touch_SPD2010::writeCommand(uint8_t command) {
    const uint8_t data[4] = {command, 0x00, 0x01, 0x00};
    const bool ok = writeBytes(data, sizeof(data));
    delayMicroseconds(200);
    return ok;
}

bool Touch_SPD2010::clearInterrupt() {
    return writeCommand(0x02);
}

bool Touch_SPD2010::readStatus(uint8_t status[4]) {
    const uint8_t address[2] = {0x20, 0x00};
    if (!writeBytes(address, sizeof(address))) return false;
    delayMicroseconds(200);
    const bool ok = readBytes(status, 4);
    delayMicroseconds(200);
    return ok;
}

bool Touch_SPD2010::begin(IBus& bus) {
    (void)bus;
#if defined(ARDUINO_ARCH_ESP32)
    if (!_wire.begin(_sda, _scl, _frequency)) return false;
#else
    _wire.begin();
    (void)_sda;
    (void)_scl;
    (void)_frequency;
#endif
    _wire.beginTransmission(_address);
    if (_wire.endTransmission() != 0) return false;

    // A firmware-version read also wakes/probes the touch CPU, matching the
    // official SPD2010 component's startup sequence.
    const uint8_t versionAddress[2] = {0x26, 0x00};
    uint8_t version[18] = {};
    if (!writeBytes(versionAddress, sizeof(versionAddress))) return false;
    delayMicroseconds(200);
    if (!readBytes(version, sizeof(version))) return false;
    delay(50);

    _initialized = true;
    TouchPoint ignored;
    (void)read(ignored);
    delay(100);
    return true;
}

void Touch_SPD2010::rotate(uint16_t& x, uint16_t& y) const {
    const uint16_t originalX = x;
    const uint16_t originalY = y;
    switch (_rotation) {
        case 0:
            break;
        case 1:
            x = static_cast<uint16_t>(_height - 1U - originalY);
            y = originalX;
            break;
        case 2:
            x = static_cast<uint16_t>(_width - 1U - originalX);
            y = static_cast<uint16_t>(_height - 1U - originalY);
            break;
        default:
            x = originalY;
            y = static_cast<uint16_t>(_width - 1U - originalX);
            break;
    }
}

bool Touch_SPD2010::read(TouchPoint& point) {
    return readMulti(&point, 1) == 1;
}

uint8_t Touch_SPD2010::readMulti(TouchPoint* points, uint8_t maxPts) {
    if (!points || maxPts == 0) return 0;
    if (maxPts > maxPoints()) maxPts = maxPoints();
    for (uint8_t i = 0; i < maxPts; ++i) points[i] = TouchPoint();
    _gesture = 0;
    if (!_initialized) return 0;

    uint8_t status[4] = {};
    if (!readStatus(status)) return 0;
    const bool pointExists = (status[0] & 0x01U) != 0;
    const bool gestureExists = (status[0] & 0x02U) != 0;
    const bool auxiliary = (status[0] & 0x08U) != 0;
    const bool cpuRun = (status[1] & 0x08U) != 0;
    const bool inCpu = (status[1] & 0x20U) != 0;
    const bool inBios = (status[1] & 0x40U) != 0;
    const uint16_t readLength =
        static_cast<uint16_t>(status[2] | (status[3] << 8));

    if (inBios) {
        if (!clearInterrupt()) return 0;
        (void)writeCommand(0x04);
        return 0;
    }
    if (inCpu) {
        const uint8_t pointMode[4] = {0x50, 0x00, 0x00, 0x00};
        const uint8_t start[4] = {0x46, 0x00, 0x00, 0x00};
        if (!writeBytes(pointMode, sizeof(pointMode))) return 0;
        delayMicroseconds(200);
        if (!writeBytes(start, sizeof(start))) return 0;
        delayMicroseconds(200);
        (void)clearInterrupt();
        return 0;
    }
    if (cpuRun && readLength == 0) {
        (void)clearInterrupt();
        return 0;
    }
    if (!pointExists && !gestureExists) {
        if (cpuRun && auxiliary) (void)clearInterrupt();
        return 0;
    }

    if (readLength < 4 || readLength > 64) {
        (void)clearInterrupt();
        return 0;
    }
    uint8_t report[64] = {};
    const uint8_t reportAddress[2] = {0x00, 0x03};
    if (!writeBytes(reportAddress, sizeof(reportAddress))) return 0;
    delayMicroseconds(200);
    if (!readBytes(report, readLength)) return 0;
    delayMicroseconds(200);

    uint8_t outputCount = 0;
    if (pointExists && readLength >= 10 && report[4] <= 0x0A) {
        uint8_t reportCount = static_cast<uint8_t>((readLength - 4U) / 6U);
        if (reportCount > maxPoints()) reportCount = maxPoints();
        for (uint8_t i = 0; i < reportCount && outputCount < maxPts; ++i) {
            const uint8_t offset = static_cast<uint8_t>(4U + i * 6U);
            uint16_t x = static_cast<uint16_t>(
                ((report[offset + 3U] & 0xF0U) << 4) |
                report[offset + 1U]);
            uint16_t y = static_cast<uint16_t>(
                ((report[offset + 3U] & 0x0FU) << 8) |
                report[offset + 2U]);
            const uint8_t weight = report[offset + 4U];
            if (weight == 0 || x >= _width || y >= _height) continue;
            rotate(x, y);
            TouchPoint& point = points[outputCount++];
            point.x = x;
            point.y = y;
            point.id = report[offset];
            point.strength = weight;
            point.pressed = true;
        }
    } else if (gestureExists && readLength >= 7 && report[4] == 0xF6) {
        _gesture = static_cast<uint8_t>(report[6] & 0x07U);
    }

    // Drain chained HDP packets before clearing TINT. Leaving a continuation
    // unread can make subsequent polls look permanently busy.
    const uint8_t hdpAddress[2] = {0xFC, 0x02};
    uint8_t hdp[8] = {};
    for (uint8_t packet = 0; packet < 4; ++packet) {
        if (!writeBytes(hdpAddress, sizeof(hdpAddress))) break;
        delayMicroseconds(200);
        if (!readBytes(hdp, sizeof(hdp))) break;
        delayMicroseconds(200);
        if (hdp[5] == 0x82) {
            (void)clearInterrupt();
            return outputCount;
        }
        if (hdp[5] != 0x00) break;
        const uint16_t nextLength =
            static_cast<uint16_t>(hdp[2] | (hdp[3] << 8));
        if (nextLength == 0 || nextLength > 64) break;
        uint8_t remainder[64] = {};
        if (!writeBytes(reportAddress, sizeof(reportAddress))) break;
        delayMicroseconds(200);
        if (!readBytes(remainder, nextLength)) break;
        delayMicroseconds(200);
    }
    // A malformed or excessively long chain must not leave TINT asserted.
    (void)clearInterrupt();
    return outputCount;
}

bool Touch_SPD2010::isPressed() {
    TouchPoint point;
    return read(point);
}
