/**
 * @file   Font_Smooth.cpp
 * @brief  Smooth (anti-aliased) font implementation for Seeed_GFX v2.0
 *
 * Reads VLW-format font data from PROGMEM and renders glyphs using
 * callback-based pixel drawing with alpha blending.
 *
 * The VLW font format (undocumented, reverse-engineered):
 *   Header: 6 x uint32_t (24 bytes total)
 *     1. gCount (number of glyphs)
 *     2. Version number (typically 0x0B = 11)
 *     3. Font size in points (not pixels)
 *     4. Deprecated mboxY (typically 0)
 *     5. Ascent (top of "d" above baseline)
 *     6. Descent (bottom of "p" below baseline)
 *
 *   Glyph records: gCount x 7 x int32_t (28 bytes each)
 *     1. Unicode code point
 *     2. Bitmap height
 *     3. Bitmap width
 *     4. xAdvance (cursor advance)
 *     5. dY (distance from baseline to top of bitmap, +ve = up)
 *     6. dX (offset from cursor to left of bitmap, -ve = left)
 *     7. Padding (typically 0)
 *
 *   Bitmaps: one byte per pixel (8-bit alpha), stored row by row
 *     0x00 = fully transparent (background)
 *     0xFF = fully opaque (foreground)
 *
 * Original code by Bodmer (TFT_eSPI), adapted for Seeed_GFX v2.0.
 */

#include "Font_Smooth.h"

// Constructor / Destructor

SmoothFont::SmoothFont() {
}

SmoothFont::~SmoothFont() {
    unloadFont();
}

// Initialization

void SmoothFont::begin(DrawPixelCallback drawPixel,
                       DrawHLineCallback drawHLine,
                       FillRectCallback fillRect,
                       ReadPixelCallback readPixel) {
    _drawPixel = drawPixel;
    _drawHLine = drawHLine;
    _fillRect  = fillRect;
    _readPixel = readPixel;
}

// Font loading

bool SmoothFont::loadFont(const uint8_t* fontData) {
    if (fontData == nullptr) return false;

    // Unload any previously loaded font
    if (_fontLoaded) unloadFont();

    _fontPtr = fontData;

    // Read font header (6 x uint32_t = 24 bytes)
    _gFont.gArray   = (const uint8_t*)_fontPtr;
    _gFont.gCount   = (uint16_t)readInt32();  // glyph count
                          readInt32();         // vlw encoder version - discard
    _gFont.yAdvance = (uint16_t)readInt32();  // font size in points
                          readInt32();         // deprecated mboxY - discard
    _gFont.ascent   = (uint16_t)readInt32();  // top of "d"
    _gFont.descent  = (uint16_t)readInt32();  // bottom of "p"

    // Initialize derived values
    _gFont.maxAscent  = _gFont.ascent;
    _gFont.maxDescent = _gFont.descent;
    _gFont.yAdvance   = _gFont.ascent + _gFont.descent;
    _gFont.spaceWidth = _gFont.yAdvance / 4;  // Guess at space width

    if (_gFont.gCount == 0 || _gFont.gCount > 4096 ||
        (_gFont.ascent == 0 && _gFont.descent == 0)) {
        unloadFont();
        return false;
    }
    _fontLoaded = true;

    // Fetch per-glyph metrics
    return loadMetrics();
}

#if SEEED_GFX_HAS_FS
bool SmoothFont::loadFont(const char* path, fs::FS& fileSystem) {
    if (!path || !*path) return false;
    File file = fileSystem.open(path, "r");
    if (!file) return false;
    const size_t size = static_cast<size_t>(file.size());
    if (size < 52U) { file.close(); return false; }
    uint8_t* data = static_cast<uint8_t*>(malloc(size));
    if (!data) { file.close(); return false; }
    const size_t read = file.read(data, size);
    file.close();
    if (read != size || !loadFont(static_cast<const uint8_t*>(data))) {
        free(data);
        return false;
    }
    _ownedFontData = data;
    _ownedFontSize = size;
    return true;
}
#endif

