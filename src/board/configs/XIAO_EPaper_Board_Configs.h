/**
 * @file XIAO_EPaper_Board_Configs.h
 * @brief Pin and SPI configurations for XIAO ePaper adapter boards.
 */

#ifndef SEEED_GFX_XIAO_EPAPER_BOARD_CONFIGS_H
#define SEEED_GFX_XIAO_EPAPER_BOARD_CONFIGS_H

#include <Arduino.h>
#include "BoardConfig.h"

struct XIAO_EPaper_CommonConfig {
    static SpiBusConfig spi() {
#if defined(CONFIG_IDF_TARGET_ESP32S3)
        // Keep the ESP32-S3 path aligned with the hardware-verified
        // Seeed_GFX ePaper setups (Setup504..Setup517), which select HSPI.
        return SpiBusConfig(10000000, 4000000, 0, true);
#else
        return SpiBusConfig(10000000, 4000000);
#endif
    }

    static BoardPinConfig driverPins(int8_t busy) {
        return BoardPinConfig(D1, D3, D0, -1, D10, D9, D8,
                              -1, -1, busy, -1);
    }

    // EE boards are ESP32-S3 products. Their control nets use the physical
    // GPIO numbers from the Seeed_GFX setup files because D11/D16 aliases
    // differ between XIAO ESP32-S3 board-package variants.
    static BoardPinConfig esp32DisplayPins(int8_t dc, int8_t miso,
                                           int8_t cs2 = -1) {
        return BoardPinConfig(44, dc, 38, -1, D10, miso, D8,
                              -1, -1, 4, 43, cs2);
    }

   // EN boards use XIAO nRF52840 Plus. D16/D11 are raw GPIO 35/30 because the
   // nRF52 core does not expose Dxx as preprocessor macros.
    static BoardPinConfig nrfDisplayPins() {
        return BoardPinConfig(D7, 35, 30, -1, D10, -1, D8,
                              -1, -1, D3, D6);
    }
};

struct Config_XIAO_EPaper_Driver_Board : XIAO_EPaper_CommonConfig {
    static const char* name() { return "XIAO ePaper Driver Board"; }
    static BoardPinConfig pins() { return driverPins(D2); }
};

struct Config_XIAO_EPaper_Breakout_Board : XIAO_EPaper_CommonConfig {
    static const char* name() { return "XIAO ePaper Breakout Board"; }
    static BoardPinConfig pins() { return driverPins(D5); }
};

struct Config_XIAO_EPaper_EE02_Board : XIAO_EPaper_CommonConfig {
    static const char* name() { return "XIAO ePaper EE02"; }
    // EE02 exposes the second chip select required by the T133A01 panel.
    static BoardPinConfig pins() { return esp32DisplayPins(10, -1, 41); }
};

struct Config_XIAO_EPaper_EE03_Board : XIAO_EPaper_CommonConfig {
    static const char* name() { return "XIAO ePaper EE03"; }
    static BoardPinConfig pins() { return esp32DisplayPins(-1, D9); }
};

struct Config_XIAO_EPaper_EE04_Board : XIAO_EPaper_CommonConfig {
    static const char* name() { return "XIAO ePaper EE04"; }
    static BoardPinConfig pins() { return esp32DisplayPins(10, -1); }
};

struct Config_XIAO_EPaper_EE05_Board : XIAO_EPaper_CommonConfig {
    static const char* name() { return "XIAO ePaper EE05"; }
    static BoardPinConfig pins() { return esp32DisplayPins(10, -1); }
};

struct Config_XIAO_EPaper_EN04_Board : XIAO_EPaper_CommonConfig {
    static const char* name() { return "XIAO ePaper EN04"; }
    static BoardPinConfig pins() { return nrfDisplayPins(); }
};

struct Config_XIAO_EPaper_EN05_Board : XIAO_EPaper_CommonConfig {
    static const char* name() { return "XIAO ePaper EN05"; }
    static BoardPinConfig pins() { return nrfDisplayPins(); }
};

#endif // SEEED_GFX_XIAO_EPAPER_BOARD_CONFIGS_H
