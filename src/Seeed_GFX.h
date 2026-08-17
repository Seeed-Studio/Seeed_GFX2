/**
 * @file   Seeed_GFX.h
 * @brief  Main graphics library header for Seeed_GFX v2.0
 *
 * Seeed_GFX is the main user-facing graphics API class.
 * It inherits from Arduino's Print class and provides a
 * comprehensive set of drawing, text, and image rendering functions.
 *
 * Quick Start:
 * @code
 *   #include <Seeed_GFX.h>
 *   // Include board + driver + panel for your setup
 *   #include "board/boards/XIAO_ESP32S3.h"
 *   #include "driver/tft/Driver_ST7789.h"
 *   #include "panel/Panel_TFT.h"
 *
 *   Seeed_GFX display;
 *   void setup() {
 *       display.begin<Board_XIAO_ESP32S3, Config_XIAO_Expansion_1inch28_Round_GC9A01>();
 *       display.fillScreen(TFT_BLACK);
 *       display.drawString("Hello Seeed!", 10, 10);
 *   }
 * @endcode
 */

#ifndef SEEED_GFX_H
#define SEEED_GFX_H

#include <Arduino.h>
#include <Print.h>
#include <SPI.h>
#include <new>

// Enable font support
#define LOAD_GLCD
#define LOAD_GFXFF

// Core interfaces (always included)
#include "core/Color.h"
#include "core/Font.h"
#include "core/Bus.h"
#include "core/Driver.h"
#include "core/Panel.h"
#include "core/Board.h"
#include "core/Touch.h"
#include "core/Sprite.h"
#include "core/Product.h"
#include "core/Result.h"
#include "core/Capabilities.h"
#include "runtime/DisplayInstance.h"
#include "runtime/ProductDetection.h"

// Bus implementations (always included - lightweight)
#include "bus/Bus_SPI.h"
#include "bus/Bus_I2C.h"
#include "bus/Bus_Parallel8.h"

// Panel configs (data-driven, always included - lightweight structs)
// All configs are defined in Seeed_Panel_Configs.h
#include "panel/configs/Seeed_Panel_Configs.h"

// Fonts (always included for text rendering)
#include "font/Font_GLCD.h"
#include "font/Font_GFX.h"
#include "font/Font_Smooth.h"
#include "widgets/Button.h"
#include "widgets/ClassicWidgets.h"
#if !defined(ARDUINO_ARCH_NRF52)
#include "ui/image/UiImageDecoder.h"
#endif

class Driver_IT8951;

// Main Seeed_GFX Class

class Seeed_GFX : public Print {
public:
    // Constructors

    Seeed_GFX();
    explicit Seeed_GFX(IPanel& panel);
    explicit Seeed_GFX(Seeed_Product::Product product);
    virtual ~Seeed_GFX();

    // Panel management

    /** Attach a caller-owned panel. Seeed_GFX never deletes the panel. */
    void attachPanel(IPanel& panel);
    /** Backward-compatible alias for attachPanel(). */
    void setPanel(IPanel& panel);
    /** Panel reference; call hasPanel() first. */
    IPanel& panel() const { return *_panel; }
    IPanel* panelPtr() const { return _panel; }
    IBoard* boardPtr() const { return _board; }
    Seeed_Product::Product activeProduct() const { return _activeProduct; }
    IDriver* driverPtr() const { return _panel ? &_panel->driver() : nullptr; }
    template <typename DriverType>
    DriverType* driverAs() const {
        return _panel && dynamic_cast<DriverType*>(&_panel->driver())
            ? static_cast<DriverType*>(&_panel->driver()) : nullptr;
    }
    bool hasPanel() const { return _panel != nullptr; }
    DisplayCapabilities capabilities() const {
        return _panel ? _panel->capabilities() : DisplayCapabilities();
    }
    BusCapabilities busCapabilities() const {
        return _panel ? _panel->driver().bus().capabilities()
                      : BusCapabilities();
    }
    GfxResult lastResult() const { return _lastResult; }

    // Template: Quick setup with predefined Board + Panel combo

