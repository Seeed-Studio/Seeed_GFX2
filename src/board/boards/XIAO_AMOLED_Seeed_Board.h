/**
 * @file   XIAO_AMOLED_Seeed_Board.h
 * @brief  Board definitions for the Seeed XIAO 2.16" AMOLED carrier
 *         (XIAO-ESP32-S3-Plus module, "XIAO console for CAN", V1.0_260916).
 *
 * Product: XIAO-ESP32-S3-PLUS on the Seeed carrier
 *   LCD  : CO5300, quad-SPI, 480x480, RGB565
 *   Wiring (user-confirmed from the V1.0_260916 schematic):
 *     SCLK=43  DATA0=10  DATA1=11  DATA2=12  DATA3=13  CS=44
 *     SPI2 IO_MUX whitelist = GPIO 9..14 covers 10/11/12/13; 43/44 are routed
 *     through the GPIO matrix, the same mechanism the reference board uses.
 *   RST  : NOT a GPIO.  EP-RST sits behind the on-board PCA9535(0x21) expander
 *          pin P00 with an external pull-up (idle high).
 *   Power: VBAT/VCI/IOVCC rails are also behind the expander (P11/P10/P16).
 *   Touch: on the shared I2C1 (SDA=5 SCL=6), TP-INT=GPIO3(D2), TP-RST on
 *          expander pin P01.  Controller follows the glass: CST9217 @0x5A
 *          (2.16" square) or CST820/CST820B @0x15 (2.05"/1.75" round).
 *
 * Decoupling: the board owns the expander through Seeed_GFX's built-in
 * IoExpander9535 (src/board/IoExpander9535.h) — no third-party pca9535
 * library — the same pattern Board_SenseCAP_Watcher uses.  begin() brings up
 * the rails and pulses LCD + touch reset; powerOn()/powerOff() expose the
 * rails for runtime suspend/resume.
 *
 *   Seeed_GFX display;
 *   display.begin<Board_XIAO_AMOLED_Seeed, Config_XIAO_AMOLED>();
 *
 * A sketch that already owns the expander may still override the LCD reset by
 * calling setResetDriver() BEFORE begin<>(); otherwise the board's own
 * expander line is used.
 */

#ifndef XIAO_AMOLED_SEEED_BOARDS_H
#define XIAO_AMOLED_SEEED_BOARDS_H

#include <Arduino.h>
#include <new>
#include "../../core/Board.h"
#include "../../bus/Bus_ESP32QSPI.h"
#include "../IoExpander9535.h"

/** Seeed XIAO 2.16" AMOLED carrier, CO5300 480x480 QSPI. */
class Board_XIAO_AMOLED_Seeed : public IBoard {
public:
    /** Reset assertion function: active=true drives the line low. */
    typedef void (*ResetDriverFn)(bool active, void* ctx);

    Board_XIAO_AMOLED_Seeed() : _expander(Wire, 0x21) {}

    const char* name() const override {
        return "XIAO-ESP32-S3-PLUS AMOLED (Seeed 2.16 carrier)";
    }

    bool begin() override {
        // Shared I2C1 for the expander + CST9217 touch (SDA=5 SCL=6).
        Wire.begin(5, 6, 100000);
        if (!_expander.begin()) return false;

        // On-board expander pins: rails out low, resets out high (idle).
        _expander.pinModeOutput(kPinVbat,  false);
        _expander.pinModeOutput(kPinVci,   false);
        _expander.pinModeOutput(kPinIovcc, false);
        _expander.pinModeOutput(kPinRst,   true);
        _expander.pinModeOutput(kPinTpRst, true);
        powerOn();

        resetLcd();
        resetTouch();
        return true;
    }

    /** Bring the panel rails up (P11 VBAT, P10 VCI, P16 IOVCC). */
    void powerOn() override {
        _expander.writePin(kPinVbat,  true);
        _expander.writePin(kPinVci,   true);
        _expander.writePin(kPinIovcc, true);
        delay(10);
    }

    /** Drop the panel rails (reverse order of powerOn()). */
    void powerOff() override {
        _expander.writePin(kPinIovcc, false);
        _expander.writePin(kPinVci,   false);
        _expander.writePin(kPinVbat,  false);
    }

