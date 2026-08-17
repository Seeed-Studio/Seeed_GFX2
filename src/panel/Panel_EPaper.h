/**
 * @file   Panel_EPaper.h
 * @brief  ePaper panel implementation for Seeed_GFX v2.0
 *
 * Manages ePaper-specific features: frame buffer (1bpp/4bpp),
 * full/partial refresh, temperature-compensated
 * waveform optimization, and gray mode switching.
 *
 * The frame buffer is a standalone allocation managed by this class.
 * At 1bpp: each byte = 8 horizontal pixels, MSB = leftmost pixel.
 * At 4bpp: each byte = 2 horizontal pixels, high nibble = left pixel.
 * Monochrome uses 0 = white and 1 = black. Indexed4 uses the
 * controller palette values defined by this panel implementation.
 */

#ifndef SEEED_GFX_PANEL_EPAPER_H
#define SEEED_GFX_PANEL_EPAPER_H

#include <Arduino.h>
#include "../core/Panel.h"
#include "../driver/epaper/seeed_ep.h"
#include "../core/Board.h"
#include "../core/Gpio.h"

class Panel_EPaper : public IPanel {
public:
    Panel_EPaper(IDriver& driver, IBus& bus, IBoard* board = nullptr);
    virtual ~Panel_EPaper();

    // IPanel interface