void SmoothFont::unloadFont() {
    if (_gUnicode)  { free(_gUnicode);  _gUnicode  = nullptr; }
    if (_gHeight)   { free(_gHeight);   _gHeight   = nullptr; }
    if (_gWidth)    { free(_gWidth);    _gWidth    = nullptr; }
    if (_gxAdvance) { free(_gxAdvance); _gxAdvance = nullptr; }
    if (_gdY)       { free(_gdY);       _gdY       = nullptr; }
    if (_gdX)       { free(_gdX);       _gdX       = nullptr; }
    if (_gBitmap)   { free(_gBitmap);   _gBitmap   = nullptr; }
    if (_ownedFontData) {
        free(_ownedFontData);
        _ownedFontData = nullptr;
        _ownedFontSize = 0;
    }

    _gFont.gArray = nullptr;
    _fontPtr = nullptr;
    _fontLoaded = false;
}

// loadMetrics - Read per-glyph metrics from font data

bool SmoothFont::loadMetrics() {
    uint32_t headerPtr = 24;
    uint32_t bitmapPtr = headerPtr + _gFont.gCount * 28;

    // Allocate metric arrays
    _gUnicode  = (uint16_t*)malloc(_gFont.gCount * 2);  // 2 bytes each
    _gHeight   = (uint8_t*)malloc(_gFont.gCount);        // 1 byte each
    _gWidth    = (uint8_t*)malloc(_gFont.gCount);        // 1 byte each
    _gxAdvance = (uint8_t*)malloc(_gFont.gCount);        // 1 byte each
    _gdY       = (int16_t*)malloc(_gFont.gCount * 2);    // 2 bytes each
    _gdX       = (int8_t*)malloc(_gFont.gCount);         // 1 byte each
    _gBitmap   = (uint32_t*)malloc(_gFont.gCount * 4);   // 4 bytes each

    if (!_gUnicode || !_gHeight || !_gWidth || !_gxAdvance ||
        !_gdY || !_gdX || !_gBitmap) {
        // Allocation failed - free what we got and return
        unloadFont();
        return false;
    }

    // Reset font data pointer to start of glyph records
    _fontPtr = _gFont.gArray + headerPtr;

    for (uint16_t gNum = 0; gNum < _gFont.gCount; gNum++) {
        _gUnicode[gNum]  = (uint16_t)readInt32();  // Unicode code point
        _gHeight[gNum]   = (uint8_t)readInt32();   // Glyph bitmap height
        _gWidth[gNum]    = (uint8_t)readInt32();   // Glyph bitmap width
        _gxAdvance[gNum] = (uint8_t)readInt32();   // Cursor x advance
        _gdY[gNum]       = (int16_t)readInt32();   // y delta from baseline
        _gdX[gNum]       = (int8_t)readInt32();    // x delta from cursor
        readInt32(); // padding - ignored

        // Track maximum descent across all glyphs
        if (((int16_t)_gHeight[gNum] - (int16_t)_gdY[gNum]) > _gFont.maxDescent) {
            // Avoid non-printable characters that give bad values
            if (((_gUnicode[gNum] > 0x20) && (_gUnicode[gNum] < 0xA0) &&
                 (_gUnicode[gNum] != 0x7F)) || (_gUnicode[gNum] > 0xFF)) {
                _gFont.maxDescent = _gHeight[gNum] - _gdY[gNum];
            }
        }

        _gBitmap[gNum] = bitmapPtr;
        bitmapPtr += _gWidth[gNum] * _gHeight[gNum];

        yield();
    }

    // Update metrics based on actual glyph data
    _gFont.yAdvance   = _gFont.maxAscent + _gFont.maxDescent;
    _gFont.spaceWidth = (_gFont.ascent + _gFont.descent) * 2 / 7;
    return true;
}

