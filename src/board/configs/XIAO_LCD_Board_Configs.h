/**
 * @file XIAO_LCD_Board_Configs.h
 * @brief Pin maps and target-dependent SPI limits for XIAO LCD products.
 *
 * Panel controller, dimensions and color behavior remain in
 * panel/configs/Seeed_Panel_Configs.h. This file only describes board wiring.
 */

#ifndef SEEED_GFX_XIAO_LCD_BOARD_CONFIGS_H
#define SEEED_GFX_XIAO_LCD_BOARD_CONFIGS_H

#include <Arduino.h>
#include "BoardConfig.h"

/** Reliable TFT SPI rates from Seeed_GFX XIAO_SPI_Frequency.h. */
inline SpiBusConfig xiaoLcdSpiConfig() {
#if defined(ARDUINO_ARCH_NRF52840) || defined(ARDUINO_ARCH_NRF52) || \
    defined(ARDUINO_ARCH_SAMD) || defined(SEEED_XIAO_M0) || \
    defined(ARDUINO_SEEED_XIAO_M0_PLUS) || \
    defined(ARDUINO_SEEED_XIAO_NRF52840) || \
    defined(ARDUINO_Seeed_XIAO_nRF52840) || \
    defined(ARDUINO_Seeed_XIAO_nRF52840_Plus)
    return SpiBusConfig(12000000, 4000000);
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
    return SpiBusConfig(50000000, 12000000, 0, true);
#elif defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C5) || \
      defined(CONFIG_IDF_TARGET_ESP32C6)
    return SpiBusConfig(40000000, 6000000);
#elif defined(ARDUINO_ARCH_ESP32) || defined(ESP32)
    return SpiBusConfig(40000000, 12000000);
#elif defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_RP2350)
    return SpiBusConfig(62500000, 12000000);
#elif defined(ARDUINO_ARCH_RENESAS) || defined(ARDUINO_ARCH_RENESAS_UNO) || \
      defined(ARDUINO_ARCH_SILABS)
    return SpiBusConfig(25000000, 4000000);
#else
    return SpiBusConfig(25000000, 4000000);
#endif
}

/**
 * Conservative bus rate for the ST7789 80x160 panel on the XIAO 0.96" board.
 *
 * The downloaded, hardware-verified example drives this panel with software
 * SPI.  Keeping the new hardware-SPI path at 10 MHz avoids the signal-margin
 * failure seen at the shared 40/50/62.5 MHz rates while remaining much faster
 * than the reference bit-banged implementation.
 */
inline SpiBusConfig xiao096LcdSpiConfig() {
#if defined(CONFIG_IDF_TARGET_ESP32S3)
    return SpiBusConfig(10000000, 4000000, 0, true);
#else
    return SpiBusConfig(10000000, 4000000);
#endif
}

template <int8_t RST, int8_t BL>
struct Config_XIAO_0inch96_LCD_Board {
    static const char* name() { return "XIAO 0.96 inch LCD Board"; }
    static BoardPinConfig pins() {
        return BoardPinConfig(D2, D3, RST, BL, D10, -1, D8);
    }
    static SpiBusConfig spi() { return xiao096LcdSpiConfig(); }
};

template <int8_t RST, int8_t BL>
struct Config_XIAO_1inch14_LCD_Board {
    static const char* name() { return "XIAO 1.14 inch LCD Board"; }
    static BoardPinConfig pins() {
        return BoardPinConfig(D2, D3, RST, BL, D10, -1, D8);
    }
    static SpiBusConfig spi() { return xiaoLcdSpiConfig(); }
};

struct Config_Seeed_1inch47_LCD_Board {
    static const char* name() { return "1.47 inch LCD SPI Display"; }
    static BoardPinConfig pins() {
        return BoardPinConfig(D1, D3, D0, D6, D10, -1, D8);
    }
    static SpiBusConfig spi() { return xiaoLcdSpiConfig(); }
};

template <int8_t RST, int8_t BL, int8_t IRQ = D7>
struct Config_XIAO_1inch47_Touch_LCD_Board {
    static const char* name() { return "XIAO 1.47 inch Touch Display"; }
    static BoardPinConfig pins() {
        return BoardPinConfig(D2, D3, RST, BL, D10, -1, D8, -1, IRQ);
    }
    static SpiBusConfig spi() { return xiaoLcdSpiConfig(); }
};

struct Config_Seeed_1inch69_LCD_Board {
    static const char* name() { return "1.69 inch LCD Display"; }
    static BoardPinConfig pins() {
        return BoardPinConfig(D1, D3, D0, D6, D10, -1, D8);
    }
    static SpiBusConfig spi() { return xiaoLcdSpiConfig(); }
};

struct Config_XIAO_ILI9341_LCD_Board {
    static const char* name() { return "XIAO ILI9341 LCD"; }
    static BoardPinConfig pins() {
        return BoardPinConfig(D1, D3, D0, -1, D10, D9, D8);
    }
    static SpiBusConfig spi() { return xiaoLcdSpiConfig(); }
};

#endif // SEEED_GFX_XIAO_LCD_BOARD_CONFIGS_H
