#ifndef SEEED_GFX_SPRITE_H
#define SEEED_GFX_SPRITE_H

#include <Arduino.h>
#include <Print.h>
#include "core/Color.h"
#include "core/Font.h"
#include "font/Font_Smooth.h"

class Seeed_GFX;

/** RAM-backed drawing surface compatible with TFT_eSPI's sprite workflow. */
class Seeed_Sprite : public Print {
public:
    Seeed_Sprite();
    virtual ~Seeed_Sprite();

    void* createSprite(Seeed_GFX* gfx, int16_t width, int16_t height, uint8_t frames = 1);
    void* createSprite(Seeed_GFX& gfx, int16_t width, int16_t height, uint8_t frames = 1) {
        return createSprite(&gfx, width, height, frames);
    }
    void deleteSprite();
    bool created() const { return _storage != nullptr; }
    void* getPointer() { return frameBuffer(_frame); }
    void* frameBuffer(int8_t frame);
    void* setColorDepth(int8_t b);
    int8_t getColorDepth() const { return _bpp; }

    void createPalette(uint16_t* palette = nullptr, uint8_t colors = 16);
    void createPalette(const uint16_t* palette, uint8_t colors = 16);
    void setPaletteColor(uint8_t index, uint16_t color);
    uint16_t getPaletteColor(uint8_t index) const;
    void setBitmapColor(uint16_t fg, uint16_t bg) {
        _bitmapFg = fg;
        _bitmapBg = (fg == bg) ? static_cast<uint16_t>(~fg) : bg;
    }

    int16_t width() const { return (_rotation & 1) ? _iheight : _iwidth; }
    int16_t height() const { return (_rotation & 1) ? _iwidth : _iheight; }
    bool pushSprite(int32_t x, int32_t y);
    bool pushSprite(int32_t x, int32_t y, uint16_t transparent);
    bool pushSprite(int32_t tx, int32_t ty, int32_t sx, int32_t sy, int32_t sw, int32_t sh);
    bool pushToSprite(Seeed_Sprite* destination, int32_t x, int32_t y);
    bool pushToSprite(Seeed_Sprite* destination, int32_t x, int32_t y, uint16_t transparent);