    bool begin() override;
    GfxResult end() override;
    GfxResult lastResult() const override { return _lastResult; }
    GfxResult configure(PanelMode mode) override;
    GfxResult configureVisibleArea(uint16_t visibleWidth,
                                   uint16_t visibleHeight) override;
    uint16_t width() const override;
    uint16_t height() const override;
    uint8_t colorDepth() const override;
    void setRotation(uint8_t r) override;
    uint8_t rotation() const override;
    void invertDisplay(bool i) override;
    void setAddrWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) override;
    void writePixel(uint16_t color) override;
    void writePixels(const uint16_t* colors, size_t len) override;
    void writeFill(uint16_t color, size_t len) override;
    bool pushImage4BPP(int32_t x, int32_t y, int32_t w, int32_t h,
                       const uint8_t* data,
                       bool dataInProgmem = false) override;
    bool pushImage4BPPRotatedCW(int32_t x, int32_t y, int32_t w, int32_t h,
                                const uint8_t* data,
                                bool dataInProgmem = false) override;
    uint16_t readPixel(uint16_t x, uint16_t y) override;
    void sleep() override;
    void wake() override;
    void setBacklight(uint8_t brightness) override;
    uint8_t backlight() const override;
    IDriver& driver() override;
    DisplayCapabilities capabilities() const override;

    /** Select an explicit panel-vendor/batch waveform profile before refresh. */
    bool selectWaveformProfile(const char* id);
    const EPaperWaveformProfile* waveformProfile() const;
    EPaperWaveformResult waveformProfileResult() const;

    // ePaper-specific methods

    /** Full screen refresh - sends the entire frame buffer to the display.
     *  Wakes the display, pushes old+new color data, triggers update, then sleeps.
     */
    void update();
    GfxResult refresh() override;

    /** Fast full-screen refresh when supported by the controller. */
    void updateFast() override;
    GfxResult refreshFast() override;

    /** Partial screen refresh - only updates the specified region.
     *  @param x  Left coordinate (in rotated space)
     *  @param y  Top coordinate (in rotated space)
     *  @param w  Width of region
     *  @param h  Height of region
     */
    void updatePartial(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
    GfxResult refreshPartial(uint16_t x, uint16_t y,
                             uint16_t w, uint16_t h) override;

    /** Compatibility synchronization point.
     *  Current driver operations are synchronous and perform their own
     *  polarity-aware, bounded BUSY wait before returning.
     */
    void waitBusy();

    // Frame buffer access

    /** Get a pointer to the raw frame buffer.
     *  Format: 1bpp packed (8 pixels per byte, MSB first) or
     *          4bpp packed (2 pixels per byte, high nibble first).
     */
    uint8_t* frameBuffer();

    /** Number of bytes in one physical frame-buffer row. */
    size_t frameStride() const;

    /** Total number of bytes required by the current frame buffer. */
    size_t frameBufferSize() const;

    /** Write a byte-sized pixel block directly to the frame buffer.
     *  Used by text rendering when glyph data is already bit-packed.
     *  At 1bpp: writes 8 horizontal pixels in one byte.
     *  At 4bpp: writes 2 horizontal pixels in one byte.
     *  @param x     X coordinate (must be aligned to 8/bpp boundary)
     *  @param y     Y coordinate
     *  @param color Byte value to write (already formatted for the current bpp)
     *  @param bpp   Bits per pixel (1 or 4)
     */
    void drawBufferPixel(int32_t x, int32_t y, uint32_t color, uint8_t bpp);

    // Gray mode

    /** Switch to multi-gray mode (4-level or 16-level).
     *  Reallocates the frame buffer to 4bpp and fills with white.
     *  @param grayLevel  4 or 16
     */
    bool initGrayMode(uint8_t grayLevel);

    /** Switch back to monochrome (1bpp) mode.
     *  Reallocates the frame buffer to 1bpp and fills with white.
     */
    bool deinitGrayMode();

    // Color mode (6-color / BWRY)

    /** Switch to 6-color mode (Black, White, Red, Yellow, Blue, Green).
     *  Uses 4bpp frame buffer. Maps RGB565 colors to the 6-color palette.
     */
    bool initColorfulMode();

    /** Switch to BWRY mode (Black, White, Red, Yellow).
     *  Uses 4bpp frame buffer. Maps RGB565 colors to the 4-color palette.
     */
    bool initBWRYMode();

    /** Check if the panel is in colorful/BWRY mode */
    bool isColorMode() const { return _colorMode != COLOR_MODE_NONE; }

    // Temperature compensation

    /** Set the temperature for waveform optimization.
     *  @param temp  Temperature in degrees Celsius
     */
    void setTemp(float temp);

    /** Get the current temperature setting */
    float getTemp() const;

    // Horizontal mirror

    /** Enable or disable horizontal mirroring of the display output.
     *  Some ePaper panels are mounted with a horizontal mirror.
     */
    void setHorizontalMirror(bool mirror);
    bool horizontalMirror() const;

    // Vertical mirror

    /** Enable or disable vertical (native-Y) mirroring of the display output.
     *  Reverses row order at refresh time. A 90-degree rotation swaps
     *  native-X <-> displayed-Y and native-Y <-> displayed-X, so on a
     *  landscape-rotated (rot1/rot3) panel a board-level *horizontal* mirror
     *  must be countered with a *native-Y* flip (setHorizontalMirror flips
     *  native-X = displayed-Y on a rotated panel, which would give 180 deg,
     *  not a fix). Composes with setHorizontalMirror (both on = 180 deg).
     *  Full and partial refresh paths both honor this correction.
     */
    void setVerticalMirror(bool mirror);
    bool verticalMirror() const;

    /** Select automatic displayed-space horizontal correction. This follows
     *  rotation unless an explicit H/V mirror override is subsequently set. */
    void setDisplayHorizontalMirror(bool mirror);

    /** Select automatic displayed-space vertical correction. This follows
     *  rotation unless an explicit H/V mirror override is subsequently set. */
    void setDisplayVerticalMirror(bool mirror);

private:
    IDriver& _driver;
    IBus&    _bus;
    IBoard*  _board;

    uint8_t* _frameBuffer;
    uint8_t* _oldFrameBuffer;
    uint8_t  _rotation;
    uint8_t  _grayLevel;
    uint8_t  _bpp;
    enum ColorMode : uint8_t {
        COLOR_MODE_NONE     = 0,  // Monochrome or grayscale
        COLOR_MODE_COLORFUL = 1,  // 6-color (Black, White, Red, Yellow, Blue, Green)
        COLOR_MODE_BWRY     = 2,  // 4-color (Black, White, Red, Yellow)
    };
    uint8_t  _colorMode;
    bool     _sleeping;
    bool     _initialized;
    bool     _horizontalMirror;
    bool     _verticalMirror;
    bool     _displayHorizontalMirror;
    bool     _displayVerticalMirror;
    bool     _mirrorOverride;
    float    _temp;
    GfxResult _lastResult;

    // Controller/frame-buffer storage geometry. The 2.13" 122x250 panels
    // store byte-aligned 128-pixel rows, so this may exceed the visible size.
    uint16_t _init_width;
    uint16_t _init_height;

    // User-visible drawing geometry. Zero means "use all controller storage"
    // and is resolved during begin().
    uint16_t _visible_width;
    uint16_t _visible_height;

    // Address window tracking for writePixel / writeFill
    uint16_t _addr_x0, _addr_y0, _addr_x1, _addr_y1;
    uint16_t _addr_x, _addr_y;

    // Internal helpers

    /** Push raw previous-frame data through the concrete driver. */
    void pushOldColors(const uint8_t* data, size_t len);

    /** Push raw current-frame data through the concrete driver. */
    void pushNewColors(const uint8_t* data, size_t len);

    /** Push raw grayscale data through the controller's gray data planes. */
    void pushGrayColors(const uint8_t* data, size_t len);

    /** Push new color data with horizontal bit+byte flip.
     *  @param data        Source data buffer
     *  @param bytesPerRow Number of bytes per row (depends on bpp)
     *  @param h           Number of rows
     */
    bool pushNewColorsFlip(const uint8_t* data, uint16_t bytesPerRow, uint16_t h);
    bool pushOldColorsFlip(const uint8_t* data, uint16_t bytesPerRow, uint16_t h);
    bool pushGrayColorsFlip(const uint8_t* data, uint16_t bytesPerRow, uint16_t h);

    /** Trigger a full display update */
    void ePaperUpdate();

    /** Trigger a fast display update */
    void ePaperUpdateFast();

    /** Trigger a partial display update */
    void ePaperUpdatePartial();

    /** Wake the display from sleep and set temperature */
    void ePaperWakeUp();

    /** Put the display into deep sleep */
    void ePaperSleep();

    /** Send temperature compensation value to the display */
    void ePaperSetTemp(float temp);

    /** Execute a full-screen transfer using normal or fast waveform. */
    GfxResult refreshFull(bool fast);

    /** Convert the driver's sticky operation status to a public result. */
    GfxResult driverOperationResult() const;

    /** Allocate the frame buffer for the current bpp */
    bool allocateFrameBuffer();

    /** Free the frame buffer */
    void freeFrameBuffer();

    /** Map an RGB565 color to the nearest 6-color or BWRY palette entry */
    uint8_t mapColorToPalette(uint16_t color);

    /** Convert a stored indexed/grayscale nibble back to RGB565. */
    uint16_t decode4BitColor(uint8_t value) const;

    /** Map a logical rotated coordinate to the physical frame buffer. */
    bool mapToPhysical(int32_t x, int32_t y, uint16_t& physicalX,
                       uint16_t& physicalY) const;

    /** Force controller-only padding columns to the active mode's white. */
    void clearStoragePadding(uint8_t* buffer);
};

#endif // SEEED_GFX_PANEL_EPAPER_H
