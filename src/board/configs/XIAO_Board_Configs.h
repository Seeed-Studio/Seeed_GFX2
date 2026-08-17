/**
 * @file XIAO_Board_Configs.h
 * @brief MCU board pin maps and SPI limits for Seeed XIAO boards.
 *
 * LCD product wiring belongs in XIAO_LCD_Board_Configs.h.
 */

#ifndef SEEED_GFX_XIAO_BOARD_CONFIGS_H
#define SEEED_GFX_XIAO_BOARD_CONFIGS_H

#include <Arduino.h>
#include "BoardConfig.h"

struct XIAO_StandardDisplayPins {
    static BoardPinConfig make(int8_t rst = -1, int8_t touchIRQ = -1) {
        return BoardPinConfig(D1, D3, rst, D6, D10, D9, D8,
                              -1, touchIRQ);
    }
};

struct Config_XIAO_SAMD21_Board {
    static const char* name() { return "XIAO SAMD21"; }
    static BoardPinConfig pins() { return XIAO_StandardDisplayPins::make(); }
    static SpiBusConfig spi() { return SpiBusConfig(12000000, 4000000); }
};

struct Config_XIAO_MG24_Board {
    static const char* name() { return "XIAO MG24"; }
    static BoardPinConfig pins() { return XIAO_StandardDisplayPins::make(); }
    static SpiBusConfig spi() { return SpiBusConfig(25000000, 4000000); }
};

struct Config_XIAO_ESP32_Board {
    static const char* name() { return "XIAO ESP32"; }
    static BoardPinConfig pins() { return XIAO_StandardDisplayPins::make(); }
    static SpiBusConfig spi() { return SpiBusConfig(40000000, 12000000); }
};

struct Config_XIAO_ESP32C3_Board {
    static const char* name() { return "XIAO ESP32C3"; }
    static BoardPinConfig pins() { return XIAO_StandardDisplayPins::make(); }
    static SpiBusConfig spi() { return SpiBusConfig(40000000, 6000000); }
};

struct Config_XIAO_ESP32C5_Board {
    static const char* name() { return "XIAO ESP32C5"; }
    static BoardPinConfig pins() { return XIAO_StandardDisplayPins::make(); }
    static SpiBusConfig spi() { return SpiBusConfig(40000000, 6000000); }
};

struct Config_XIAO_ESP32C6_Board {
    static const char* name() { return "XIAO ESP32C6"; }
    static BoardPinConfig pins() { return XIAO_StandardDisplayPins::make(); }
    static SpiBusConfig spi() { return SpiBusConfig(40000000, 6000000); }
};

struct Config_XIAO_ESP32S2_Board {
    static const char* name() { return "XIAO ESP32S2"; }
    static BoardPinConfig pins() { return XIAO_StandardDisplayPins::make(); }
    static SpiBusConfig spi() { return SpiBusConfig(40000000, 12000000); }
};

struct Config_XIAO_ESP32S3_Board {
    static const char* name() { return "XIAO ESP32S3"; }
    static BoardPinConfig pins() { return XIAO_StandardDisplayPins::make(-1, D7); }
    static SpiBusConfig spi() {
        return SpiBusConfig(50000000, 12000000, 0, true);
    }
};

struct Config_XIAO_ESP32S3_Sense_Board {
    static const char* name() { return "XIAO ESP32S3 Sense"; }
    static BoardPinConfig pins() { return XIAO_StandardDisplayPins::make(-1, D7); }
    static SpiBusConfig spi() {
        return SpiBusConfig(50000000, 12000000, 0, true);
    }
};

struct Config_XIAO_ESP32S3_Plus_Board {
    static const char* name() { return "XIAO ESP32S3 Plus"; }
    static BoardPinConfig pins() { return XIAO_StandardDisplayPins::make(-1, D7); }
    static SpiBusConfig spi() {
        return SpiBusConfig(50000000, 12000000, 0, true);
    }
};

struct Config_XIAO_RP2040_Board {
    static const char* name() { return "XIAO RP2040"; }
    static BoardPinConfig pins() { return XIAO_StandardDisplayPins::make(); }
    static SpiBusConfig spi() { return SpiBusConfig(62500000, 12000000); }
};

struct Config_XIAO_RP2350_Board {
    static const char* name() { return "XIAO RP2350"; }
    static BoardPinConfig pins() { return XIAO_StandardDisplayPins::make(); }
    static SpiBusConfig spi() { return SpiBusConfig(62500000, 12000000); }
};

struct Config_XIAO_RA4M1_Board {
    static const char* name() { return "XIAO RA4M1"; }
    static BoardPinConfig pins() { return XIAO_StandardDisplayPins::make(); }
    static SpiBusConfig spi() { return SpiBusConfig(25000000, 4000000); }
};

struct Config_XIAO_nRF52840_Board {
    static const char* name() { return "XIAO nRF52840"; }
    static BoardPinConfig pins() { return XIAO_StandardDisplayPins::make(D0); }
    static SpiBusConfig spi() { return SpiBusConfig(12000000, 4000000); }
};

struct Config_XIAO_nRF52840_Sense_Board {
    static const char* name() { return "XIAO nRF52840 Sense"; }
    static BoardPinConfig pins() { return XIAO_StandardDisplayPins::make(D0); }
    static SpiBusConfig spi() { return SpiBusConfig(12000000, 4000000); }
};

struct Config_XIAO_nRF52840_Plus_Board {
    static const char* name() { return "XIAO nRF52840 Plus"; }
    static BoardPinConfig pins() { return XIAO_StandardDisplayPins::make(D0); }
    static SpiBusConfig spi() { return SpiBusConfig(12000000, 4000000); }
};

struct Config_XIAO_nRF52840_Sense_Plus_Board {
    static const char* name() { return "XIAO nRF52840 Sense Plus"; }
    static BoardPinConfig pins() { return XIAO_StandardDisplayPins::make(D0); }
    static SpiBusConfig spi() { return SpiBusConfig(12000000, 4000000); }
};

struct Config_XIAO_nRF54L15_Board {
    static const char* name() { return "XIAO nRF54L15"; }
    static BoardPinConfig pins() { return XIAO_StandardDisplayPins::make(D0); }
    static SpiBusConfig spi() { return SpiBusConfig(8000000, 4000000); }
};

struct Config_XIAO_nRF54LM20A_Board {
    static const char* name() { return "XIAO nRF54LM20A"; }
    static BoardPinConfig pins() { return XIAO_StandardDisplayPins::make(D0); }
    static SpiBusConfig spi() { return SpiBusConfig(8000000, 4000000); }
};

#endif // SEEED_GFX_XIAO_BOARD_CONFIGS_H