    void drawPixel(int32_t x, int32_t y, uint32_t color);
    void drawLine(int32_t xs, int32_t ys, int32_t xe, int32_t ye, uint32_t color);
    void drawFastVLine(int32_t x, int32_t y, int32_t h, uint32_t color);
    void drawFastHLine(int32_t x, int32_t y, int32_t w, uint32_t color);
    void drawRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color);
    void fillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color);
    void fillScreen(uint32_t color);
    void fillSprite(uint32_t color) { fillScreen(color); }
    void drawCircle(int32_t x, int32_t y, int32_t r, uint32_t color);
    void fillCircle(int32_t x, int32_t y, int32_t r, uint32_t color);
    void drawTriangle(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t color);
    void fillTriangle(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t color);
    void drawRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint32_t color);
    void fillRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint32_t color);
    void drawEllipse(int32_t x, int32_t y, int32_t rx, int32_t ry, uint32_t color);
    void fillEllipse(int32_t x, int32_t y, int32_t rx, int32_t ry, uint32_t color);
    void drawRectVGradient(int16_t x, int16_t y, int16_t w, int16_t h,
                           uint32_t color1, uint32_t color2);
    void drawRectHGradient(int16_t x, int16_t y, int16_t w, int16_t h,
                           uint32_t color1, uint32_t color2);
    void fillRectVGradient(int16_t x, int16_t y, int16_t w, int16_t h,
                           uint32_t color1, uint32_t color2) {
        drawRectVGradient(x, y, w, h, color1, color2);
    }
    void fillRectHGradient(int16_t x, int16_t y, int16_t w, int16_t h,
                           uint32_t color1, uint32_t color2) {
        drawRectHGradient(x, y, w, h, color1, color2);
    }
    uint16_t drawPixel(int32_t x, int32_t y, uint32_t color, uint8_t alpha,
                       uint32_t bgColor = 0x00FFFFFF);
    void drawSmoothArc(int32_t x, int32_t y, int32_t r, int32_t ir,
                       uint32_t startAngle, uint32_t endAngle,
                       uint32_t fgColor, uint32_t bgColor,
                       bool roundEnds = false);
    void drawArc(int32_t x, int32_t y, int32_t r, int32_t ir,
                 uint32_t startAngle, uint32_t endAngle, uint32_t fgColor,
                 uint32_t bgColor, bool smoothArc = true);
    void drawSmoothCircle(int32_t x, int32_t y, int32_t r,
                          uint32_t fgColor, uint32_t bgColor);
    void fillSmoothCircle(int32_t x, int32_t y, int32_t r, uint32_t color,
                          uint32_t bgColor = 0x00FFFFFF);
    void drawSmoothRoundRect(int32_t x, int32_t y, int32_t r, int32_t ir,
                             int32_t w, int32_t h, uint32_t fgColor,
                             uint32_t bgColor = 0x00FFFFFF,
                             uint8_t quadrants = 0x0F);
    void fillSmoothRoundRect(int32_t x, int32_t y, int32_t w, int32_t h,
                             int32_t radius, uint32_t color,
                             uint32_t bgColor = 0x00FFFFFF);
    void drawSpot(float x, float y, float radius, uint32_t fgColor,
                  uint32_t bgColor = 0x00FFFFFF);
    void drawWideLine(float x0, float y0, float x1, float y1, float width,
                      uint32_t fgColor, uint32_t bgColor = 0x00FFFFFF);
    void drawWedgeLine(float x0, float y0, float x1, float y1,
                       float startWidth, float endWidth, uint32_t fgColor,
                       uint32_t bgColor = 0x00FFFFFF);
    uint16_t readPixel(int32_t x, int32_t y);
    uint16_t readPixelValue(int32_t x, int32_t y);

    void setSwapBytes(bool swap) { _swapBytes = swap; }
    bool getSwapBytes() const { return _swapBytes; }
    void drawBitmap(int16_t x, int16_t y, const uint8_t* bitmap,
                    int16_t w, int16_t h, uint16_t fgColor);
    void drawBitmap(int16_t x, int16_t y, const uint8_t* bitmap,
                    int16_t w, int16_t h, uint16_t fgColor,
                    uint16_t bgColor);
    void drawXBitmap(int16_t x, int16_t y, const uint8_t* bitmap,
                     int16_t w, int16_t h, uint16_t fgColor);
    void drawXBitmap(int16_t x, int16_t y, const uint8_t* bitmap,
                     int16_t w, int16_t h, uint16_t fgColor,
                     uint16_t bgColor);
    void readRect(int32_t x, int32_t y, int32_t w, int32_t h,
                  uint16_t* data);
    void pushRect(int32_t x, int32_t y, int32_t w, int32_t h,
                  const uint16_t* data);
    void setWindow(int32_t x0, int32_t y0, int32_t x1, int32_t y1);
    void pushColor(uint16_t color);
    void pushColor(uint16_t color, uint32_t len);
    void writeColor(uint16_t color) { pushColor(color); }
    void pushImage(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t* data);
    void pushImage(int32_t x, int32_t y, int32_t w, int32_t h,
                   const uint16_t* data, uint16_t transparent);
    void pushImage(int32_t x, int32_t y, int32_t w, int32_t h,
                   const uint8_t* data, bool bpp8,
                   const uint16_t* colorMap = nullptr);
    void pushImage(int32_t x, int32_t y, int32_t w, int32_t h,
                   const uint8_t* data, uint8_t transparent, bool bpp8,
                   const uint16_t* colorMap = nullptr);
    bool pushImage4BPP(int32_t x, int32_t y, int32_t w, int32_t h,
                       const uint8_t* data,
                       const uint16_t* colorMap = nullptr);
    void pushMaskedImage(int32_t x, int32_t y, int32_t w, int32_t h,
                         const uint16_t* image, const uint8_t* mask);

    void setScrollRect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t color = TFT_BLACK);
    void scroll(int16_t dx, int16_t dy = 0);
    void setRotation(uint8_t r);
    uint8_t getRotation() const { return _rotation; }
    bool pushRotated(int16_t angle, uint32_t transparent = 0x00FFFFFF);
    bool pushRotated(Seeed_Sprite* destination, int16_t angle, uint32_t transparent = 0x00FFFFFF);
    bool getRotatedBounds(int16_t angle, int16_t* minX, int16_t* minY, int16_t* maxX, int16_t* maxY);
    bool getRotatedBounds(Seeed_Sprite* destination, int16_t angle, int16_t* minX, int16_t* minY, int16_t* maxX, int16_t* maxY);
    void setPivot(int16_t x, int16_t y) { _pivotX = x; _pivotY = y; _hasPivot = true; }
    int16_t getPivotX() const { return _pivotX; }
    int16_t getPivotY() const { return _pivotY; }

    void setViewport(int32_t x, int32_t y, int32_t w, int32_t h, bool vpDatum = true);
    bool checkViewport(int32_t x, int32_t y, int32_t w, int32_t h);
    void resetViewport();
    int32_t getViewportX() const { return _vpX; }
    int32_t getViewportY() const { return _vpY; }
    int32_t getViewportWidth() const { return _vpW - _vpX; }
    int32_t getViewportHeight() const { return _vpH - _vpY; }
    bool getViewportDatum() const { return _vpDatum; }
    void frameViewport(uint16_t color, int32_t width = 1);
    void setOrigin(int32_t x, int32_t y) { _xDatum = x; _yDatum = y; }
    int32_t getOriginX() const { return _xDatum; }
    int32_t getOriginY() const { return _yDatum; }

    int16_t drawString(const char* str, int32_t x, int32_t y, uint8_t font);
    int16_t drawString(const char* str, int32_t x, int32_t y);
    int16_t drawString(const String& str, int32_t x, int32_t y, uint8_t font) {
        return drawString(str.c_str(), x, y, font);
    }
    int16_t drawString(const String& str, int32_t x, int32_t y) { return drawString(str.c_str(), x, y); }
    int16_t drawNumber(long intNumber, int32_t x, int32_t y, uint8_t font);
    int16_t drawNumber(long intNumber, int32_t x, int32_t y) {
        return drawNumber(intNumber, x, y, _textFont);
    }
    int16_t drawFloat(float floatNumber, uint8_t decimal, int32_t x, int32_t y, uint8_t font);
    int16_t drawFloat(float floatNumber, uint8_t decimal, int32_t x, int32_t y) {
        return drawFloat(floatNumber, decimal, x, y, _textFont);
    }
    int16_t drawCentreString(const char* str, int32_t x, int32_t y, uint8_t font);
    int16_t drawRightString(const char* str, int32_t x, int32_t y, uint8_t font);
    int16_t drawCentreString(const String& str, int32_t x, int32_t y,
                             uint8_t font) {
        return drawCentreString(str.c_str(), x, y, font);
    }
    int16_t drawRightString(const String& str, int32_t x, int32_t y,
                            uint8_t font) {
        return drawRightString(str.c_str(), x, y, font);
    }
    int16_t drawChar(uint16_t code, int32_t x, int32_t y, uint8_t font);
    int16_t drawChar(uint16_t code, int32_t x, int32_t y) {
        return drawChar(code, x, y, _textFont);
    }
    void drawChar(int32_t x, int32_t y, uint16_t code, uint32_t color,
                  uint32_t background, uint8_t size);
    uint16_t drawGlyph(uint16_t code);
    void printToSprite(const String& text) { print(text); }
    void printToSprite(const char* text, uint16_t len);
    void setCursor(int16_t x, int16_t y);
    int16_t getCursorX() const { return _cursorX; }
    int16_t getCursorY() const { return _cursorY; }
    void setTextColor(uint16_t color);
    void setTextColor(uint16_t fg, uint16_t bg);
    void setTextSize(uint8_t size);
    void setTextFont(uint8_t font);
    void setFreeFont(const GFXfont* font = nullptr);
    void setTextDatum(uint8_t datum);
    uint8_t getTextDatum() const { return _textDatum; }
    void setTextWrap(bool wrapX, bool wrapY = false);
    void setTextPadding(uint16_t width) { _textPadding = width; }
    uint16_t getTextPadding() const { return _textPadding; }
    int16_t textWidth(const char* str);
    int16_t textWidth(const char* str, uint8_t font);
    int16_t fontHeight();
    int16_t fontHeight(uint8_t font);
    bool loadFont(const uint8_t* fontData);