    /** Begin with a predefined board class.
     *  Use this when you want to construct driver and panel manually.
     */
    template<typename BoardType>
#if defined(__GNUC__)
    __attribute__((deprecated("begin<BoardType>() does not create a usable display; use begin<BoardType, PanelConfigType>()")))
#endif
    bool begin() {
        releaseOwnedHardware();
        _board = new (std::nothrow) BoardType();
        if (!_board) {
            _lastResult = GfxResult(GfxError::AllocationFailed, "board allocation failed");
            return false;
        }
        _ownsBoard = true;
        const bool ok = _board->begin();
        _lastResult = ok ? GfxResult::success()
                         : GfxResult(GfxError::BoardInitFailed, "board initialization failed");
        return ok;
    }

    /** Begin with a predefined board + panel config.
     *  This is the recommended quick-start approach.
     *  NOTE: Include the board, driver, and panel headers before calling.
     *  Example:
     *    #include "board/boards/XIAO_ESP32S3.h"
     *    #include "driver/tft/Driver_GC9A01.h"
     *    #include "panel/Panel_TFT.h"
     *    tft.begin<Board_XIAO_ESP32S3,
     *              Config_XIAO_Expansion_1inch28_Round_GC9A01>();
     */
    template<typename BoardType, typename PanelConfigType>
    bool begin() {
        releaseOwnedHardware();
        BoardType* board = new (std::nothrow) BoardType();
        if (!board) {
            _lastResult = GfxResult(GfxError::AllocationFailed, "board allocation failed");
            return false;
        }
        IBus* bus = board->createBus();
        IDriver* driver = createConfiguredDriver<PanelConfigType>(0);
        IPanel* panel = (bus && driver)
            ? new (std::nothrow) typename PanelConfigType::Panel(*driver, *bus, board)
            : nullptr;
        applyMirrorFromConfig<PanelConfigType>(panel, 0);
        _lastResult = _instance.adopt(board, bus, driver, panel);
        if (!_lastResult) return false;
        _lastResult = configurePanelGeometryForConfig<PanelConfigType>(
            *_instance.panel(), 0);
        if (!_lastResult) {
            _instance.reset();
            return false;
        }
        _lastResult = _instance.begin();
        if (!_lastResult) {
            _instance.reset();
            return false;
        }
        _lastResult = configurePanelForConfig<PanelConfigType>(*_instance.panel(), 0);
        if (!_lastResult) {
            _instance.reset();
            return false;
        }
        _instance.panel()->setRotation(configInitialRotation<PanelConfigType>(0));
        applyBreakoutMirrorPolicy<BoardType, PanelConfigType>(_instance.panel(), 0);
        applyBreakoutVerticalMirrorPolicy<BoardType, PanelConfigType>(_instance.panel(), 0);
        applyInversionFromConfig<PanelConfigType>(_instance.panel(), 0);
        _board = _instance.board();
        _panel = _instance.panel();
        _ownsBoard = false;
        bool ok = _panel != nullptr;
        if (ok) {
            _vpW = _panel->width();
            _vpH = _panel->height();
            _vpX = _vpY = 0;
            _vpOoB = false;
            _booted = true;
        }
        return ok;
    }

    /** Begin with a product enum */
    bool begin(Seeed_Product::Product product);

    /** Begin with an existing panel */
    bool begin();
    bool init() { return begin(); }
    void initFromSleep() { if (_panel) _panel->wake(); }

    /** Shut down the active panel stack. Safe to call repeatedly. */
    GfxResult end();

    // Graphics API