// readInt32 - Read a 32-bit big-endian value from font data

uint32_t SmoothFont::readInt32() {
    uint32_t val = 0;
    val  = (uint32_t)pgm_read_byte(_fontPtr++) << 24;
    val |= (uint32_t)pgm_read_byte(_fontPtr++) << 16;
    val |= (uint32_t)pgm_read_byte(_fontPtr++) << 8;
    val |= (uint32_t)pgm_read_byte(_fontPtr++);
    return val;
}

// getUnicodeIndex - Find glyph index for a Unicode character

bool SmoothFont::getUnicodeIndex(uint16_t unicode, uint16_t* index) {
    if (!_fontLoaded) return false;

    for (uint16_t i = 0; i < _gFont.gCount; i++) {
        if (_gUnicode[i] == unicode) {
            *index = i;
            return true;
        }
    }
    return false;
}

// alphaBlend - Blend foreground and background colors

uint16_t SmoothFont::alphaBlend(uint8_t alpha, uint16_t fgc, uint16_t bgc) {
    // Divide by 255 so alpha endpoints remain exact.
    uint16_t fgR = ((fgc >> 11) & 0x1F) * alpha;
    uint16_t fgG = ((fgc >> 5)  & 0x3F) * alpha;
    uint16_t fgB = (fgc         & 0x1F) * alpha;

    uint16_t bgR = ((bgc >> 11) & 0x1F) * (255 - alpha);
    uint16_t bgG = ((bgc >> 5)  & 0x3F) * (255 - alpha);
    uint16_t bgB = (bgc         & 0x1F) * (255 - alpha);

    uint16_t r = ((fgR + bgR) / 255U) & 0x1F;
    uint16_t g = ((fgG + bgG) / 255U) & 0x3F;
    uint16_t b = ((fgB + bgB) / 255U) & 0x1F;

    return (r << 11) | (g << 5) | b;
}

// drawChar - Basic glyph rendering (no background fill)

uint16_t SmoothFont::drawChar(int32_t x, int32_t y, uint16_t ch,
                               uint16_t fgColor, uint16_t bgColor) {
    return drawChar(x, y, ch, fgColor, bgColor, false);
}

// drawChar - Full glyph rendering with optional background fill

uint16_t SmoothFont::drawChar(int32_t x, int32_t y, uint16_t ch,
                               uint16_t fgColor, uint16_t bgColor,
                               bool fillBg) {
    if (!_fontLoaded) return 0;

    // Handle control characters
    if (ch < 0x21) {
        if (ch == 0x20) {
            // An opaque space must erase its advance cell as well; this is
            // required when a padded value becomes shorter or contains gaps.
            if (fillBg && _fillRect)
                _fillRect(x, y, _gFont.spaceWidth, _gFont.yAdvance, bgColor);
            return _gFont.spaceWidth;
        }
        if (ch == '\n') {
            return 0; // Newline - caller handles line advance
        }
        return 0;
    }

    // Find the glyph
    uint16_t gNum = 0;
    if (!getUnicodeIndex(ch, &gNum)) {
        // Character not found in font
        return _gFont.spaceWidth + 1;
    }

    // Calculate glyph position
    int16_t glyphX = x + _gdX[gNum];
    int16_t glyphY = y + _gFont.maxAscent - _gdY[gNum];

    const uint8_t* gPtr = (const uint8_t*)_gFont.gArray;

    // Clear the complete advance cell before drawing. Clearing only the
    // margins around the glyph leaves zero-alpha holes from the previous
    // character behind when changing numeric values in place.
    if (fillBg && _fillRect) {
        _fillRect(x, y, _gxAdvance[gNum], _gFont.yAdvance, bgColor);
        const int16_t glyphRight = glyphX + _gWidth[gNum];
        const int16_t cursorRight = x + _gxAdvance[gNum];
        if (glyphRight > cursorRight)
            _fillRect(cursorRight, glyphY, glyphRight - cursorRight,
                      _gHeight[gNum], bgColor);
    }

    // Render the glyph bitmap
    if (_drawPixel) {
        for (int32_t row = 0; row < _gHeight[gNum]; row++) {
            int32_t pixelY = glyphY + row;

            for (int32_t col = 0; col < _gWidth[gNum]; col++) {
                int32_t pixelX = glyphX + col;

                // Read alpha value from font data
                uint8_t alpha = pgm_read_byte(
                    gPtr + _gBitmap[gNum] + col + _gWidth[gNum] * row);

                if (alpha == 0) {
                    // Fully transparent - skip
                    continue;
                }

                if (alpha == 0xFF) {
                    // Fully opaque - draw foreground color directly
                    _drawPixel(pixelX, pixelY, fgColor);
                } else {
                    // Semi-transparent - alpha blend
                    uint16_t bg = bgColor;
                    if (_readPixel && !fillBg) {
                        bg = _readPixel(pixelX, pixelY);
                    }
                    _drawPixel(pixelX, pixelY, alphaBlend(alpha, fgColor, bg));
                }
            }
        }
    }

    return _gxAdvance[gNum];
}

