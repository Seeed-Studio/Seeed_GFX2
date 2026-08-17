/**
 * @file   Board.h
 * @brief  IBoard abstract interface for Seeed_GFX v2.0
 *
 * The Board layer encapsulates board-specific pin mappings and
 * hardware initialization. Seeed provides predefined Board subclasses
 * for official products; users can also create custom boards.
 */

#ifndef SEEED_GFX_BOARD_H
#define SEEED_GFX_BOARD_H

#include <stdint.h>
#include <stddef.h>

// Forward declaration
class IBus;

class IBoard {
public:
    virtual ~IBoard() = default;

    // Board information

    /** Human-readable board name */
    virtual const char* name() const = 0;

    // Initialization

    /** Initialize board GPIOs, power, etc.
     *  @return true on success
     */
    virtual bool begin() = 0;

    // Pin queries

    /** Chip Select pin (-1 if not used) */
    virtual int8_t pinCS() const = 0;

    /** Optional secondary chip select for dual-controller panels. */
    virtual int8_t pinCS2() const { return -1; }

    /** Data/Command pin */
    virtual int8_t pinDC() const = 0;

    /** Reset pin (-1 if not connected) */
    virtual int8_t pinRST() const = 0;

    /** Backlight control pin (-1 if not used) */
    virtual int8_t pinBacklight() const = 0;

    // SPI pins
    virtual int8_t pinMOSI() const = 0;
    virtual int8_t pinMISO() const = 0;
    virtual int8_t pinSCLK() const = 0;

    // Touch pins
    virtual int8_t pinTouchCS()  const { return -1; }
    virtual int8_t pinTouchIRQ() const { return -1; }

    // ePaper-specific pins
    virtual int8_t busyPin() const { return -1; }
    virtual int8_t enablePin() const { return -1; }

    /** Default panel mounting/orientation correction for this product. */
    virtual bool panelHorizontalMirror() const { return false; }

    /**
     * Board-level left/right correction expressed in displayed coordinates.
     *
     * Unlike panelHorizontalMirror(), this follows the user's current
     * rotation: rotation 0/2 maps it to native X, while rotation 1/3 maps it
     * to native Y. It is intended for adapter boards such as the XIAO ePaper
     * Breakout whose front-view mounting correction must apply automatically
     * to every panel and every new sketch.
     */
    virtual bool panelDisplayHorizontalMirror() const { return false; }

    // Factory

    /** Create a bus instance for this board.
     *  The caller is responsible for deleting the returned object.
     */
    virtual IBus* createBus() = 0;

    // Backlight control

    /** Set backlight brightness (0-255).
     *  Default implementation uses digitalWrite if pinBacklight >= 0.
     */
    virtual void setBacklight(uint8_t brightness) { (void)brightness; }

    // Power control

    /** Power on the display */
    virtual void powerOn() {}

    /** Power off the display */
    virtual void powerOff() {}
};

#endif // SEEED_GFX_BOARD_H