    // --- Core drawing ---
    virtual void drawPixel(int32_t x, int32_t y, uint32_t color);
    virtual void drawLine(int32_t xs, int32_t ys, int32_t xe, int32_t ye, uint32_t color);
    virtual void drawFastVLine(int32_t x, int32_t y, int32_t h, uint32_t color);
    virtual void drawFastHLine(int32_t x, int32_t y, int32_t w, uint32_t color);
    virtual void fillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color);
    virtual void fillScreen(uint32_t color);

    void drawRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color);
    void drawRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t radius, uint32_t color);
    void fillRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t radius, uint32_t color);

    void drawCircle(int32_t x, int32_t y, int32_t r, uint32_t color);
    void drawCircleHelper(int32_t x, int32_t y, int32_t r, uint8_t cornername, uint32_t color);
    void fillCircle(int32_t x, int32_t y, int32_t r, uint32_t color);
    void fillCircleHelper(int32_t x, int32_t y, int32_t r, uint8_t cornername, int32_t delta, uint32_t color);

    void drawEllipse(int16_t x, int16_t y, int32_t rx, int32_t ry, uint16_t color);
    void fillEllipse(int16_t x, int16_t y, int32_t rx, int32_t ry, uint16_t color);

    void drawTriangle(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t x3, int32_t y3, uint32_t color);
    void fillTriangle(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t x3, int32_t y3, uint32_t color);

    void drawRectVGradient(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t color1, uint32_t color2);
    void drawRectHGradient(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t color1, uint32_t color2);
    // TFT_eSPI-compatible names.  The older library API accidentally used
    // "drawRect" even though these functions fill the complete rectangle.
    void fillRectVGradient(int16_t x, int16_t y, int16_t w, int16_t h,
                           uint32_t color1, uint32_t color2) {
        drawRectVGradient(x, y, w, h, color1, color2);
    }
    void fillRectHGradient(int16_t x, int16_t y, int16_t w, int16_t h,
                           uint32_t color1, uint32_t color2) {
        drawRectHGradient(x, y, w, h, color1, color2);
    }

    // --- Smooth (anti-aliased) drawing ---
    uint16_t drawPixel(int32_t x, int32_t y, uint32_t color, uint8_t alpha, uint32_t bg_color = 0x00FFFFFF);
    void drawSmoothArc(int32_t x, int32_t y, int32_t r, int32_t ir, uint32_t startAngle, uint32_t endAngle,
                       uint32_t fg_color, uint32_t bg_color, bool roundEnds = false);
    void drawArc(int32_t x, int32_t y, int32_t r, int32_t ir, uint32_t startAngle, uint32_t endAngle,
                 uint32_t fg_color, uint32_t bg_color, bool smoothArc = true);
    void drawSmoothCircle(int32_t x, int32_t y, int32_t r, uint32_t fg_color, uint32_t bg_color);
    void fillSmoothCircle(int32_t x, int32_t y, int32_t r, uint32_t color, uint32_t bg_color = 0x00FFFFFF);
    void drawSmoothRoundRect(int32_t x, int32_t y, int32_t r, int32_t ir, int32_t w, int32_t h,
                             uint32_t fg_color, uint32_t bg_color = 0x00FFFFFF, uint8_t quadrants = 0xF);
    void fillSmoothRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t radius,
                             uint32_t color, uint32_t bg_color = 0x00FFFFFF);
    void drawSpot(float ax, float ay, float r, uint32_t fg_color, uint32_t bg_color = 0x00FFFFFF);
    void drawWideLine(float ax, float ay, float bx, float by, float wd, uint32_t fg_color, uint32_t bg_color = 0x00FFFFFF);
    void drawWedgeLine(float ax, float ay, float bx, float by, float aw, float bw, uint32_t fg_color, uint32_t bg_color = 0x00FFFFFF);

    // --- Pixel reading ---
    virtual uint16_t readPixel(int32_t x, int32_t y);

    // --- Image rendering ---
    void setSwapBytes(bool swap);
    bool getSwapBytes() const;

    void drawBitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint16_t fgcolor);
    void drawBitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint16_t fgcolor, uint16_t bgcolor);
    /** Draw an MSB-first packed 1bpp bitmap after a clockwise quarter turn.
     *  The source dimensions are w x h; the output occupies h x w. */
    void drawBitmapRotatedCW(int16_t x, int16_t y, const uint8_t *bitmap,
                             int16_t w, int16_t h, uint16_t fgcolor,
                             uint16_t bgcolor);
    void drawXBitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint16_t fgcolor);
    void drawXBitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint16_t fgcolor, uint16_t bgcolor);
    void setBitmapColor(uint16_t fgcolor, uint16_t bgcolor);

    void setPivot(int16_t x, int16_t y);
    int16_t getPivotX() const;
    int16_t getPivotY() const;

    void readRect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t *data);
    void pushRect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t *data);

    void pushImage(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t *data);
    void pushImage(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t *data, uint16_t transparent);
    void pushImage(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t *data, uint16_t transparent);
    void pushImage(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t *data);
    void pushImage(int32_t x, int32_t y, int32_t w, int32_t h, uint8_t *data, bool bpp8 = true, uint16_t *cmap = nullptr);
    void pushImage(int32_t x, int32_t y, int32_t w, int32_t h, uint8_t *data, uint8_t transparent, bool bpp8 = true, uint16_t *cmap = nullptr);
    void pushImage(int32_t x, int32_t y, int32_t w, int32_t h, const uint8_t *data, bool bpp8, uint16_t *cmap = nullptr);
    /**
     * Draw packed indexed/grayscale 4bpp ePaper data (two pixels per byte).
     * Set dataInProgmem for embedded image.h assets; leave it false for RAM.
     */
    bool pushImage4BPP(int32_t x, int32_t y, int32_t w, int32_t h,
                       const uint8_t* data,
                       bool dataInProgmem = false);
    /** Draw packed indexed/grayscale 4bpp ePaper data after a clockwise turn. */
    bool pushImage4BPPRotatedCW(int32_t x, int32_t y, int32_t w, int32_t h,
                                const uint8_t* data,
                                bool dataInProgmem = false);
    // Decode an in-memory BMP (16-bit RGB555 or 24-bit RGB888, uncompressed)
    // and blit it row by row. `data` may live in RAM or memory-mapped flash
    // (ESP32 / SAMD / STM32). Returns false on unsupported format or decode
    // error. BMP decode is a UI-layer facility, unavailable on nRF52 (no-op).
    bool drawBmp(int32_t x, int32_t y, const uint8_t* data, size_t len);
