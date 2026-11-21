/**
 * @file   XIAO_Console_CAN_Configs.h
 * @brief  Panel configuration for the XIAO ESP32-C5 "console for CAN" carrier
 *
 * ST7789V 240x320 4-wire SPI TFT on the CAN console board.  Same controller as
 * the Seeed 1.14" IPS panels, just a different resolution and portrait origin.
 *
 * Values mirror the ESP-IDF product `can_console_display.c` config defaults:
 *   color_order_bgr = false  -> rgbOrder 0x00 (RGB)
 *   invert_colors   = true   -> invert = true  (INVON)
 * The panel reports in portrait 240x320 with no source/gate offset, so no
 * initialRotation is set (the driver's 240x320 path uses colstart/rowstart 0).
 */

#ifndef SEEED_GFX_XIAO_CONSOLE_CAN_CONFIGS_H
#define SEEED_GFX_XIAO_CONSOLE_CAN_CONFIGS_H

#include <stdint.h>
#include "../../core/Panel.h"

class Driver_ST7789;

/** ST7789 240x320 SPI TFT on the XIAO ESP32-C5 console-for-CAN carrier. */
struct Config_XIAO_Console_CAN {
    using Driver = Driver_ST7789;
    using Panel  = Panel_TFT;
    static constexpr uint16_t width      = 240;
    static constexpr uint16_t height     = 320;
    static constexpr uint8_t  colorDepth = 16;
    static constexpr uint8_t  rgbOrder   = 0x00;  // ST7789 RGB (bsp BGR=false)
    static constexpr bool     invert     = true;  // INVON (bsp invert=true)
};

#endif // SEEED_GFX_XIAO_CONSOLE_CAN_CONFIGS_H