/**
 * @file   Wio_Terminal.h
 * @brief  Board definition for Seeed Wio Terminal
 *
 * Uses Wio Terminal built-in LCD constants.
 * SPI Frequency: 24MHz write / 20MHz read
 */

#ifndef SEEED_GFX_BOARD_WIO_TERMINAL_H
#define SEEED_GFX_BOARD_WIO_TERMINAL_H

#include <Arduino.h>
#include "../../core/Board.h"
#include "../../bus/Bus_SPI.h"
#include "../configs/BoardConfig.h"
#include <new>

// Wio Terminal LCD pin definitions (from ATSAMD51 board package)
// Provide fallbacks when compiling for other platforms
#ifndef LCD_SS_PIN
  #define LCD_SS_PIN   A4   // CS
#endif
#ifndef LCD_DC
  #define LCD_DC       A3   // DC
#endif
#ifndef LCD_RESET
  #define LCD_RESET    A5   // RST
#endif
#ifndef LCD_BACKLIGHT
  #define LCD_BACKLIGHT A2  // BL
#endif
#ifndef LCD_MISO_PIN
  #ifdef PIN_SPI3_MISO
    #define LCD_MISO_PIN PIN_SPI3_MISO
  #else
    #define LCD_MISO_PIN -1
  #endif
#endif

class Board_Wio_Terminal : public IBoard {
public:
    static SpiBusConfig spiConfig() {
        return SpiBusConfig(_spiWriteFreq, _spiReadFreq);
    }

    static void configureBus(Bus_SPI& bus) {
        bus.setReadFrequency(_spiReadFreq);
    }

    const char* name() const override { return "Wio Terminal"; }

    bool begin() override {
        if (pinRST() >= 0) {
            pinMode(pinRST(), OUTPUT);
            digitalWrite(pinRST(), HIGH);
        }
        if (pinBacklight() >= 0) {
            pinMode(pinBacklight(), OUTPUT);
            digitalWrite(pinBacklight(), HIGH);
        }
        return true;
    }

    int8_t pinCS()  const override { return LCD_SS_PIN; }
    int8_t pinDC()  const override { return LCD_DC; }
    int8_t pinRST() const override { return LCD_RESET; }
    int8_t pinBacklight() const override { return LCD_BACKLIGHT; }
    int8_t pinMOSI() const override { return -1; } // Uses predefined SPI port
    // The built-in ILI9341 SDO signal is routed to SPI3 MISO.
    // The predefined LCD_SPI instance remains responsible for the bus.
    int8_t pinMISO() const override { return LCD_MISO_PIN; }
    int8_t pinSCLK() const override { return -1; }

    IBus* createBus() override {
        Bus_SPI* bus = new (std::nothrow) Bus_SPI(pinCS(), pinDC(), pinRST(),
                                    pinMOSI(), pinMISO(), pinSCLK(),
                                    _spiWriteFreq);
        if (!bus) return nullptr;
        configureBus(*bus);
        return bus;
    }

    void setBacklight(uint8_t brightness) override {
        if (pinBacklight() >= 0) {
            // The Wio Terminal variant declares LCD_BACKLIGHT (PC05) as
            // NOT_ON_PWM.  Treat brightness as an explicit on/off request so
            // this remains correct across Seeed SAMD core versions instead of
            // relying on analogWrite()'s platform-specific digital fallback.
            digitalWrite(pinBacklight(), brightness ? HIGH : LOW);
        }
    }

private:
    // 24MHz is the documented SAMD SPI ceiling (SEEED_SPI_MAX_FREQ). 50MHz was
    // over the SAMD51 SPI3 (sercom7) reliable limit and corrupted bulk pixel
    // transfers after init (short init commands survived -> "flash then black").
    static constexpr uint32_t _spiWriteFreq = 24000000;
    static constexpr uint32_t _spiReadFreq  = 20000000;
};

#endif // SEEED_GFX_BOARD_WIO_TERMINAL_H
