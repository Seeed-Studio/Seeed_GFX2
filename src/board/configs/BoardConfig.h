/**
 * @file BoardConfig.h
 * @brief Data-only configuration types used by configured board adapters.
 */

#ifndef SEEED_GFX_BOARD_CONFIG_H
#define SEEED_GFX_BOARD_CONFIG_H

#include <stdint.h>

struct BoardPinConfig {
    int8_t cs;
    int8_t dc;
    int8_t rst;
    int8_t backlight;
    int8_t mosi;
    int8_t miso;
    int8_t sclk;
    int8_t touchCS;
    int8_t touchIRQ;
    int8_t busy;
    int8_t enable;
    int8_t cs2;
    int8_t auxiliaryEnable;
    bool horizontalMirror;
    bool enableActiveHigh;
    bool auxiliaryEnableActiveHigh;

    constexpr BoardPinConfig(int8_t csPin, int8_t dcPin, int8_t rstPin,
                             int8_t backlightPin, int8_t mosiPin,
                             int8_t misoPin, int8_t sclkPin,
                             int8_t touchCsPin = -1, int8_t touchIrqPin = -1,
                             int8_t busyPin = -1, int8_t enablePin = -1,
                             int8_t cs2Pin = -1,
                             int8_t auxiliaryEnablePin = -1,
                             bool mirrorHorizontally = false,
                             bool enableIsActiveHigh = true,
                             bool auxiliaryEnableIsActiveHigh = true)
        : cs(csPin), dc(dcPin), rst(rstPin), backlight(backlightPin),
          mosi(mosiPin), miso(misoPin), sclk(sclkPin),
          touchCS(touchCsPin), touchIRQ(touchIrqPin),
          busy(busyPin), enable(enablePin), cs2(cs2Pin),
          auxiliaryEnable(auxiliaryEnablePin),
          horizontalMirror(mirrorHorizontally),
          enableActiveHigh(enableIsActiveHigh),
          auxiliaryEnableActiveHigh(auxiliaryEnableIsActiveHigh) {}
};

struct SpiBusConfig {
    uint32_t writeFrequency;
    uint32_t readFrequency;
    uint8_t mode;
    bool useSecondaryHost;

    constexpr SpiBusConfig(uint32_t writeHz, uint32_t readHz,
                           uint8_t spiMode = 0,
                           bool secondaryHost = false)
        : writeFrequency(writeHz), readFrequency(readHz), mode(spiMode),
          useSecondaryHost(secondaryHost) {}
};

#endif // SEEED_GFX_BOARD_CONFIG_H
