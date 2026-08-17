/**
 * @file   Panel.h
 * @brief  IPanel abstract interface for Seeed_GFX v2.0
 *
 * The Panel layer bridges Driver and Graphics. It manages panel-level
 * features: frame buffer, refresh modes (ePaper full/partial/fast),
 * backlight control, and power management.
 * Different panel types (TFT, ePaper, OLED) have different Panel subclasses.
 */

#ifndef SEEED_GFX_PANEL_H
#define SEEED_GFX_PANEL_H

#include <stdint.h>
#include <stddef.h>
#include "Driver.h"
#include "Capabilities.h"
#include "Result.h"

enum class PanelMode : uint8_t {
    Native = 0,
    Gray4,
    Gray16,
    Colorful,
    BWRY,
};

/** Physical color technology of an ePaper panel.
 *
 * This is separate from colorDepth(): colorDepth() describes the active
 * frame-buffer encoding, while this enum describes what the glass can render.
 */
enum class EPaperColorSystem : uint8_t {
    Unknown = 0,
    Monochrome,
    MonochromeGray,
    BWRY,
    Spectra6,
};

class IPanel {
public:
    virtual ~IPanel() = default;

    // Initialization

    /** Initialize the panel (board, bus, driver) */
    virtual bool begin() = 0;

    /** Last initialization, configuration, refresh, or shutdown result. */
    virtual GfxResult lastResult() const { return GfxResult::success(); }

    /**
     * Shut down the complete hardware stack owned by this panel binding.
     * Concrete panels override this when board power control is available.
     */
    virtual GfxResult end() {
        sleep();
        driver().bus().end();
        return GfxResult::success();
    }

    /** Configure an optional panel mode without exposing a concrete panel type. */
    virtual GfxResult configure(PanelMode mode) {
        return mode == PanelMode::Native
            ? GfxResult::success()
            : GfxResult(GfxError::NotSupported, "panel mode is not supported");
    }

    /**
     * Configure the user-visible drawing area before begin().
     *
     * Byte-aligned ePaper controllers can have wider RAM rows than the
     * visible glass (for example 128 stored columns for 122 visible pixels).
     */
    virtual GfxResult configureVisibleArea(uint16_t visibleWidth,
                                           uint16_t visibleHeight) {
        (void)visibleWidth;
        (void)visibleHeight;
        return GfxResult(GfxError::NotSupported,
                         "separate visible panel geometry is not supported");
    }

    // Dimensions

    /** Display width in pixels */
    virtual uint16_t width() const = 0;

    /** Display height in pixels */
    virtual uint16_t height() const = 0;

    /** Color depth in bits per pixel */
    virtual uint8_t colorDepth() const = 0;

    // Display control

    /** Set display rotation (0-3) */
    virtual void setRotation(uint8_t r) = 0;

    /** Get current rotation */
    virtual uint8_t rotation() const = 0;

    /** Invert display colors */
    virtual void invertDisplay(bool invert) = 0;

    // Pixel operations

    /** Set the pixel address window */
    virtual void setAddrWindow(uint16_t xs, uint16_t ys,
                               uint16_t xe, uint16_t ye) = 0;

    /** Write a single pixel */
    virtual void writePixel(uint16_t color) = 0;

    /** Write an array of pixels */
    virtual void writePixels(const uint16_t* data, size_t len) = 0;

    /** Fill with a single color */
    virtual void writeFill(uint16_t color, size_t len) = 0;

    /**
     * Draw a packed 4bpp image without treating palette indices as RGB565.
     * Each source byte stores two horizontal pixels, high nibble first.
     */
    virtual bool pushImage4BPP(int32_t x, int32_t y, int32_t w, int32_t h,
                               const uint8_t* data,
                               bool dataInProgmem = false) {
        (void)x; (void)y; (void)w; (void)h;
        (void)data; (void)dataInProgmem;
        return false;
    }

    /** Draw packed 4bpp data after a clockwise quarter turn. */
    virtual bool pushImage4BPPRotatedCW(int32_t x, int32_t y,
                                        int32_t w, int32_t h,
                                        const uint8_t* data,
                                        bool dataInProgmem = false) {
        (void)x; (void)y; (void)w; (void)h;
        (void)data; (void)dataInProgmem;
        return false;
    }

    /** Read a pixel at the given coordinates */
    virtual uint16_t readPixel(uint16_t x, uint16_t y) = 0;

    // Power management

    /** Put display to sleep */
    virtual void sleep() = 0;

    /** Wake display from sleep */
    virtual void wake() = 0;

    // Backlight

    /** Set backlight brightness (0-255) */
    virtual void setBacklight(uint8_t brightness) = 0;

    /** Get current backlight brightness */
    virtual uint8_t backlight() const = 0;

    // Accessors

    /** Get the underlying driver */
    virtual IDriver& driver() = 0;

    /** Query capabilities before using optional operations. */
    virtual DisplayCapabilities capabilities() const = 0;

    // Transfer target

    /** Transaction and DMA operations are exposed at the Panel boundary. */
    virtual void beginWrite() { driver().bus().beginWrite(); }
    virtual void endWrite() { driver().bus().endWrite(); }
    virtual bool enableDMA(bool enable = true) {
        return driver().enableDMA(enable);
    }
    virtual bool writePixelsDMA(const uint16_t* data, size_t len) {
        return driver().writePixelsDMA(data, len);
    }
    virtual bool dmaBusy() { return driver().dmaBusy(); }
    virtual void writeBytes(const uint8_t* data, size_t len) {
        driver().bus().writeData(data, len);
    }

    // ePaper / Display refresh

    /** Trigger a display refresh (ePaper full update).
     *  Default no-op for TFT/OLED panels. Panel_EPaper overrides this.
     */
    virtual GfxResult refresh() {
        return GfxResult(GfxError::NotSupported, "full refresh is not supported");
    }
    virtual GfxResult refreshPartial(uint16_t x, uint16_t y,
                                     uint16_t w, uint16_t h) {
        (void)x; (void)y; (void)w; (void)h;
        return GfxResult(GfxError::NotSupported, "partial refresh is not supported");
    }
    virtual GfxResult refreshFast() {
        return GfxResult(GfxError::NotSupported, "fast refresh is not supported");
    }

    /** Compatibility entry points. Prefer refresh()/refreshPartial(). */
    virtual void update() { (void)refresh(); }
    virtual void updateFast() { (void)refreshFast(); }
    virtual void updatePartial(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
        (void)refreshPartial(x, y, w, h);
    }
};

#endif // SEEED_GFX_PANEL_H
