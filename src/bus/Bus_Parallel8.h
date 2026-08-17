/**
 * @file   Bus_Parallel8.h
 * @brief  Portable 8-bit parallel bus implementation for Seeed_GFX v2.0
 *
 * Implements IBus using 8-bit parallel interface.
 * Primarily used for high-performance TFT displays on ESP32 and RP2040.
 */

#ifndef SEEED_GFX_BUS_PARALLEL8_H
#define SEEED_GFX_BUS_PARALLEL8_H

#include <Arduino.h>
#include "core/Bus.h"

class Bus_Parallel8 : public IBus {
public:
    Bus_Parallel8(int8_t cs, int8_t dc, int8_t wr, int8_t rd,
                  int8_t d0, int8_t d1, int8_t d2, int8_t d3,
                  int8_t d4, int8_t d5, int8_t d6, int8_t d7);
    virtual ~Bus_Parallel8();

    bool isParallel() const override { return true; }

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
    BusCapabilities capabilities() const override {
        BusCapabilities caps;
        caps.readable = _rd >= 0;
        caps.parallel = true;
        caps.maxTransferBytes = maxTransferSize();
        return caps;
    }

private:
    int8_t _cs, _dc, _wr, _rd;
    int8_t _d0, _d1, _d2, _d3, _d4, _d5, _d6, _d7;
    uint32_t _freq;
    bool _writing = false;

    void setDataDirection(uint8_t mode);
    void writeByte(uint8_t value);
};

#endif // SEEED_GFX_BUS_PARALLEL8_H
