/**
 * @file   Bus_I2C.h
 * @brief  I2C bus implementation for Seeed_GFX v2.0
 *
 * Implements IBus using Arduino's Wire (I2C) library.
 * Primarily used for OLED displays (SSD1306, SH1107 etc.)
 * and I2C touch controllers.
 */

#ifndef SEEED_GFX_BUS_I2C_H
#define SEEED_GFX_BUS_I2C_H

#include <Arduino.h>
#include <Wire.h>
#include "core/Bus.h"

class Bus_I2C : public IBus {
public:
    Bus_I2C(uint8_t addr, TwoWire& wire = Wire);
    virtual ~Bus_I2C();

    bool begin() override;
    void end() override;

    void beginWrite() override;
    void endWrite() override;

    void writeCommand(uint8_t cmd) override;
    void writeData(uint8_t data) override;
    void writeData(const uint8_t* data, size_t len) override;

    void beginRead() override;
    void endRead() override;
    uint8_t readData() override;

    void setFrequency(uint32_t freq) override;
    uint32_t frequency() const override { return _freq; }
    int lastError() const override { return _lastError; }
    void clearError() { _lastError = 0; }
    size_t maxTransferSize() const override { return 30; }
    BusCapabilities capabilities() const override {
        BusCapabilities caps;
        caps.readable = true;
        caps.maxTransferBytes = maxTransferSize();
        return caps;
    }

private:
    uint8_t   _addr;
    TwoWire&  _wire;
    uint32_t  _freq;
    int       _lastError;
    bool      _begun;
};

#endif // SEEED_GFX_BUS_I2C_H
