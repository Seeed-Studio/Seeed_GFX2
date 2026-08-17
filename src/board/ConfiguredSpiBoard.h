/**
 * @file ConfiguredSpiBoard.h
 * @brief Reusable SPI board implementation driven by a data-only config.
 */

#ifndef SEEED_GFX_CONFIGURED_SPI_BOARD_H
#define SEEED_GFX_CONFIGURED_SPI_BOARD_H

#include <Arduino.h>
#include "../core/Board.h"
#include "../core/Gpio.h"
#include "../bus/Bus_SPI.h"
#include <new>
#include "configs/BoardConfig.h"

template <typename Config>
class ConfiguredSpiBoard : public IBoard {
public:
    using Configuration = Config;

    static SpiBusConfig spiConfig() { return Config::spi(); }

    static void configureBus(Bus_SPI& bus) {
        const SpiBusConfig spi = Config::spi();
        bus.setReadFrequency(spi.readFrequency);
        bus.setSPIMode(spi.mode);
        bus.useHSPI(spi.useSecondaryHost);
    }

    const char* name() const override { return Config::name(); }

    bool begin() override {
        const BoardPinConfig pins = Config::pins();

        if (pins.rst >= 0) {
            gfxPinModeOutput(pins.rst);
            gfxDigitalWrite(pins.rst, true);
        }
        if (pins.backlight >= 0) {
            gfxPinModeOutput(pins.backlight);
            gfxDigitalWrite(pins.backlight, true);
        }
        if (pins.busy >= 0) {
            gfxPinModeInput(pins.busy);
        }
        if (pins.enable >= 0) {
            gfxPinModeOutput(pins.enable);
            gfxDigitalWrite(pins.enable, pins.enableActiveHigh);
        }
        if (pins.auxiliaryEnable >= 0) {
            gfxPinModeOutput(pins.auxiliaryEnable);
            gfxDigitalWrite(pins.auxiliaryEnable,
                            pins.auxiliaryEnableActiveHigh);
        }
        return true;
    }

    int8_t pinCS() const override { return Config::pins().cs; }
    int8_t pinCS2() const override { return Config::pins().cs2; }
    int8_t pinDC() const override { return Config::pins().dc; }
    int8_t pinRST() const override { return Config::pins().rst; }
    int8_t pinBacklight() const override { return Config::pins().backlight; }
    int8_t pinMOSI() const override { return Config::pins().mosi; }
    int8_t pinMISO() const override { return Config::pins().miso; }
    int8_t pinSCLK() const override { return Config::pins().sclk; }
    int8_t pinTouchCS() const override { return Config::pins().touchCS; }
    int8_t pinTouchIRQ() const override { return Config::pins().touchIRQ; }
    int8_t busyPin() const override { return Config::pins().busy; }
    int8_t enablePin() const override { return Config::pins().enable; }
    bool panelHorizontalMirror() const override {
        return Config::pins().horizontalMirror;
    }

    IBus* createBus() override {
        const BoardPinConfig pins = Config::pins();
        const SpiBusConfig spi = Config::spi();
        Bus_SPI* bus = new (std::nothrow) Bus_SPI(pins.cs, pins.dc, pins.rst,
                                   pins.mosi, pins.miso, pins.sclk,
                                   spi.writeFrequency, pins.cs2);
        if (!bus) return nullptr;
        configureBus(*bus);
        return bus;
    }

    void setBacklight(uint8_t brightness) override {
        const int8_t pin = pinBacklight();
        if (pin >= 0) {
            analogWrite(pin, brightness);
        }
    }

    void powerOn() override {
        const BoardPinConfig pins = Config::pins();
        if (pins.enable >= 0) {
            gfxDigitalWrite(pins.enable, pins.enableActiveHigh);
        }
        if (pins.auxiliaryEnable >= 0) {
            gfxDigitalWrite(pins.auxiliaryEnable,
                            pins.auxiliaryEnableActiveHigh);
        }
    }

    void powerOff() override {
        const BoardPinConfig pins = Config::pins();
        if (pins.enable >= 0) {
            gfxDigitalWrite(pins.enable, !pins.enableActiveHigh);
        }
        if (pins.auxiliaryEnable >= 0) {
            gfxDigitalWrite(pins.auxiliaryEnable,
                            !pins.auxiliaryEnableActiveHigh);
        }
    }
};

#endif // SEEED_GFX_CONFIGURED_SPI_BOARD_H