    // --- Pin queries. RST is behind the expander (P00 = EP-RST), so
    // pinRST() == -1; begin() asserts it through the onboard expander
    // (or the optional reset driver).
    int8_t pinCS()  const override { return 8; }
    int8_t pinDC()  const override { return -1; }  // QSPI: no separate DC line
    int8_t pinRST() const override { return -1; }  // via expander P00, not GPIO
    int8_t pinBacklight() const override { return -1; }  // register 0x51, no PWM
    int8_t pinMOSI() const override { return 10; }  // DATA0
    int8_t pinMISO() const override { return 11; }  // DATA1
    int8_t pinSCLK() const override { return 7; }  // QSPI_SCK

    // Touch: CST9217 on the shared I2C1 (SDA=GPIO5 SCL=GPIO6);
    // TP-INT = GPIO3 (header D2).
    int8_t pinTouchCS()  const override { return -1; }
    int8_t pinTouchIRQ() const override { return 3; }

    IBus* createBus() override {
        const Esp32QspiLcdConfig config = {
            /* sclk             */ 44,
            /* data0..data3    */ 10, 11, 12, 13,
            /* cs              */ 43,
            /* frequency       */ 40 * 1000 * 1000U,   // A/B: 40MHz -> 10MHz
                                                       // (white/black clean but
                                                       // colors garble @40MHz;
                                                       // 10MHz isolates
                                                       // timing-skew vs wiring)
            /* maxTransferBytes */ 480U * 480U * 2U,
            /* spiMode         */ 0,   // CO5300 quad mode = SPI mode 0
            /* host            */ 1,   // SPI2_HOST (data pins 10-13 whitelist)
        };
        return new (std::nothrow) Bus_ESP32QSPI(config);
    }

    // Brightness is handled by Driver_CO5300 through CMD 0x51.
    void setBacklight(uint8_t /*brightness*/) override {}

    /**
     * Install an external reset driver begin() uses instead of the onboard
     * expander line (e.g. when the sketch already owns the expander).
     * @param fn  Called with (true) to assert reset low, (false) to release.
     * @param ctx Opaque context forwarded to fn (may be nullptr).
     */
    static void setResetDriver(ResetDriverFn fn, void* ctx = nullptr) {
        s_resetDriver() = fn;
        s_resetCtx() = ctx;
    }

    /** Detach the reset driver; begin() then uses the onboard expander. */
    static void clearResetDriver() {
        s_resetDriver() = nullptr;
        s_resetCtx() = nullptr;
    }

    /**
     * Pulse the touch-controller reset (TP-RST, P01). begin() runs this once,
     * but that happens before the ~1.2 s QSPI LCD init and the CST9217 can
     * idle back to a non-ACKing state in between. Re-pulse right before
     * attachTouch()'s config read (the factory UI does the same).
     */
    void resetTouch() {
        _expander.writePin(kPinTpRst, true);
        delay(2);
        _expander.writePin(kPinTpRst, false);   // assert low
        delay(10);
        _expander.writePin(kPinTpRst, true);    // release
        delay(50);
    }

private:
    // Expander bit indices (PCA9535 port bits, NOT schematic P-names).
    enum ExpandPin : uint8_t {
        kPinRst   = 0,   // P00 EP-RST  (LCD reset)
        kPinTpRst = 1,   // P01 TP-RST  (touch reset)
        kPinVci   = 8,   // P10 panel VCI
        kPinVbat  = 9,   // P11 panel VBAT
        kPinIovcc = 14,  // P16 U23 EP_VCI rail
    };

    // LCD reset: preferred external hook, falling back to the onboard line.
    void resetLcd() {
        if (s_resetDriver() != nullptr) {
            s_resetDriver()(false, s_resetCtx());   // release (idle high)
            delay(200);
            s_resetDriver()(true, s_resetCtx());    // assert low
            delay(30);
            s_resetDriver()(false, s_resetCtx());   // release
            delay(120);
            return;
        }
        _expander.writePin(kPinRst, true);
        delay(15);
        _expander.writePin(kPinRst, false);   // assert low
        delay(30);
        _expander.writePin(kPinRst, true);    // release
        delay(120);
    }

    // Touch reset (TP-RST active low): see the public resetTouch().

    IoExpander9535 _expander;

    // Function-local statics: header-only, no ODR issues across TUs.
    static ResetDriverFn& s_resetDriver() {
        static ResetDriverFn fn = nullptr;
        return fn;
    }
    static void*& s_resetCtx() {
        static void* ctx = nullptr;
        return ctx;
    }
};

#endif // XIAO_AMOLED_SEEED_BOARDS_H