#if !defined(ARDUINO_ARCH_NRF52)
    UiStatus drawImage(int32_t x, int32_t y, IUiDataSource& source,
                       UiImageFormat format = UiImageFormat::Auto,
                       void* workBuffer = nullptr, size_t workBytes = 0);
    UiStatus drawImage(int32_t x, int32_t y, const void* data,
                       size_t dataBytes,
                       UiImageFormat format = UiImageFormat::Auto,
                       void* workBuffer = nullptr, size_t workBytes = 0);
    UiStatus drawImage(int32_t x, int32_t y, IUiDataSource& source,
                       IUiImageDecoder& decoder, void* workBuffer = nullptr,
                       size_t workBytes = 0);
    bool drawImageRaw(int32_t x, int32_t y, int32_t w, int32_t h,
                      const void* data, size_t dataBytes,
                      UiImageFormat format,
                      const uint16_t* colorMap = nullptr);
#endif
    void pushMaskedImage(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t *img, uint8_t *mask);
    void readRectRGB(int32_t x, int32_t y, int32_t w, int32_t h, uint8_t *data);

    // --- Pixel push ---
    void pushColor(uint16_t color);
    void pushColor(uint16_t color, uint32_t len);
    void pushColors(uint16_t *data, uint32_t len, bool swap = true);
    void pushColors(uint8_t *data, uint32_t len);
    void pushBlock(uint16_t color, uint32_t len);
    void pushPixels(const void *data_in, uint32_t len);

    // Text API

    int16_t drawNumber(long intNumber, int32_t x, int32_t y, uint8_t font);
    int16_t drawNumber(long intNumber, int32_t x, int32_t y);
    int16_t drawFloat(float floatNumber, uint8_t decimal, int32_t x, int32_t y, uint8_t font);
    int16_t drawFloat(float floatNumber, uint8_t decimal, int32_t x, int32_t y);
    int16_t drawString(const char *string, int32_t x, int32_t y, uint8_t font);
    int16_t drawString(const char *string, int32_t x, int32_t y);
    int16_t drawString(const String &string, int32_t x, int32_t y, uint8_t font);
    int16_t drawString(const String &string, int32_t x, int32_t y);
    int16_t drawCentreString(const char *string, int32_t x, int32_t y, uint8_t font);
    int16_t drawRightString(const char *string, int32_t x, int32_t y, uint8_t font);
    int16_t drawCentreString(const String& string, int32_t x, int32_t y,
                             uint8_t font) {
        return drawCentreString(string.c_str(), x, y, font);
    }
    int16_t drawRightString(const String& string, int32_t x, int32_t y,
                            uint8_t font) {
        return drawRightString(string.c_str(), x, y, font);
    }

    virtual int16_t drawChar(uint16_t uniCode, int32_t x, int32_t y, uint8_t font);
    virtual int16_t drawChar(uint16_t uniCode, int32_t x, int32_t y);
    virtual void drawChar(int32_t x, int32_t y, uint16_t c, uint32_t color, uint32_t bg, uint8_t size);

    void setCursor(int16_t x, int16_t y);
    void setCursor(int16_t x, int16_t y, uint8_t font);
    int16_t getCursorX() const;
    int16_t getCursorY() const;

    void setTextColor(uint16_t color);
    void setTextColor(uint16_t fgcolor, uint16_t bgcolor, bool bgfill = false);
    void setTextSize(uint8_t size);
    void setTextWrap(bool wrapX, bool wrapY = false);
    void setTextDatum(uint8_t datum);
    uint8_t getTextDatum() const;
    void setTextPadding(uint16_t x_width);
    uint16_t getTextPadding() const;

    void setFreeFont(const GFXfont *f = NULL);
    void setTextFont(uint8_t font);

    int16_t textWidth(const char *string, uint8_t font);
    int16_t textWidth(const char *string);
    int16_t textWidth(const String &string, uint8_t font);
    int16_t textWidth(const String &string);
    int16_t fontHeight(uint8_t font);
    int16_t fontHeight();

    uint16_t decodeUTF8(uint8_t *buf, uint16_t *index, uint16_t remaining);
    uint16_t decodeUTF8(uint8_t c);

    virtual int16_t height() const;
    virtual int16_t width() const;

    size_t write(uint8_t c) override;

    void setCallback(getColorCallback getCol);
    uint16_t fontsLoaded() const;

    // VLW smooth-font convenience API. Fonts are supplied as PROGMEM arrays.
    bool loadFont(const uint8_t* fontData);
