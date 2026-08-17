/**
 * @file   Font_ProgramSpace.h
 * @brief  Cross-core PROGMEM / pgm_read_* support for the font .c tables.
 *
 * The font tables are plain C (.c) and were ported from TFT_eSPI, which
 * used `#include <pgmspace.h>`. That header exists on AVR / ESP8266 / ESP32
 * but NOT on nRF52840 / RP2040 / SAMD / STM32 cores. On those cores flash
 * is directly addressable, so PROGMEM is a no-op and pgm_read_* is a plain
 * dereference. This header picks the right path per core and stays
 * C-compilable (no Arduino.h / C++).
 */
#ifndef SEEED_GFX_FONT_PROGRAM_SPACE_H
#define SEEED_GFX_FONT_PROGRAM_SPACE_H

#include <stdint.h>

#if defined(__AVR__) || defined(ARDUINO_ARCH_ESP8266) || \
    defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
  #include <pgmspace.h>
#else
  /* Non-AVR/ESP cores: flash is memory-mapped, no special annotation. */
  #ifndef PROGMEM
    #define PROGMEM
  #endif
  #ifndef PGM_P
    #define PGM_P const unsigned char*
  #endif
  #ifndef PGM_VOID_P
    #define PGM_VOID_P const void*
  #endif
  #ifndef PSTR
    #define PSTR(s) ((const char*)(s))
  #endif
  #ifndef pgm_read_byte
    #define pgm_read_byte(addr) (*(const uint8_t*)(addr))
  #endif
  #ifndef pgm_read_word
    #define pgm_read_word(addr) (*(const uint16_t*)(addr))
  #endif
  #ifndef pgm_read_dword
    #define pgm_read_dword(addr) (*(const uint32_t*)(addr))
  #endif
  #ifndef pgm_read_ptr
    #define pgm_read_ptr(addr) ((void*)pgm_read_dword(addr))
  #endif
#endif

#endif /* SEEED_GFX_FONT_PROGRAM_SPACE_H */
