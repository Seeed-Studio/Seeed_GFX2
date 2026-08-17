/**
 * @file   Board.h
 * @brief  Board base class and generic custom board for Seeed_GFX v2.0
 *
 * Provides the Board_Custom class for user-defined boards,
 * and serves as the header that includes the IBoard interface.
 */

#ifndef SEEED_GFX_BOARD_BASE_H
#define SEEED_GFX_BOARD_BASE_H

#include <Arduino.h>
#include "../core/Board.h"
#include "../core/Gpio.h"
#include "../bus/Bus_SPI.h"
#include <new>

/**
 * Board_Custom - A generic board class that takes all pin assignments
 * as constructor parameters. Used for user-defined/custom boards.
 */
class Board_Custom : public IBoard {
public:
    Board_Custom(const char* boardName,
                 int8_t cs, int8_t dc, int8_t rst,
                 int8_t mosi, int8_t miso, int8_t sclk,
                 int8_t bl = -1, uint32_t spiFreq = 40000000,
                 int8_t busy = -1, int8_t enable = -1,
                 uint32_t spiReadFreq = 20000000,
                 int8_t cs2 = -1, int8_t auxiliaryEnable = -1,
                 bool enableActiveHigh = true,
                 bool auxiliaryEnableActiveHigh = true)
        : _name(boardName), _cs(cs), _dc(dc), _rst(rst)
        , _mosi(mosi), _miso(miso), _sclk(sclk)
        , _bl(bl), _busy(busy), _enable(enable)
        , _spiFreq(spiFreq), _spiReadFreq(spiReadFreq)
        , _cs2(cs2), _auxiliaryEnable(auxiliaryEnable)
        , _enableActiveHigh(enableActiveHigh)
        , _auxiliaryEnableActiveHigh(auxiliaryEnableActiveHigh) {}

    const char* name() const override { return _name; }

    bool begin() override {
        if (_rst >= 0) {
            gfxPinModeOutput(_rst);
            gfxDigitalWrite(_rst, true);
        }
        if (_bl >= 0) {
            gfxPinModeOutput(_bl);
            gfxDigitalWrite(_bl, true);
        }
        if (_busy >= 0) {
            gfxPinModeInput(_busy);
        }
        if (_enable >= 0) {
            gfxPinModeOutput(_enable);
            gfxDigitalWrite(_enable, _enableActiveHigh);
        }
        if (_auxiliaryEnable >= 0) {
            gfxPinModeOutput(_auxiliaryEnable);
            gfxDigitalWrite(_auxiliaryEnable, _auxiliaryEnableActiveHigh);
        }
        return true;
    }

    int8_t pinCS()  const override { return _cs; }
    int8_t pinCS2() const override { return _cs2; }
    int8_t pinDC()  const override { return _dc; }
    int8_t pinRST() const override { return _rst; }
    int8_t pinBacklight() const override { return _bl; }
    int8_t pinMOSI() const override { return _mosi; }
    int8_t pinMISO() const override { return _miso; }
    int8_t pinSCLK() const override { return _sclk; }
    int8_t busyPin() const override { return _busy; }
    int8_t enablePin() const override { return _enable; }

    IBus* createBus() override {
        Bus_SPI* bus = new (std::nothrow) Bus_SPI(_cs, _dc, _rst, _mosi, _miso, _sclk,
                                   _spiFreq, _cs2);
        if (!bus) return nullptr;
        bus->setReadFrequency(_spiReadFreq);
        return bus;
    }

    void setBacklight(uint8_t brightness) override {
        if (_bl >= 0) {
            analogWrite(_bl, brightness);
        }
    }

    void powerOn() override {
        if (_enable >= 0) gfxDigitalWrite(_enable, _enableActiveHigh);
        if (_auxiliaryEnable >= 0) {
            gfxDigitalWrite(_auxiliaryEnable, _auxiliaryEnableActiveHigh);
        }
    }

    void powerOff() override {
        if (_enable >= 0) gfxDigitalWrite(_enable, !_enableActiveHigh);
        if (_auxiliaryEnable >= 0) {
            gfxDigitalWrite(_auxiliaryEnable, !_auxiliaryEnableActiveHigh);
        }
    }

private:
    const char* _name;
    int8_t _cs, _dc, _rst;
    int8_t _mosi, _miso, _sclk;
    int8_t _bl;
    int8_t _busy, _enable;
    uint32_t _spiFreq;
    uint32_t _spiReadFreq;
    int8_t _cs2;
    int8_t _auxiliaryEnable;
    bool _enableActiveHigh;
    bool _auxiliaryEnableActiveHigh;
};

#endif // SEEED_GFX_BOARD_BASE_H
