/**
 * @file   XIAO_AMOLED_Configs.h
 * @brief  Panel configuration for the XIAO ESP32-S3-PLUS-AMOLED
 *
 * Product: XIAO ESP32-S3-PLUS-AMOLED (Waveshare ESP32-S3-Touch-AMOLED-2.16
 *          board, CO5300 QSPI AMOLED, 480x480, RGB565)
 * CO5300 (Espressif esp_lcd_co5300 兼容, QSPI 接口)
 * 2.16", 480x480 , 16-bit RGB565
 * Board_XIAO_AMOLED (PCLK=38 DATA0..3=4/5/6/7 CS=12 RST=39, SPI mode 0)
 */

#ifndef SEEED_GFX_XIAO_AMOLED_CONFIGS_H
#define SEEED_GFX_XIAO_AMOLED_CONFIGS_H

#include <stdint.h>
#include "../../core/Panel.h"

class Driver_CO5300;

/**  CO5300 2.16" 480x480 AMOLED on the XIAO form factor. */
struct Config_XIAO_AMOLED {
    using Driver = Driver_CO5300;
    using Panel  = Panel_TFT;
    static constexpr uint16_t width      = 480;
    static constexpr uint16_t height     = 480;
    static constexpr uint8_t  colorDepth = 16;
};

/**
 *  CO5300 2.05" 368x448 round AMOLED (H0205S003AMT003A) on the same XIAO
 *  carrier.  Driver_CO5300 derives the per-glass profile from these dimensions:
 *  factory MADCTL 0x00, source GRAM gap +16 (CASET 16..383) and the extended
 *  source/gate-map init.
 */
struct Config_XIAO_AMOLED_205 {
    using Driver = Driver_CO5300;
    using Panel  = Panel_TFT;
    static constexpr uint16_t width      = 368;
    static constexpr uint16_t height     = 448;
    static constexpr uint8_t  colorDepth = 16;
};

/**
 *  CO5300 1.75" 466x466 round AMOLED (H0175Y003AMT003V1) on the same XIAO
 *  carrier.  Same QSPI interface and power rails (VBAT/VCI/IOVCC) as the
 *  2.16"; Driver_CO5300 derives factory MADCTL 0x00 and source GRAM gap +6
 *  (CASET 6..471) from these dimensions — the window the vendor 175_qspi demo
 *  and the Arduino xiao_175.ino reference both use.
 *
 *  Touch is on the carrier (CST9217 / Seeed), independent of the panel's own
 *  TPIC (CST820B on this 1.75"), so Touch has no size-dependent config here.
 */
struct Config_XIAO_AMOLED_175 {
    using Driver = Driver_CO5300;
    using Panel  = Panel_TFT;
    static constexpr uint16_t width      = 466;
    static constexpr uint16_t height     = 466;
    static constexpr uint8_t  colorDepth = 16;
};

#endif // SEEED_GFX_XIAO_AMOLED_CONFIGS_H