// Glyph metric queries

uint16_t SmoothFont::getCharWidth(uint16_t ch) {
    if (!_fontLoaded) return 0;

    if (ch < 0x21) {
        if (ch == 0x20) return _gFont.spaceWidth;
        return 0;
    }

    uint16_t gNum = 0;
    if (getUnicodeIndex(ch, &gNum)) {
        return _gxAdvance[gNum];
    }

    return _gFont.spaceWidth;
}

uint8_t SmoothFont::getCharBitmapWidth(uint16_t ch) {
    if (!_fontLoaded) return 0;

    uint16_t gNum = 0;
    if (getUnicodeIndex(ch, &gNum)) {
        return _gWidth[gNum];
    }
    return 0;
}

uint8_t SmoothFont::getCharBitmapHeight(uint16_t ch) {
    if (!_fontLoaded) return 0;

    uint16_t gNum = 0;
    if (getUnicodeIndex(ch, &gNum)) {
        return _gHeight[gNum];
    }
    return 0;
}

// showFont - Display all glyphs for debugging

void SmoothFont::showFont(uint16_t screenWidth, uint16_t screenHeight,
                          uint16_t fgColor, uint16_t bgColor,
                          uint32_t pageDelay) {
    if (!_fontLoaded) return;

    // Force a new page at the first character, matching TFT_eSPI behaviour.
    int16_t cursorX = screenWidth;
    int16_t cursorY = screenHeight;
    uint32_t timeDelay = 0;

    for (uint16_t i = 0; i < _gFont.gCount; i++) {
        // Check if this glyph needs a new line
        if (cursorX + _gdX[i] + _gWidth[i] >= (int16_t)screenWidth) {
            cursorX = -_gdX[i];
            cursorY += _gFont.yAdvance;

            // Check if we need a new page (screen full)
            if (cursorY + (int16_t)_gFont.maxAscent + (int16_t)_gFont.descent >= (int16_t)screenHeight) {
                cursorX = -_gdX[i];
                cursorY = 0;
                if (timeDelay) delay(timeDelay);
                timeDelay = pageDelay;
                if (_fillRect) {
                    _fillRect(0, 0, screenWidth, screenHeight, bgColor);
                }
            }
        }

        drawChar(cursorX, cursorY, _gUnicode[i], fgColor, bgColor);
        cursorX += _gxAdvance[i];
        yield();
    }

    // Delay after the last page and clear the screen so subsequent text
    // starts on a blank background, matching the original TFT_eSPI demo.
    if (timeDelay) delay(timeDelay);
    if (_fillRect) {
        _fillRect(0, 0, screenWidth, screenHeight, bgColor);
    }
}
