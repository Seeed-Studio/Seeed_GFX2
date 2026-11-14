/**
 * @file   Driver_CO5300.h
 * @brief  CO5300 quad-SPI AMOLED driver for Seeed_GFX v2.0
 *
 * 480x480 RGB565 AMOLED controller, driven over the ESP32-S3 quad-SPI
 * transport (Bus_ESP32QSPI). Command/data framing matches the Espressif
 * esp_lcd_co5300 component used by the Waveshare ESP32-S3-Touch-AMOLED-2.16
 * BSP: LCD_OPCODE_WRITE_CMD 0x02 and LCD_OPCODE_WRITE_COLOR 0x32.
 */

#ifndef SEEED_GFX_DRIVER_CO5300_H
#define SEEED_GFX_DRIVER_CO5300_H

#include "../../core/Driver.h"

class Bus_ESP32QSPI;

class Driver_CO5300 : public IDriver {
public:
    /** @param w         Physical panel width in pixels (native, unrotated)
     *  @param h         Physical panel height in pixels
     *  @param rgbOrder  0 for RGB, 0x08 (TFT_MAD_BGR) to set the BGR bit.
     *                   The Waveshare board is RGB-native; the param exists
     *                   so createConfiguredDriver picks the 3-arg ctor. */
    Driver_CO5300(uint16_t w = 480, uint16_t h = 480, uint8_t rgbOrder = 0);
    ~Driver_CO5300() override;

    bool init(IBus& bus) override;
    uint16_t width() const override { return (_rotation & 1U) ? _height : _width; }
    uint16_t height() const override { return (_rotation & 1U) ? _width : _height; }
    uint16_t nativeWidth() const override { return _width; }
    uint16_t nativeHeight() const override { return _height; }
    uint8_t colorDepth() const override { return 16; }
    const char* name() const override { return "CO5300"; }
    void setRotation(uint8_t rotation) override;
    uint8_t rotation() const override { return _rotation; }
    void invertDisplay(bool invert) override;
    void displayOn() override;
    void displayOff() override;
    void setAddrWindow(uint16_t xs, uint16_t ys,
                       uint16_t xe, uint16_t ye) override;
    void writePixel(uint16_t color) override;
    void writePixels(const uint16_t* data, size_t len) override;
    void writeFill(uint16_t color, size_t len) override;
    void sleep() override;
    void wake() override;
    IBus& bus() override { return *_bus; }

    /** Probe/diagnostic accessor: pixels per CS pulse (DMA buffer). */
    size_t transferCapacityPixels() const { return _transferCapacityPixels; }

    /** Brightness through CMD 0x51 (the board has no PWM backlight). */
    void setBrightness(uint8_t value);

    /**
     * Inter-pulse phase sweep for multi-slice fills (scan-collision workaround,
     * see _interPulseSpacingUs below / flushChunk). baseUs=0 disables it.
     */
    void setInterPulseSpacingUs(uint32_t baseUs);

    /** Constant inter-pulse gap (us) inserted before EVERY slice of a
     *  multi-slice fill, independent of the sweep sawtooth. 0 = none. Used to
     *  probe how much spacing one-shot fills need to match the LVGL demo's
     *  cadence (which never fires consecutive 50-row pulses back to back). */
    void setInterPulseGapUs(uint32_t gapUs);

private:
    bool runInitSequence();
    /** Byte-swap (or fill) then push one DMA-capable chunk in RGB565 big-endian. */
    bool flushChunk(const uint16_t* data, size_t count,
                    bool fill, uint16_t fillColor);
    /** Send the staged odd-window rows as one pulse (see _pendingRows). */
    void finalizeStaged();

    uint16_t   _width;
    uint16_t   _height;
    uint8_t    _madctlTable[4];   // [rotation] -> MADCTL value
    Bus_ESP32QSPI* _qspi;
    uint16_t*  _transferBuffer;   // DMA-capable chunk buffer
    size_t     _transferCapacityPixels;
    // Current address window (clamped by setAddrWindow). writeFill/writePixels
    // slice this window into per-pulse windows because the CO5300 drops its
    // RAMWR state across CS pulses — every pulse must re-address its own
    // CASET/RASET slice, exactly like the BSP's per-draw_bitmap sends.
    uint16_t   _winX0, _winY0, _winX1, _winY1;
    // Inter-pulse phase sweep. The CO5300 panel refresh scans GRAM rows
    // continuously; a RAMWR slice that collides with the scan of its own
    // rows is dropped (GRAM keeps its old value — boot-green on a fresh
    // panel). A multi-slice fill that repeats the exact same pulse timing
    // every pass hits the scan at the same phase and drops the same bands
    // forever (see diag: dual-pass did nothing, 40x same-paced fills did
    // nothing, but 10 isolated fills 300ms apart all painted).
    //   setInterPulseSpacingUs(base) inserts delayMicroseconds(base * (p & 15))
    // before each slice (p = global slice counter), sweeping the write phase
    // over a full frame period. Every band eventually lands on a quiet phase
    // and, because a dropped write leaves GRAM unchanged, once written the
    // screen stays correct.
    volatile uint32_t _interPulseSpacingUs;
    uint32_t          _pulseNumber;
    volatile uint32_t _interPulseGapUs;
    // Even-height window assembler. The CO5300 discards color bursts aimed at
    // odd-height CASET/RASET windows (1-row blits paint nothing; every window
    // that landed was even), while Seeed_GFX::pushImage blits one row per call.
    // writePixels() stages those rows — already byte-swapped, in
    // _transferBuffer — until an even count is assembled, then ships them as a
    // single even-height pulse (2-row windows are proven to land). The staged
    // run's window is snapshotted here so a later setAddrWindow cannot corrupt
    // the target before finalizeStaged() runs.
    uint16_t _pendingX0, _pendingX1, _pendingY0;   // staged run's window (cols + first row)
    size_t   _pendingRows;                          // staged rows in _transferBuffer
    // Per-glass GRAM gap in native space, applied to CASET/RASET inside
    // setAddrWindow (2.05" = +16, 1.75" = +6, 2.16" = 0).  Derived from the
    // configured resolution (see panelProfileFor in the .cpp).
    uint16_t _gapX;
    uint16_t _gapY;
    // true -> the round 2.05" extended source/gate-map init; false -> the
    // 2.16"/1.75" simple init.  Same derivation source as _gapX/_gapY.
    bool     _extendedInit;
};

#endif // SEEED_GFX_DRIVER_CO5300_H