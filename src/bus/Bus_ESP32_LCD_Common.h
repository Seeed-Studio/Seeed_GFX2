/**
 * @file Bus_ESP32_LCD_Common.h
 * @brief Shared target detection for native ESP32-S3 LCD transports.
 */

#ifndef SEEED_GFX_BUS_ESP32_LCD_COMMON_H
#define SEEED_GFX_BUS_ESP32_LCD_COMMON_H

// Bus_ESP32RGB.cpp and Bus_ESP32QSPI.cpp include this header directly, before
// Arduino.h has had a chance to expose the ESP-IDF target macros. Load the
// selected Arduino-ESP32 SDK configuration explicitly so an ESP32-S3 build is
// not mistaken for a platform without the native LCD peripheral.
#if defined(ARDUINO_ARCH_ESP32)
#include <sdkconfig.h>
#endif

#if defined(ARDUINO_ARCH_ESP32) && defined(CONFIG_IDF_TARGET_ESP32S3)
#define SEEED_GFX_HAS_ESP32S3_LCD 1
#else
#define SEEED_GFX_HAS_ESP32S3_LCD 0
#endif

#endif