#if SEEED_GFX_HAS_FS
    bool loadFont(const char* path, fs::FS& fileSystem);
    bool loadFont(const String& path, fs::FS& fileSystem) {
        return loadFont(path.c_str(), fileSystem);
    }
#endif
    void unloadFont();
    bool smoothFontLoaded() const;
    /** Legacy-compatible smooth-font loaded-state query. */
    bool isFontLoaded() const { return smoothFontLoaded(); }
    uint16_t drawGlyph(uint16_t code);
    /** Render all loaded smooth-font glyphs using this display's dimensions. */
    void showFont(uint32_t pageDelay = 0);
    SmoothFont& smoothFont() { return _smoothFont; }

    // Display settings

    void setRotation(uint8_t r);
    uint8_t getRotation() const;
    /** Set the touch controller rotation independently of the display.
     *  Useful when the touch panel is mounted with a different orientation
     *  than the LCD panel (e.g. SenseCAP Indicator GX). */
    void setTouchRotation(uint8_t r);
    void invertDisplay(bool i);

    // ePaper-specific (no-op for TFT/OLED)
    GfxResult refresh() {
        _lastResult = _panel
            ? _panel->refresh()
            : GfxResult(GfxError::NotInitialized, "no panel attached");
        return _lastResult;
    }
    GfxResult refreshPartial(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
        _lastResult = _panel
            ? _panel->refreshPartial(x, y, w, h)
            : GfxResult(GfxError::NotInitialized, "no panel attached");
        return _lastResult;
    }
    GfxResult refreshFast() {
        _lastResult = _panel
            ? _panel->refreshFast()
            : GfxResult(GfxError::NotInitialized, "no panel attached");
        return _lastResult;
    }
    /** Compatibility wrappers. Prefer the result-bearing refresh methods. */
    void update() { (void)refresh(); }
    void updateFast() { (void)refreshFast(); }
    void updatePartial(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
        (void)refreshPartial(x, y, w, h);
    }

    void setViewport(int32_t x, int32_t y, int32_t w, int32_t h, bool vpDatum = true);
    bool checkViewport(int32_t x, int32_t y, int32_t w, int32_t h);
    int32_t getViewportX() const;
    int32_t getViewportY() const;
    int32_t getViewportWidth() const;
    int32_t getViewportHeight() const;
    bool getViewportDatum() const;
    void frameViewport(uint16_t color, int32_t w);
    void resetViewport();
    bool clipAddrWindow(int32_t *x, int32_t *y, int32_t *w, int32_t *h);
    bool clipWindow(int32_t *xs, int32_t *ys, int32_t *xe, int32_t *ye);

    void setOrigin(int32_t x, int32_t y);
    int32_t getOriginX() const;
    int32_t getOriginY() const;

    // Transaction control

    void startWrite();
    void endWrite();
    void writeColor(uint16_t color, uint32_t len);
    void setWindow(int32_t xs, int32_t ys, int32_t xe, int32_t ye);
    void setAddrWindow(int32_t x, int32_t y, int32_t w, int32_t h);

    // Low-level controller access for sketches that used TFT_eSPI command APIs.
    void writecommand(uint8_t command);
    void writedata(uint8_t data);
    void writendata(const uint8_t* data, uint16_t length);
    void writecommanddata(uint8_t command, const uint8_t* data, uint16_t length);
    uint8_t readcommand8(uint8_t command, uint8_t index = 0);
    uint16_t readcommand16(uint8_t command, uint8_t index = 0);
    uint32_t readcommand32(uint8_t command, uint8_t index = 0);

    // DMA support

    bool initDMA(bool ctrl_cs = false);
    void deInitDMA();
    void pushImageDMA(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t *data, uint16_t *buffer = nullptr);
    void pushImageDMA(int32_t x, int32_t y, int32_t w, int32_t h,
                      const uint16_t* data);
    void pushPixelsDMA(uint16_t *image, uint32_t len);
    DmaTransferResult submitImageAsync(int32_t x, int32_t y, int32_t w,
                                       int32_t h, const uint16_t* data,
                                       uint16_t* copyBuffer = nullptr);
    DmaTransferResult submitPixelsAsync(uint16_t* image, uint32_t len);
    DmaTransferResult lastDmaTransferResult() const { return _lastDmaTransfer; }
    bool dmaBusy();
    void dmaWait();

    // Color utilities

    uint16_t color565(uint8_t red, uint8_t green, uint8_t blue);
    uint16_t color8to16(uint8_t color332);
    uint8_t  color16to8(uint16_t color565);
    uint32_t color16to24(uint16_t color565);
    uint32_t color24to16(uint32_t color888);
    uint16_t alphaBlend(uint8_t alpha, uint16_t fgc, uint16_t bgc);
    uint16_t alphaBlend(uint8_t alpha, uint16_t fgc, uint16_t bgc, uint8_t dither);
    uint32_t alphaBlend24(uint8_t alpha, uint32_t fgc, uint32_t bgc, uint8_t dither = 0);

    // Configuration

    void setAttribute(uint8_t id = 0, uint8_t a = 0);
    uint8_t getAttribute(uint8_t id = 0);

    // Touch

    bool getTouch(int32_t *x, int32_t *y, uint16_t threshold = 600);
    bool getTouch(uint16_t* x, uint16_t* y, uint16_t threshold = 600) {
        if (!x || !y) return false;
        int32_t tx = 0, ty = 0;
        const bool touched = getTouch(&tx, &ty, threshold);
        if (touched) {
            *x = static_cast<uint16_t>(tx < 0 ? 0 : tx);
            *y = static_cast<uint16_t>(ty < 0 ? 0 : ty);
        }
        return touched;
    }
    uint8_t getTouchPoints(TouchPoint* points, uint8_t maxPoints);
    uint8_t getTouchGesture() const;
    uint8_t touchPointCapacity() const;
    bool getTouchRaw(uint16_t *x, uint16_t *y);
    uint16_t getTouchRawZ();
    void convertRawXY(uint16_t *x, uint16_t *y);
    void calibrateTouch(uint16_t *parameters, uint32_t color_fg, uint32_t color_bg, uint8_t size);
    void setTouch(uint16_t *parameters);
    bool attachTouch(ITouch& touch, IBus& bus);
    void detachTouch();

    // Public members

    uint32_t textcolor = 0xFFFF;
    uint32_t textbgcolor = 0x0000;
    uint32_t bitmap_fg = 0xFFFF;
    uint32_t bitmap_bg = 0x0000;

    uint8_t textfont = 1;
    uint8_t textsize = 1;
    uint8_t textdatum = TL_DATUM;
    uint8_t rotation = 0;

    bool DMA_Enabled = false;
    uint8_t spiBusyCheck = 0;

