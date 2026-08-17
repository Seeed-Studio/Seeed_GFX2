/**
 * @file   Bus_I2C.cpp
 * @brief  I2C bus implementation for Seeed_GFX v2.0
 */

#include "Bus_I2C.h"

Bus_I2C::Bus_I2C(uint8_t addr, TwoWire& wire)
    : _addr(addr), _wire(wire), _freq(400000), _lastError(0), _begun(false) {}

Bus_I2C::~Bus_I2C() { end(); }

bool Bus_I2C::begin() {
    if (_begun) return true;
    _wire.begin();
    _wire.setClock(_freq);
    _lastError = 0;
    _begun = true;
    return true;
}

void Bus_I2C::end() {
    if (!_begun) return;
    _wire.end();
    _begun = false;
}

void Bus_I2C::beginWrite() {
    // Each command/data method owns an I2C packet because it must prepend
    // the controller's command/data control byte.
}

void Bus_I2C::endWrite() {
}

void Bus_I2C::writeCommand(uint8_t cmd) {
    if (!_begun) { _lastError = -1; return; }
    // For I2C displays, commands are typically sent as control bytes
    // with bit 7 = 0 (Co=1, D/C=0) for command
    _wire.beginTransmission(_addr);
    if (_wire.write(0x00) != 1 || _wire.write(cmd) != 1) _lastError = 4;
    const uint8_t status = _wire.endTransmission();
    if (status != 0) _lastError = status;
}

void Bus_I2C::writeData(uint8_t data) {
    if (!_begun) { _lastError = -1; return; }
    _wire.beginTransmission(_addr);
    if (_wire.write(0x40) != 1 || _wire.write(data) != 1) _lastError = 4;
    const uint8_t status = _wire.endTransmission();
    if (status != 0) _lastError = status;
}

void Bus_I2C::writeData(const uint8_t* data, size_t len) {
    if (!_begun) { _lastError = -1; return; }
    if (!data && len != 0) { _lastError = -2; return; }
    while (len > 0) {
        const size_t chunk = len > maxTransferSize() ? maxTransferSize() : len;
        _wire.beginTransmission(_addr);
        if (_wire.write(0x40) != 1 || _wire.write(data, chunk) != chunk)
            _lastError = 4;
        const uint8_t status = _wire.endTransmission();
        if (status != 0) _lastError = status;
        data += chunk;
        len -= chunk;
    }
}

void Bus_I2C::beginRead() {
    if (!_begun) { _lastError = -1; return; }
    if (_wire.requestFrom(_addr, (uint8_t)1) != 1) _lastError = 5;
}

void Bus_I2C::endRead() {
    // No explicit end for I2C reads
}

uint8_t Bus_I2C::readData() {
    if (_wire.available()) {
        return _wire.read();
    }
    return 0;
}

void Bus_I2C::setFrequency(uint32_t freq) {
    _freq = freq;
    _wire.setClock(freq);
}
