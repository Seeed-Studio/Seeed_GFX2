#include "IoExpander9535.h"

namespace {
constexpr uint8_t kInputPort = 0x00;
constexpr uint8_t kOutputPort = 0x02;
constexpr uint8_t kConfiguration = 0x06;
}

bool IoExpander9535::probe() const {
    _wire.beginTransmission(_address);
    return _wire.endTransmission() == 0;
}

bool IoExpander9535::read16(uint8_t reg, uint16_t& value) const {
    _wire.beginTransmission(_address);
    _wire.write(reg);
    if (_wire.endTransmission(false) != 0) return false;
    if (_wire.requestFrom(_address, static_cast<uint8_t>(2)) != 2) return false;
    const uint8_t low = static_cast<uint8_t>(_wire.read());
    const uint8_t high = static_cast<uint8_t>(_wire.read());
    value = static_cast<uint16_t>(low | (static_cast<uint16_t>(high) << 8));
    return true;
}

bool IoExpander9535::write16(uint8_t reg, uint16_t value) {
    _wire.beginTransmission(_address);
    _wire.write(reg);
    _wire.write(static_cast<uint8_t>(value & 0xFF));
    _wire.write(static_cast<uint8_t>(value >> 8));
    return _wire.endTransmission() == 0;
}

bool IoExpander9535::begin() {
    if (!probe()) return false;
    if (!read16(kOutputPort, _output)) return false;
    if (!read16(kConfiguration, _configuration)) return false;
    _initialized = true;
    return true;
}

bool IoExpander9535::pinModeOutput(uint8_t pin, bool initialLevel) {
    if (!_initialized || pin >= 16) return false;
    const uint16_t bit = static_cast<uint16_t>(1U << pin);
    const uint16_t nextOutput =
        initialLevel ? static_cast<uint16_t>(_output | bit)
                     : static_cast<uint16_t>(_output & ~bit);
    // Set the output latch before enabling the driver to avoid a low pulse.
    if (nextOutput != _output && !write16(kOutputPort, nextOutput)) return false;
    _output = nextOutput;
    const uint16_t nextConfig = static_cast<uint16_t>(_configuration & ~bit);
    if (nextConfig != _configuration && !write16(kConfiguration, nextConfig)) {
        return false;
    }
    _configuration = nextConfig;
    return true;
}

bool IoExpander9535::pinModeInput(uint8_t pin) {
    if (!_initialized || pin >= 16) return false;
    const uint16_t next =
        static_cast<uint16_t>(_configuration | static_cast<uint16_t>(1U << pin));
    if (next != _configuration && !write16(kConfiguration, next)) return false;
    _configuration = next;
    return true;
}

bool IoExpander9535::writePin(uint8_t pin, bool level) {
    if (!_initialized || pin >= 16) return false;
    return writeMask(static_cast<uint16_t>(1U << pin), level);
}

bool IoExpander9535::writeMask(uint16_t mask, bool level) {
    if (!_initialized) return false;
    const uint16_t next =
        level ? static_cast<uint16_t>(_output | mask)
              : static_cast<uint16_t>(_output & ~mask);
    if (next == _output) return true;
    if (!write16(kOutputPort, next)) return false;
    _output = next;
    return true;
}

bool IoExpander9535::readInputs(uint16_t& value) {
    if (!_initialized) return false;
    return read16(kInputPort, value);
}