protected:
    IPanel* _panel = nullptr;
    IBoard* _board = nullptr;
    // Stored by value: the display stack components remain dynamically selected,
    // but their RAII owner does not need a separate heap allocation.
    DisplayInstance _instance;
    bool _ownsBoard = false;
    Seeed_Product::Product _pendingProduct = Seeed_Product::CUSTOM;
    Seeed_Product::Product _activeProduct = Seeed_Product::CUSTOM;
    bool _hasPendingProduct = false;
    GfxResult _lastResult;

    // Viewport
    int32_t _vpX, _vpY, _vpW, _vpH;
    int32_t _xDatum, _yDatum;
    int32_t _xWidth, _yHeight;
    bool _vpDatum, _vpOoB;

    // Cursor
    int32_t cursor_x, cursor_y, padX;
    int32_t bg_cursor_x;
    int32_t last_cursor_x;

    // Text state
    uint32_t fontsloaded;
    uint8_t glyph_ab, glyph_bb;
    bool isDigits;
    bool textwrapX, textwrapY;
    bool _swapBytes;
    DmaTransferResult _lastDmaTransfer = DmaTransferResult::Unsupported;
    bool _booted;
    bool _cp437, _utf8, _psram_enable;
    uint32_t _utf8Codepoint = 0;
    uint8_t _utf8BytesRemaining = 0;
    uint32_t _lastColor;
    bool _fillbg;

    // Pivot
    int16_t _xPivot, _yPivot;

    // Font
    GFXfont *gfxFont;
    getColorCallback getColor = nullptr;
    SmoothFont _smoothFont;

    // Font rendering helpers
    static uint16_t readSmoothFontPixel(int32_t x, int32_t y);
    void drawCharGlcd(int32_t x, int32_t y, uint16_t c, uint32_t color, uint32_t bg, uint8_t size);
    void drawCharGfx(int32_t x, int32_t y, uint16_t c, uint32_t color, uint32_t bg, uint8_t size);

    // Touch
    ITouch* _touch = nullptr;
    uint16_t touchCalibration_x0, touchCalibration_x1;
    uint16_t touchCalibration_y0, touchCalibration_y1;
    bool touchCalibration_rotate, touchCalibration_invert_x, touchCalibration_invert_y;

    // Internal helpers
    template<typename PanelConfigType>
    static auto configuredDriverWidth(int)
        -> decltype(static_cast<uint16_t>(PanelConfigType::storageWidth)) {
        return static_cast<uint16_t>(PanelConfigType::storageWidth);
    }

    template<typename PanelConfigType>
    static uint16_t configuredDriverWidth(long) {
        return static_cast<uint16_t>(PanelConfigType::width);
    }

    template<typename PanelConfigType>
    static auto configuredDriverHeight(int)
        -> decltype(static_cast<uint16_t>(PanelConfigType::storageHeight)) {
        return static_cast<uint16_t>(PanelConfigType::storageHeight);
    }

    template<typename PanelConfigType>
    static uint16_t configuredDriverHeight(long) {
        return static_cast<uint16_t>(PanelConfigType::height);
    }

    template<typename PanelConfigType>
    static auto createConfiguredDriver(int)
        -> decltype(static_cast<IDriver*>(new (std::nothrow)
            typename PanelConfigType::Driver(configuredDriverWidth<PanelConfigType>(0),
                                             configuredDriverHeight<PanelConfigType>(0),
                                             PanelConfigType::rgbOrder))) {
        return new (std::nothrow) typename PanelConfigType::Driver(
            configuredDriverWidth<PanelConfigType>(0),
            configuredDriverHeight<PanelConfigType>(0),
            PanelConfigType::rgbOrder);
    }

    template<typename PanelConfigType>
    static IDriver* createConfiguredDriver(long) {
        return new (std::nothrow) typename PanelConfigType::Driver(
            configuredDriverWidth<PanelConfigType>(0),
            configuredDriverHeight<PanelConfigType>(0));
    }

    template<typename PanelConfigType>
    static auto configurePanelGeometryForConfig(IPanel& panel, int)
        -> decltype(static_cast<uint16_t>(PanelConfigType::storageWidth),
                    GfxResult()) {
        return panel.configureVisibleArea(
            static_cast<uint16_t>(PanelConfigType::width),
            static_cast<uint16_t>(PanelConfigType::height));
    }

    template<typename PanelConfigType>
    static GfxResult configurePanelGeometryForConfig(IPanel&, long) {
        return GfxResult::success();
    }

    template<typename PanelConfigType>
    static auto configurePanelForConfig(IPanel& panel, int)
        -> decltype(PanelConfigType::panelMode(), GfxResult()) {
        return panel.configure(PanelConfigType::panelMode());
    }

    template<typename PanelConfigType>
    static GfxResult configurePanelForConfig(IPanel&, long) {
        return GfxResult::success();
    }

    template<typename PanelConfigType>
    static auto configInitialRotation(int)
        -> decltype(static_cast<uint8_t>(PanelConfigType::initialRotation)) {
        return static_cast<uint8_t>(PanelConfigType::initialRotation % 4);
    }

    template<typename PanelConfigType>
    static uint8_t configInitialRotation(long) { return 0; }

    // Apply horizontal mirror declared by the panel config (e.g. the 4.26"
    // ePaper glass). Decoupled from the adapter board so the same config
    // works across EE04/EE05/EN04/EN05. No-op when the config has no mirror
    // field (TFT and non-mirrored ePaper).
    template<typename PanelConfigType>
    static auto applyMirrorFromConfig(IPanel* panel, int)
        -> decltype(PanelConfigType::mirror, void()) {
        if (panel) {
            static_cast<typename PanelConfigType::Panel*>(panel)
                ->setHorizontalMirror(PanelConfigType::mirror);
        }
    }

    template<typename PanelConfigType>
    static void applyMirrorFromConfig(IPanel*, long) {}

    // Breakout correction is panel-specific. A generic Breakout board must not
    // flip UC8179 and other panel families that mount normally.
    template<typename BoardType, typename PanelConfigType>
    static auto applyBreakoutMirrorPolicy(IPanel* panel, int)
        -> decltype(BoardType::usesPanelConfigBreakoutMirror,
                    PanelConfigType::breakoutDisplayHorizontalMirror, void()) {
        if (panel && BoardType::usesPanelConfigBreakoutMirror) {
            static_cast<typename PanelConfigType::Panel*>(panel)
                ->setDisplayHorizontalMirror(
                    PanelConfigType::breakoutDisplayHorizontalMirror);
        }
    }

    template<typename BoardType, typename PanelConfigType>
    static void applyBreakoutMirrorPolicy(IPanel*, long) {}

    template<typename BoardType, typename PanelConfigType>
    static auto applyBreakoutVerticalMirrorPolicy(IPanel* panel, int)
        -> decltype(BoardType::usesPanelConfigBreakoutMirror,
                    PanelConfigType::breakoutDisplayVerticalMirror, void()) {
        if (panel && BoardType::usesPanelConfigBreakoutMirror) {
            static_cast<typename PanelConfigType::Panel*>(panel)
                ->setDisplayVerticalMirror(
                    PanelConfigType::breakoutDisplayVerticalMirror);
        }
    }

    template<typename BoardType, typename PanelConfigType>
    static void applyBreakoutVerticalMirrorPolicy(IPanel*, long) {}

    // Apply inversion declared by the panel config. Driver_ST7789::init() forces
    // INVON for every panel, but some panels need inversion OFF for correct
    // colors (e.g. the 1.47" JD9853A). Called after setRotation in begin<> so it
    // is the final inversion state, overriding the driver init-time INVON. No-op
    // when the config has no invert field (TFT configs that match the driver
    // default, and all ePaper).
    template<typename PanelConfigType>
    static auto applyInversionFromConfig(IPanel* panel, int)
        -> decltype(PanelConfigType::invert, void()) {
        if (panel) panel->invertDisplay(PanelConfigType::invert);
    }
    template<typename PanelConfigType>
    static void applyInversionFromConfig(IPanel*, long) {}

    void _beginWrite();
    void _endWrite();
    void _setAddrWindow(int32_t x, int32_t y, int32_t w, int32_t h);
    void releaseOwnedHardware();
};

#endif // SEEED_GFX_H
