/**
 * @file reTerminal_ePaper_Board_Configs.h
 * @brief Pin and SPI configurations for reTerminal ePaper products.
 */

#ifndef SEEED_GFX_RETERMINAL_EPAPER_BOARD_CONFIGS_H
#define SEEED_GFX_RETERMINAL_EPAPER_BOARD_CONFIGS_H

#include "BoardConfig.h"

struct reTerminal_ePaper_CommonConfig {
    // The official Setup520-Setup525 profiles select USE_HSPI_PORT on the
    // ESP32-S3. Keep the display and the on-board microSD slot on that same
    // peripheral because they share SCK/MOSI (and MISO where applicable).
    static SpiBusConfig spi() {
        return SpiBusConfig(10000000, 4000000, 0, true);
    }

    static constexpr int8_t sdChipSelectPin() { return 14; }
    static constexpr int8_t sdDetectPin() { return 15; }
    static constexpr int8_t sdEnablePin() { return 16; }

    // E1001/E1002 V1.2 schematics: GPIO10=SCREEN_CS#, GPIO11=SCREEN_DC#,
    // GPIO12=SCREEN_RST#, GPIO13=SCREEN_BUSY#, GPIO9=MOSI, GPIO8=MISO,
    // GPIO7=SCK. The panel is write-only, but the shared microSD needs MISO.
    static BoardPinConfig e1001Pins() {
        return BoardPinConfig(10, 11, 12, -1, 9, 8, 7,
                              -1, -1, 13);
    }

    // Sticky shares the microSD SPI bus: SCK=13, MOSI=14, and MISO=12 is the
    // read-back line. The panel itself is normally write-only, but the
    // production unit ships with either an SSD1677 or an SSD2677 (random per
    // unit); the firmware-style auto-detect probe (reset -> 0x70 -> read one
    // byte -> 0x07 means SSD2677) needs this MISO, as does the SSD2677
    // register temperature read.
    static BoardPinConfig stickyPins(bool horizontalMirror) {
        return BoardPinConfig(15, 16, 17, -1, 14, 12, 13,
                              -1, -1, 18, 47, -1, -1,
                              horizontalMirror);
    }
};

struct Config_reTerminal_E1001_Board : reTerminal_ePaper_CommonConfig {
    static const char* name() { return "reTerminal E1001"; }
    static BoardPinConfig pins() { return e1001Pins(); }
};

struct Config_reTerminal_E1002_Board : reTerminal_ePaper_CommonConfig {
    static const char* name() { return "reTerminal E1002"; }
    static BoardPinConfig pins() { return e1001Pins(); }
};

struct Config_reTerminal_E1003_Board : reTerminal_ePaper_CommonConfig {
    static const char* name() { return "reTerminal E1003"; }
    static constexpr int8_t sdEnablePin() { return 39; }

    static BoardPinConfig pins() {
        // E1003 V1.0: GPIO10=ITE_CS#, GPIO12=ITE_RST#, GPIO13=ITE_BUSY#,
        // GPIO9/8/7=ITE_SD_MOSI/MISO/SCK, GPIO11=EPD_Drive_EN and
        // GPIO21=ITE_VCC_EN. DC is not part of the IT8951/TCON protocol.
        return BoardPinConfig(10, -1, 12, -1, 9, 8, 7,
                              -1, -1, 13, 11, -1, 21);
    }
};

struct Config_reTerminal_E1004_Board : reTerminal_ePaper_CommonConfig {
    static const char* name() { return "reTerminal E1004"; }

    static BoardPinConfig pins() {
        // E1004 V1.0: GPIO10=SCREEN_CS#, GPIO2=SPI_CS_S, GPIO11=SCREEN_DC#,
        // GPIO38=SCREEN_RST#, GPIO9/8/7=MOSI/MISO/SCK and GPIO13=SCREEN_BUSY#.
        // GPIO12 is labelled SCREEN_EN# at the ESP32 net, but the schematic
        // connects it through R52 directly to the active-high EN input of
        // U10 (TPS22916), with R53 pulling EN low. HIGH therefore enables
        // EPD_3V3, matching the original Setup523/T133A01 initialization.
        return BoardPinConfig(10, 11, 38, -1, 9, 8, 7,
                              -1, -1, 13, 12, 2, -1, false, true);
    }
};

struct Config_reTerminal_Sticky_Board : reTerminal_ePaper_CommonConfig {
    static const char* name() { return "reTerminal Sticky"; }
    static BoardPinConfig pins() { return stickyPins(true); }
    // The inherited sdChipSelectPin/sdDetectPin/sdEnablePin (GPIO14/15/16)
    // belong to the E1001-generation microSD slot; on the Sticky those GPIOs
    // are the display MOSI/CS/DC. The Sticky microSD CS is GPIO8, and the
    // card shares MISO=GPIO12 with the panel, so the board layer must
    // deselect it before the auto-detect probe reads over that line.
    static constexpr int8_t sdChipSelectPin() { return 8; }
    static constexpr int8_t sdDetectPin() { return -1; }
    static constexpr int8_t sdEnablePin() { return -1; }
};

#endif // SEEED_GFX_RETERMINAL_EPAPER_BOARD_CONFIGS_H