#if SEEED_GFX_HAS_FS
    bool loadFont(const char* path, fs::FS& fileSystem);
    bool loadFont(const String& path, fs::FS& fileSystem) {
        return loadFont(path.c_str(), fileSystem);
    }
#endif
    void unloadFont();
    bool smoothFontLoaded() const { return _smoothFont.isLoaded(); }
    size_t write(uint8_t c) override;

private:
    size_t bytesPerFrame() const;
    uint8_t* activeData() const;
    bool logicalToPhysical(int32_t& x, int32_t& y) const;
    uint8_t paletteIndex(uint16_t color) const;
    uint16_t storedColor(size_t pixel) const;
    uint16_t storedValue(size_t pixel) const;
    void setStoredColor(size_t pixel, uint16_t color);
    uint16_t readLogicalColor(int32_t x, int32_t y) const;
    uint16_t readLogicalValue(int32_t x, int32_t y) const;
    bool prepareRenderer(Seeed_GFX& renderer) const;
    bool copyRotatedTo(Seeed_Sprite* destination, int16_t angle, uint32_t transparent);

    int16_t _iwidth = 0, _iheight = 0;
    int8_t _bpp = 16;
    uint8_t _rotation = 0, _frames = 1, _frame = 0;
    uint8_t* _storage = nullptr;
    uint16_t _palette[16] = {0};
    uint16_t _bitmapFg = TFT_WHITE, _bitmapBg = TFT_BLACK;
    Seeed_GFX* _gfx = nullptr;
    int32_t _vpX = 0, _vpY = 0, _vpW = 0, _vpH = 0, _xDatum = 0, _yDatum = 0;
    bool _vpDatum = false, _vpOoB = false;
    int32_t _cursorX = 0, _cursorY = 0;
    uint16_t _textColor = TFT_WHITE, _textBgColor = TFT_BLACK;
    uint8_t _textSize = 1, _textFont = FONT_GLCD, _textDatum = TL_DATUM;
    uint16_t _textPadding = 0;
    const GFXfont* _gfxFont = nullptr;
    SmoothFont _smoothFont;
    bool _fillTextBackground = false;
    bool _textWrapX = true, _textWrapY = false;
    bool _swapBytes = false;
    uint32_t _utf8Codepoint = 0;
    uint8_t _utf8BytesRemaining = 0;
    int32_t _winX0 = 0, _winY0 = 0, _winX1 = -1, _winY1 = -1, _winX = 0, _winY = 0;
    int32_t _scrollX = 0, _scrollY = 0, _scrollW = 0, _scrollH = 0;
    uint16_t _scrollColor = TFT_BLACK;
    int16_t _pivotX = 0, _pivotY = 0;
    bool _hasPivot = false;
};

using TFT_eSprite = Seeed_Sprite;

#endif
