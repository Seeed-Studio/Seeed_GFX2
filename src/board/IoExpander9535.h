/**
 * @file IoExpander9535.h
 * @brief Small PCA9535/TCA9535-compatible 16-bit GPIO expander driver.
 *
 * SenseCAP Watcher uses a PCA9535 and SenseCAP Indicator uses a TCA9535.
 * Both devices share the same register layout, so keeping the implementation
 * here avoids pulling either product firmware's BSP into Seeed_GFX.
 */

#ifndef SEEED_GFX_IO_EXPANDER_9535_H
#define SEEED_GFX_IO_EXPANDER_9535_H

#include <Arduino.h>
#include <Wire.h>
#include <stdint.h>

class IoExpander9535 {
public:
    IoExpander9535(TwoWire& wire, uint8_t address)
        : _wire(wire), _address(address), _output(0), _configuration(0xFFFF),
          _initialized(false) {}

    bool begin();
    bool probe() const;
    bool pinModeOutput(uint8_t pin, bool initialLevel = false);
    bool pinModeInput(uint8_t pin);
    bool writePin(uint8_t pin, bool level);
    bool writeMask(uint16_t mask, bool level);
    bool readInputs(uint16_t& value);

    uint8_t address() const { return _address; }
    uint16_t outputState() const { return _output; }
    uint16_t configurationState() const { return _configuration; }

private:
    bool read16(uint8_t reg, uint16_t& value) const;
    bool write16(uint8_t reg, uint16_t value);

    TwoWire& _wire;
    uint8_t _address;
    uint16_t _output;
    uint16_t _configuration;
    bool _initialized;
};

#endif
