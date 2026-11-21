/**
 * @file   XIAO_Console_CAN_Board.h
 * @brief  Board definitions for the XIAO ESP32-C5 "console for CAN" carrier
 *         (ST7789 240x320 SPI TFT + GT911 capacitive touch).
 *
 * This is the same screen as the ESP-IDF xiao_c5_can_fd product; the pin table
 * below mirrors `components/can_console_bsp/src/can_console_bsp.c` (single
 * source of truth for the CAN console board).
 *
 *   LCD  : ST7789V, 4-wire SPI, 240x320, RGB565, RGB order, inverted
 *     SCLK = GPIO8   MOSI = GPIO10   CS = GPIO7   DC = GPIO25
 *   Reset / backlight / touch-reset are NOT GPIOs — they sit behind the
 *   on-board PCA9535 (0x21) expander:
 *     P0 = LCD reset (active low)   P5 = LCD backlight (active high)
 *     P1 = GT911 reset (active low)
 *   Touch: GT911 on the shared I2C (SDA=23 SCL=24), INT = GPIO0.
 *
 * Decoupling: the board owns the expander through Seeed_GFX's built-in
 * IoExpander9535 (src/board/IoExpander9535.h), the same pattern
 * Board_XIAO_AMOLED_Seeed uses.  pinRST()/pinBacklight() return -1 so the
 * driver skips GPIO reset/backlight and the board's own expander lines apply.
 *
 *   Seeed_GFX display;
 *   display.begin<Board_XIAO_Console_CAN, Config_XIAO_Console_CAN>();
 *
 * GT911 reset + I2C address select needs the MCU-owned INT line (GPIO0), so
 * resetTouch() below is richer than the AMOLED board's: it drives INT through
 * the vendor handshake so the chip latches the primary 0x5D address.  Call it
 * right before attachTouch() (the factory UI does the same).
 */

#ifndef XIAO_CONSOLE_CAN_BOARDS_H
#define XIAO_CONSOLE_CAN_BOARDS_H

#include <Arduino.h>
#include <new>
#include "../../core/Board.h"
#include "../../core/Gpio.h"
#include "../../bus/Bus_SPI.h"
#include "../IoExpander9535.h"

/** XIAO ESP32-C5 console-for-CAN carrier, ST7789 240x320 SPI + GT911. */
class Board_XIAO_Console_CAN : public IBoard {
public:
    Board_XIAO_Console_CAN() : _expander(Wire, 0x21) {}

    const char* name() const override {
        return "XIAO ESP32-C5 console for CAN (ST7789 + GT911)";
    }

    bool begin() override {
        // Shared I2C for the PCA9535 expander + GT911 touch (SDA=23 SCL=24).
        Wire.begin(23, 24, 100000);
        if (!_expander.begin()) return false;

        // Display-related expander lines: resets idle high, backlight off.
        _expander.pinModeOutput(kPinRst,       true);
        _expander.pinModeOutput(kPinTpRst,     true);
        _expander.pinModeOutput(kPinBacklight, false);

        resetLcd();
        // NOTE: touch reset is intentionally NOT pulsed here.  The GT911 reset
        // also does the INT-level address select, and it must land close to the
        // I2C probe; run resetTouch() after LCD init, right before attachTouch().
        return true;
    }

    // --- Pin queries. RST/backlight/touch-rst are behind the expander, so
    // those pins report -1 and begin() drives them through the onboard chip.
    int8_t pinCS()  const override { return 7; }   // LCD CS
    int8_t pinDC()  const override { return 25; }  // LCD DC/RS
    int8_t pinRST() const override { return -1; }  // expander P0
    int8_t pinBacklight() const override { return -1; }  // expander P5
    int8_t pinMOSI() const override { return 10; }
    int8_t pinMISO() const override { return -1; } // 4-wire SPI: no MISO
    int8_t pinSCLK() const override { return 8; }

    // Touch: GT911 on the shared I2C (SDA=23 SCL=24); INT = GPIO0.
    int8_t pinTouchCS()  const override { return -1; }
    int8_t pinTouchIRQ() const override { return 0; }

    IBus* createBus() override {
        // 240x320 ST7789 in SPI mode 0.  20 MHz is a safe bring-up default for
        // the GPIO-matrix SPI pins on ESP32-C5; raise it once colours are clean.
        Bus_SPI* bus = new (std::nothrow) Bus_SPI(pinCS(), pinDC(), pinRST(),
                                                  pinMOSI(), pinMISO(), pinSCLK(),
                                                  20 * 1000 * 1000U);
        if (bus) bus->setSPIMode(0);
        return bus;
    }

    // Backlight lives on expander P5 (active high). The carrier has no PWM for
    // it, so this is on/off at brightness 0 vs anything else.
    void setBacklight(uint8_t brightness) override {
        _expander.writePin(kPinBacklight, brightness != 0);
    }

    /**
     * GT911 vendor reset + I2C address select.  INT (GPIO0) is the MCU-owned
     * line sampled at the RESET release edge: low -> primary 0x5D, high ->
     * backup 0x14.  Mirrors can_console_display_gt911_reset().
     */
    void resetTouch() {
        gfxPinModeOutput(kTouchInt);
        gfxDigitalWrite(kTouchInt, true);        // INT idle high
        _expander.writePin(kPinTpRst, true);     // RESET idle high
        delay(2);
        _expander.writePin(kPinTpRst, false);    // assert RESET
        delay(10);
        _expander.writePin(kPinTpRst, true);     // release (first pulse)
        delay(10);
        _expander.writePin(kPinTpRst, false);    // assert again
        gfxDigitalWrite(kTouchInt, false);       // INT low through the handshake
        delay(20);
        delay(20);                               // GT_RD_CMD window, INT stays low
        gfxDigitalWrite(kTouchInt, false);       // address select: low => 0x5D
        _expander.writePin(kPinTpRst, true);     // release; INT level latched
        delay(20);
        gfxPinModeInput(kTouchInt);              // restore INT to input
    }

private:
    // PCA9535 port bits (NOT schematic P-names).
    enum ExpandPin : uint8_t {
        kPinRst       = 0,   // P0 LCD reset (active low)
        kPinTpRst     = 1,   // P1 GT911 reset (active low)
        kPinBacklight = 5,   // P5 LCD backlight (active high)
    };

    static constexpr int8_t kTouchInt = 0;   // GT911 INT = GPIO0

    void resetLcd() {
        _expander.writePin(kPinRst, true);
        delay(15);
        _expander.writePin(kPinRst, false);   // assert low
        delay(30);
        _expander.writePin(kPinRst, true);    // release
        delay(120);
    }

    IoExpander9535 _expander;
};

#endif // XIAO_CONSOLE_CAN_BOARDS_H