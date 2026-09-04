/**
 * @file   Seeed_GFX.cpp
 * @brief  Main graphics library implementation for Seeed_GFX v2.0
 *
 * Adapted from TFT_eSPI.cpp drawing functions.
 * All hardware access goes through the IPanel interface.
 */

#include "Seeed_GFX.h"
#include <stdio.h>
#include <algorithm>
#include <math.h>

// The Seeed nRF52 and SAMD cores' pgm_read_ptr return const void*, but the
// TFT_eSPI-derived font code casts to non-const T*. Keep ESP32 and every
// other platform on the original pgm_read_ptr path so their behavior remains
// byte-for-byte unchanged.
#if defined(ARDUINO_ARCH_NRF52) || defined(ARDUINO_ARCH_SAMD)
#define SEEED_PGM_PTR(addr) (const_cast<void*>(pgm_read_ptr(addr)))
#else
#define SEEED_PGM_PTR(addr) pgm_read_ptr(addr)
#endif

#include "runtime/ProductCatalog.h"
#include "font/Font16.h"
#include "font/Font32rle.h"
#include "font/Font64rle.h"
#include "font/Font7srle.h"
#include "font/Font72rle.h"

#if !defined(ARDUINO_ARCH_NRF52)
// BMP decode lives in the UI image layer. Seeed_GFX exposes it on the facade
// so sketches can render an in-memory BMP via data -> UiMemorySource ->
// BmpDecoder -> UiGfxImageSink -> this->pushImage, without the UI widget tree.
#include "ui/image/UiImageDecoders.h"
#endif

using std::swap;

namespace {
Seeed_GFX* smoothFontTarget = nullptr;
void drawSmoothPixel(int32_t x, int32_t y, uint16_t color) {
    if (smoothFontTarget) smoothFontTarget->drawPixel(x, y, color);
}
void drawSmoothHLine(int32_t x, int32_t y, int32_t width, uint16_t color) {
    if (smoothFontTarget) smoothFontTarget->drawFastHLine(x, y, width, color);
}
void fillSmoothRect(int32_t x, int32_t y, int32_t width, int32_t height, uint16_t color) {
    if (smoothFontTarget) smoothFontTarget->fillRect(x, y, width, height, color);
}
struct BuiltinFontView {
    const unsigned char* widths;
    const unsigned char* const* glyphs;
    uint8_t height;
    bool rle;
};

bool builtinFontView(uint8_t font, BuiltinFontView& out) {
    switch (font) {
        case FONT_16: out = {widtbl_f16, chrtbl_f16, chr_hgt_f16, false}; return true;
        case FONT_32: out = {widtbl_f32, chrtbl_f32, chr_hgt_f32, true}; return true;
        case FONT_64: out = {widtbl_f64, chrtbl_f64, chr_hgt_f64, true}; return true;
        case FONT_7SEG: out = {widtbl_f7s, chrtbl_f7s, chr_hgt_f7s, true}; return true;
        case FONT_72: out = {widtbl_f72, chrtbl_f72, chr_hgt_f72, true}; return true;
        default: return false;
    }
}

template <typename Predicate>
uint8_t sampleCoverage(int32_t x, int32_t y, const Predicate& contains,
                       uint8_t samples = 4) {
    uint16_t covered = 0;
    const uint16_t total = static_cast<uint16_t>(samples) * samples;
    for (uint8_t sy = 0; sy < samples; ++sy) {
        for (uint8_t sx = 0; sx < samples; ++sx) {
            const float px = static_cast<float>(x) +
                             (static_cast<float>(sx) + 0.5f) / samples;
            const float py = static_cast<float>(y) +
                             (static_cast<float>(sy) + 0.5f) / samples;
            if (contains(px, py)) ++covered;
        }
    }
    return static_cast<uint8_t>((covered * 255U + total / 2U) / total);
}

void drawCoveredPixel(Seeed_GFX& gfx, int32_t x, int32_t y, uint32_t fg,
                      uint32_t bg, uint8_t coverage) {
    if (coverage == 0) return;
    if (coverage == 255) gfx.drawPixel(x, y, fg);
    else gfx.drawPixel(x, y, fg, coverage, bg);
}

float normalizedAngle(float angle) {
    angle = fmodf(angle, 360.0f);
    return angle < 0.0f ? angle + 360.0f : angle;
}

bool angleInSweep(float angle, float start, float sweep) {
    if (sweep >= 360.0f) return true;
    const float relative = normalizedAngle(angle - start);
    return relative <= sweep;
}

bool roundedRectContains(float px, float py, float x, float y,
                         float w, float h, float radius) {
    if (w <= 0.0f || h <= 0.0f || px < x || py < y ||
        px > x + w || py > y + h) return false;
    radius = std::max(0.0f, std::min(radius, std::min(w, h) * 0.5f));
    if (radius == 0.0f) return true;
    const float cx = std::max(x + radius, std::min(px, x + w - radius));
    const float cy = std::max(y + radius, std::min(py, y + h - radius));
    const float dx = px - cx;
    const float dy = py - cy;
    return dx * dx + dy * dy <= radius * radius;
}
}

// Constructors

Seeed_GFX::Seeed_GFX()
    : _panel(nullptr), _board(nullptr), _ownsBoard(false)
    , _vpX(0), _vpY(0), _vpW(0), _vpH(0)
    , _xDatum(0), _yDatum(0), _xWidth(0), _yHeight(0)
    , _vpDatum(false), _vpOoB(false)
    , cursor_x(0), cursor_y(0), padX(0)
    , bg_cursor_x(0), last_cursor_x(0)
    , fontsloaded((1UL << FONT_GLCD) | (1UL << FONT_16) |
                  (1UL << FONT_32) | (1UL << FONT_64) |
                  (1UL << FONT_7SEG) | (1UL << FONT_72))
    , glyph_ab(0), glyph_bb(0)
    , isDigits(false), textwrapX(true), textwrapY(false)
    , _swapBytes(false), _booted(false)
    , _cp437(false), _utf8(true), _psram_enable(false)
    , _lastColor(0xFFFF), _fillbg(false)
    , _xPivot(0), _yPivot(0)
    , gfxFont(nullptr), getColor(nullptr)
    , touchCalibration_x0(0), touchCalibration_x1(0)
    , touchCalibration_y0(0), touchCalibration_y1(0)
    , touchCalibration_rotate(false), touchCalibration_invert_x(false), touchCalibration_invert_y(false)
{}

Seeed_GFX::Seeed_GFX(IPanel& panel)
    : Seeed_GFX()
{
    _panel = &panel;
}

Seeed_GFX::Seeed_GFX(Seeed_Product::Product product)
    : Seeed_GFX()
{
    _pendingProduct = product;
    _hasPendingProduct = true;
}

Seeed_GFX::~Seeed_GFX() {
    releaseOwnedHardware();
}

// Panel management

void Seeed_GFX::setPanel(IPanel& panel) {
    attachPanel(panel);
}

void Seeed_GFX::attachPanel(IPanel& panel) {
    releaseOwnedHardware();
    _panel = &panel;
    _lastResult = GfxResult::success();
}

void Seeed_GFX::releaseOwnedHardware() {
    if (_instance.hasStack()) {
        _instance.reset();
    } else {
        if (_ownsBoard && _board) delete _board;
    }
    _panel = nullptr;
    _board = nullptr;
    _touch = nullptr;
    _activeProduct = Seeed_Product::CUSTOM;
    _ownsBoard = false;
    _booted = false;
    DMA_Enabled = false;
    _vpX = _vpY = _vpW = _vpH = 0;
}

// Template: Quick setup
// (Template definition is in Seeed_GFX.h for compiler visibility)

bool Seeed_GFX::begin() {
    if (_hasPendingProduct) {
        const Seeed_Product::Product product = _pendingProduct;
        _hasPendingProduct = false;
        return begin(product);
    }
    if (_panel) {
        bool ok = _panel->begin();
        if (ok) {
            _vpX = _vpY = 0;
            _vpW = _panel->width();
            _vpH = _panel->height();
            _vpOoB = false;
            _booted = true;
            _lastResult = GfxResult::success();
        } else {
            _lastResult = _panel->lastResult();
            if (_lastResult.ok()) {
                _lastResult = GfxResult(GfxError::PanelInitFailed,
                                        "external panel initialization failed");
            }
        }
        return ok;
    }
    _lastResult = GfxResult(GfxError::NotInitialized, "no panel or product selected");
    return false;
}

bool Seeed_GFX::begin(Seeed_Product::Product product) {
    releaseOwnedHardware();
    _hasPendingProduct = false;

    _lastResult = ProductCatalog::create(product, _instance);
    if (!_lastResult) {
        _instance.reset();
        return false;
    }
    _lastResult = _instance.begin();
    if (!_lastResult) {
        _instance.reset();
        return false;
    }

    const ProductDescriptor* descriptor = ProductCatalog::find(product);
    IPanel* createdPanel = _instance.panel();
    if (descriptor && descriptor->mode != ProductPanelMode::Default && createdPanel) {
        const PanelMode mode = descriptor->mode == ProductPanelMode::Colorful
            ? PanelMode::Colorful : PanelMode::BWRY;
        _lastResult = createdPanel->configure(mode);
        if (!_lastResult) {
            _instance.reset();
            return false;
        }
    }

    // Keep the product-selector path aligned with the standalone 1.69 LCD
    // config: rotation 3 presents the 280x240 glass upright.
    if (product == Seeed_Product::Seeed_LCD_1INCH69 && createdPanel) {
        createdPanel->setRotation(3);
    }

    _board = _instance.board();
    _panel = createdPanel;
    _touch = _instance.touch();
    _vpX = _vpY = 0;
    _vpW = _panel->width();
    _vpH = _panel->height();
    _vpOoB = false;
    _booted = true;
    _activeProduct = product;
    _lastResult = GfxResult::success();
    return true;
}

GfxResult Seeed_GFX::end() {
    if (!_panel) {
        _lastResult = GfxResult(GfxError::NotInitialized, "no panel attached");
        return _lastResult;
    }
    if (!_booted) {
        _lastResult = GfxResult::success();
        return _lastResult;
    }
    _lastResult = (_instance.panel() == _panel)
        ? _instance.end() : _panel->end();
    _booted = false;
    DMA_Enabled = false;
    return _lastResult;
}

// Graphics API

void Seeed_GFX::drawPixel(int32_t x, int32_t y, uint32_t color) {
    if (!_panel) return;
    if (_vpOoB) return;

    x += _xDatum;
    y += _yDatum;

    if ((x < _vpX) || (y < _vpY) || (x >= _vpW) || (y >= _vpH)) return;

    _panel->setAddrWindow(x, y, x, y);
    _panel->writePixel((uint16_t)color);
}

void Seeed_GFX::drawLine(int32_t xs, int32_t ys, int32_t xe, int32_t ye, uint32_t color) {
    if (!_panel) return;

    // Bresenham's line algorithm
    int32_t dx = abs(xe - xs);
    int32_t dy = abs(ye - ys);
    int32_t sx = (xs < xe) ? 1 : -1;
    int32_t sy = (ys < ye) ? 1 : -1;
    int32_t err = dx - dy;

    while (true) {
        drawPixel(xs, ys, color);
        if (xs == xe && ys == ye) break;
        int32_t e2 = err * 2;
        if (e2 > -dy) { err -= dy; xs += sx; }
        if (e2 < dx)  { err += dx; ys += sy; }
    }
}

void Seeed_GFX::drawFastVLine(int32_t x, int32_t y, int32_t h, uint32_t color) {
    if (!_panel) return;
    if (_vpOoB) return;
    x += _xDatum;
    y += _yDatum;

    // Clipping
    if (x < _vpX || x >= _vpW) return;
    if (y < _vpY) { h -= _vpY - y; y = _vpY; }
    if ((y + h) > _vpH) { h = _vpH - y; }
    if (h < 1) return;

    _panel->setAddrWindow(x, y, x, y + h - 1);
    _panel->writeFill((uint16_t)color, h);
}

void Seeed_GFX::drawFastHLine(int32_t x, int32_t y, int32_t w, uint32_t color) {
    if (!_panel) return;
    if (_vpOoB) return;
    x += _xDatum;
    y += _yDatum;

    // Clipping
    if (y < _vpY || y >= _vpH) return;
    if (x < _vpX) { w -= _vpX - x; x = _vpX; }
    if ((x + w) > _vpW) { w = _vpW - x; }
    if (w < 1) return;

    _panel->setAddrWindow(x, y, x + w - 1, y);
    _panel->writeFill((uint16_t)color, w);
}

void Seeed_GFX::fillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
    if (!_panel) return;
    if (_vpOoB) return;
    x += _xDatum;
    y += _yDatum;

    // Clipping
    if (x < _vpX) { w -= _vpX - x; x = _vpX; }
    if (y < _vpY) { h -= _vpY - y; y = _vpY; }
    if ((x + w) > _vpW) { w = _vpW - x; }
    if ((y + h) > _vpH) { h = _vpH - y; }
    if (w < 1 || h < 1) return;

    _panel->setAddrWindow(x, y, x + w - 1, y + h - 1);
    _panel->writeFill((uint16_t)color, (uint32_t)w * h);
}

void Seeed_GFX::fillScreen(uint32_t color) {
    if (!_panel) return;
    fillRect(0, 0, _panel->width(), _panel->height(), color);
}

void Seeed_GFX::drawRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
    if (w <= 0 || h <= 0) return;
    drawFastHLine(x, y, w, color);
    drawFastHLine(x, y + h - 1, w, color);
    drawFastVLine(x, y, h, color);
    drawFastVLine(x + w - 1, y, h, color);
}

void Seeed_GFX::drawCircle(int32_t x, int32_t y, int32_t r, uint32_t color) {
    if (r < 0) return;
    int32_t f = 1 - r;
    int32_t ddF_x = 1;
    int32_t ddF_y = -2 * r;
    int32_t x1 = 0;
    int32_t y1 = r;

    drawPixel(x, y + r, color);
    drawPixel(x, y - r, color);
    drawPixel(x + r, y, color);
    drawPixel(x - r, y, color);

    while (x1 < y1) {
        if (f >= 0) {
            y1--;
            ddF_y += 2;
            f += ddF_y;
        }
        x1++;
        ddF_x += 2;
        f += ddF_x;

        drawPixel(x + x1, y + y1, color);
        drawPixel(x - x1, y + y1, color);
        drawPixel(x + x1, y - y1, color);
        drawPixel(x - x1, y - y1, color);
        drawPixel(x + y1, y + x1, color);
        drawPixel(x - y1, y + x1, color);
        drawPixel(x + y1, y - x1, color);
        drawPixel(x - y1, y - x1, color);
    }
}

void Seeed_GFX::fillCircle(int32_t x, int32_t y, int32_t r, uint32_t color) {
    if (r < 0) return;
    drawFastVLine(x, y - r, 2 * r + 1, color);
    fillCircleHelper(x, y, r, 3, 0, color);
}

void Seeed_GFX::fillCircleHelper(int32_t x, int32_t y, int32_t r, uint8_t cornername, int32_t delta, uint32_t color) {
    if (r < 0) return;
    int32_t f = 1 - r;
    int32_t ddF_x = 1;
    int32_t ddF_y = -2 * r;
    int32_t x1 = 0;
    int32_t y1 = r;

    delta++;
    while (x1 < y1) {
        if (f >= 0) { y1--; ddF_y += 2; f += ddF_y; }
        x1++; ddF_x += 2; f += ddF_x;

        if (x1 < (y1 + 1)) {
            if (cornername & 1) { drawFastVLine(x + x1, y - y1, 2 * y1 + delta, color); }
            if (cornername & 2) { drawFastVLine(x - x1, y - y1, 2 * y1 + delta, color); }
        }
        if (y1 != x1) {
            if (cornername & 1) { drawFastVLine(x + y1, y - x1, 2 * x1 + delta, color); }
            if (cornername & 2) { drawFastVLine(x - y1, y - x1, 2 * x1 + delta, color); }
        }
    }
}

void Seeed_GFX::drawCircleHelper(int32_t x, int32_t y, int32_t r, uint8_t cornername, uint32_t color) {
    if (r < 0) return;
    int32_t f = 1 - r;
    int32_t ddF_x = 1;
    int32_t ddF_y = -2 * r;
    int32_t x1 = 0;
    int32_t y1 = r;

    while (x1 < y1) {
        if (f >= 0) { y1--; ddF_y += 2; f += ddF_y; }
        x1++; ddF_x += 2; f += ddF_x;
        if (x1 < (y1 + 1)) {
            if (cornername & 0x4) { drawPixel(x + x1, y + y1, color); drawPixel(x + y1, y + x1, color); }
            if (cornername & 0x2) { drawPixel(x + x1, y - y1, color); drawPixel(x + y1, y - x1, color); }
            if (cornername & 0x8) { drawPixel(x - y1, y + x1, color); drawPixel(x - x1, y + y1, color); }
            if (cornername & 0x1) { drawPixel(x - y1, y - x1, color); drawPixel(x - x1, y - y1, color); }
        }
    }
}

void Seeed_GFX::drawTriangle(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t x3, int32_t y3, uint32_t color) {
    drawLine(x1, y1, x2, y2, color);
    drawLine(x2, y2, x3, y3, color);
    drawLine(x3, y3, x1, y1, color);
}

void Seeed_GFX::fillTriangle(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t x3, int32_t y3, uint32_t color) {
    int32_t a, b, y, last;
    // Sort coordinates by y
    if (y1 > y2) { swap(y1, y2); swap(x1, x2); }
    if (y2 > y3) { swap(y3, y2); swap(x3, x2); }
    if (y1 > y2) { swap(y1, y2); swap(x1, x2); }

    if (y1 == y3) {
        a = b = x1;
        if (x2 < a) a = x2; else if (x2 > b) b = x2;
        if (x3 < a) a = x3; else if (x3 > b) b = x3;
        drawFastHLine(a, y1, b - a + 1, color);
        return;
    }

    int32_t dx12 = x2 - x1, dy12 = y2 - y1;
    int32_t dx13 = x3 - x1, dy13 = y3 - y1;
    int32_t dx23 = x3 - x2, dy23 = y3 - y2;
    int32_t sa = 0, sb = 0;

    // A flat top skips this half, avoiding dy12 == 0.
    last = (y2 == y3) ? y2 : y2 - 1;
    for (y = y1; y <= last; y++) {
        a = x1 + sa / dy12;
        b = x1 + sb / dy13;
        sa += dx12;
        sb += dx13;
        if (a > b) swap(a, b);
        drawFastHLine(a, y, b - a + 1, color);
    }

    sa = dx23 * (y - y2);
    sb = dx13 * (y - y1);
    for (; y <= y3; y++) {
        a = x2 + sa / dy23;
        b = x1 + sb / dy13;
        sa += dx23;
        sb += dx13;
        if (a > b) swap(a, b);
        drawFastHLine(a, y, b - a + 1, color);
    }
}

void Seeed_GFX::drawRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint32_t color) {
    if (w <= 0 || h <= 0 || r < 0) return;
    r = min<int32_t>(r, min<int32_t>(w, h) / 2);
    drawFastHLine(x + r, y, w - 2 * r, color);
    drawFastHLine(x + r, y + h - 1, w - 2 * r, color);
    drawFastVLine(x, y + r, h - 2 * r, color);
    drawFastVLine(x + w - 1, y + r, h - 2 * r, color);
    drawCircleHelper(x + r, y + r, r, 1, color);
    drawCircleHelper(x + w - r - 1, y + r, r, 2, color);
    drawCircleHelper(x + w - r - 1, y + h - r - 1, r, 4, color);
    drawCircleHelper(x + r, y + h - r - 1, r, 8, color);
}

void Seeed_GFX::fillRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint32_t color) {
    if (w <= 0 || h <= 0 || r < 0) return;
    r = min<int32_t>(r, min<int32_t>(w, h) / 2);
    fillRect(x + r, y, w - 2 * r, h, color);
    fillCircleHelper(x + w - r - 1, y + r, r, 1, h - 2 * r - 1, color);
    fillCircleHelper(x + r, y + r, r, 2, h - 2 * r - 1, color);
}

void Seeed_GFX::drawEllipse(int16_t x, int16_t y, int32_t rx, int32_t ry, uint16_t color) {
    if (rx < 2 || ry < 2) return;
    int32_t x1, y1;
    float dx, dy, d1, d2;
    x1 = 0; y1 = ry;
    d1 = (ry * ry) - (rx * rx * ry) + (0.25 * rx * rx);
    dx = 2 * ry * ry * x1;
    dy = 2 * rx * rx * y1;
    while (dx < dy) {
        drawPixel(x + x1, y + y1, color);
        drawPixel(x - x1, y + y1, color);
        drawPixel(x + x1, y - y1, color);
        drawPixel(x - x1, y - y1, color);
        if (d1 < 0) { x1++; dx += (2 * ry * ry); d1 += dx + (ry * ry); }
        else { x1++; y1--; dx += (2 * ry * ry); dy -= (2 * rx * rx); d1 += dx - dy + (ry * ry); }
    }
    d2 = ((ry * ry) * ((x1 + 0.5) * (x1 + 0.5))) + ((rx * rx) * ((y1 - 1) * (y1 - 1))) - (rx * rx * ry * ry);
    while (y1 >= 0) {
        drawPixel(x + x1, y + y1, color);
        drawPixel(x - x1, y + y1, color);
        drawPixel(x + x1, y - y1, color);
        drawPixel(x - x1, y - y1, color);
        if (d2 > 0) { y1--; dy -= (2 * rx * rx); d2 += (rx * rx) - dy; }
        else { y1--; x1++; dx += (2 * ry * ry); dy -= (2 * rx * rx); d2 += dx - dy + (rx * rx); }
    }
}

void Seeed_GFX::fillEllipse(int16_t x, int16_t y, int32_t rx, int32_t ry, uint16_t color) {
    if (rx < 2 || ry < 2) return;
    int32_t x1, y1;
    float dx, dy, d1, d2;
    x1 = 0; y1 = ry;
    d1 = (ry * ry) - (rx * rx * ry) + (0.25 * rx * rx);
    dx = 2 * ry * ry * x1;
    dy = 2 * rx * rx * y1;
    while (dx < dy) {
        drawFastHLine(x - x1, y + y1, 2 * x1 + 1, color);
        drawFastHLine(x - x1, y - y1, 2 * x1 + 1, color);
        if (d1 < 0) { x1++; dx += (2 * ry * ry); d1 += dx + (ry * ry); }
        else { x1++; y1--; dx += (2 * ry * ry); dy -= (2 * rx * rx); d1 += dx - dy + (ry * ry); }
    }
    d2 = ((ry * ry) * ((x1 + 0.5) * (x1 + 0.5))) + ((rx * rx) * ((y1 - 1) * (y1 - 1))) - (rx * rx * ry * ry);
    while (y1 >= 0) {
        drawFastHLine(x - x1, y + y1, 2 * x1 + 1, color);
        drawFastHLine(x - x1, y - y1, 2 * x1 + 1, color);
        if (d2 > 0) { y1--; dy -= (2 * rx * rx); d2 += (rx * rx) - dy; }
        else { y1--; x1++; dx += (2 * ry * ry); dy -= (2 * rx * rx); d2 += dx - dy + (rx * rx); }
    }
}

// Display settings

void Seeed_GFX::setRotation(uint8_t r) {
    rotation = r & 3;
    if (_panel) {
        _panel->setRotation(rotation);
        // Match the documented TFT_eSPI behavior: changing rotation also
        // restores the coordinate origin and the full-screen viewport.
        resetViewport();
    } else {
        _xDatum = _yDatum = 0;
        _vpDatum = false;
    }
    if (_touch) _touch->setDisplayRotation(rotation);
}

uint8_t Seeed_GFX::getRotation() const {
    return rotation;
}

void Seeed_GFX::setTouchRotation(uint8_t r) {
    if (_touch) _touch->setRotation(r & 3);
}

void Seeed_GFX::invertDisplay(bool i) {
    if (_panel) {
        _panel->invertDisplay(i);
    }
}

int16_t Seeed_GFX::width() const {
    return _panel ? _panel->width() : 0;
}

int16_t Seeed_GFX::height() const {
    return _panel ? _panel->height() : 0;
}

// Pixel reading

uint16_t Seeed_GFX::readPixel(int32_t x, int32_t y) {
    if (!_panel || _vpOoB) return 0;
    x += _xDatum;
    y += _yDatum;
    if (x < _vpX || y < _vpY || x >= _vpW || y >= _vpH) return 0;
    return _panel->readPixel(static_cast<uint16_t>(x), static_cast<uint16_t>(y));
}

// Image rendering

void Seeed_GFX::setSwapBytes(bool swap) {
    _swapBytes = swap;
}

bool Seeed_GFX::getSwapBytes() const {
    return _swapBytes;
}

void Seeed_GFX::pushColor(uint16_t color) {
    if (!_panel) return;
    _panel->writePixel(color);
}

void Seeed_GFX::pushColor(uint16_t color, uint32_t len) {
    if (!_panel) return;
    _panel->writeFill(color, len);
}

void Seeed_GFX::pushBlock(uint16_t color, uint32_t len) {
    if (!_panel) return;
    _panel->writeFill(color, len);
}

void Seeed_GFX::pushPixels(const void *data_in, uint32_t len) {
    if (!_panel || !data_in || !len) return;
    const uint16_t* data = static_cast<const uint16_t*>(data_in);
    if (!_swapBytes) {
        _panel->writePixels(data, len);
        return;
    }

    // Keep the caller's buffer unchanged for the ordinary synchronous path.
    // A small fixed block avoids a heap allocation for large images while
    // still allowing the panel/driver to use its bulk-write implementation.
    uint16_t swapped[64];
    while (len) {
        const uint32_t count = std::min<uint32_t>(len, 64U);
        for (uint32_t i = 0; i < count; ++i) {
            const uint16_t value = data[i];
            swapped[i] = static_cast<uint16_t>((value >> 8) | (value << 8));
        }
        _panel->writePixels(swapped, count);
        data += count;
        len -= count;
    }
}

void Seeed_GFX::pushImage(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t *data) {
    pushImage(x, y, w, h, static_cast<const uint16_t*>(data));
}

void Seeed_GFX::pushImage(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t *data, uint16_t transparent) {
    pushImage(x, y, w, h, static_cast<const uint16_t*>(data), transparent);
}

void Seeed_GFX::setBitmapColor(uint16_t fgcolor, uint16_t bgcolor) {
    bitmap_fg = fgcolor;
    bitmap_bg = bgcolor;
}

void Seeed_GFX::drawBitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint16_t fgcolor) {
    int16_t byteWidth = (w + 7) / 8;
    uint8_t byteVal;
    for (int16_t j = 0; j < h; j++) {
        for (int16_t i = 0; i < w; i++) {
            if (i & 7) byteVal <<= 1;
            else byteVal = pgm_read_byte(bitmap + j * byteWidth + i / 8);
            if (byteVal & 0x80) drawPixel(x + i, y + j, fgcolor);
        }
    }
}

void Seeed_GFX::drawBitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint16_t fgcolor, uint16_t bgcolor) {
    int16_t byteWidth = (w + 7) / 8;
    uint8_t byteVal;
    for (int16_t j = 0; j < h; j++) {
        for (int16_t i = 0; i < w; i++) {
            if (i & 7) byteVal <<= 1;
            else byteVal = pgm_read_byte(bitmap + j * byteWidth + i / 8);
            drawPixel(x + i, y + j, (byteVal & 0x80) ? fgcolor : bgcolor);
        }
    }
}

void Seeed_GFX::drawBitmapRotatedCW(int16_t x, int16_t y,
                                    const uint8_t *bitmap, int16_t w,
                                    int16_t h, uint16_t fgcolor,
                                    uint16_t bgcolor) {
    if (!bitmap || w <= 0 || h <= 0) return;

    const int16_t byteWidth = (w + 7) / 8;
    for (int16_t sourceY = 0; sourceY < h; ++sourceY) {
        for (int16_t sourceX = 0; sourceX < w; ++sourceX) {
            const uint8_t byteValue = pgm_read_byte(
                bitmap + sourceY * byteWidth + sourceX / 8);
            const bool bitSet = (byteValue & (0x80U >> (sourceX & 7))) != 0;
            // Clockwise source (sourceX, sourceY) -> output (h-1-sourceY, sourceX).
            drawPixel(x + h - 1 - sourceY, y + sourceX,
                      bitSet ? fgcolor : bgcolor);
        }
    }
}

bool Seeed_GFX::drawBmp(int32_t x, int32_t y, const uint8_t* data, size_t len) {
#if !defined(ARDUINO_ARCH_NRF52)
    return uiOk(drawImage(x, y, data, len, UiImageFormat::BMP));
#else
    (void)x; (void)y; (void)data; (void)len;
    return false;
#endif
}

#if !defined(ARDUINO_ARCH_NRF52)
UiStatus Seeed_GFX::drawImage(int32_t x, int32_t y, IUiDataSource& source,
                              IUiImageDecoder& decoder, void* workBuffer,
                              size_t workBytes) {
    UiImageInfo information;
    UiStatus status = decoder.readInfo(source, information);
    if (!uiOk(status)) return status;
    const size_t required = decoder.requiredWorkBytes();
    if (!required) return UiStatus::Unsupported;
    uint8_t* ownedWork = nullptr;
    if (!workBuffer) {
        ownedWork = new (std::nothrow) uint8_t[required];
        if (!ownedWork) return UiStatus::CapacityExceeded;
        workBuffer = ownedWork;
        workBytes = required;
    }
    if (workBytes < required) {
        delete[] ownedWork;
        return UiStatus::InvalidArgument;
    }
    UiPoint destination;
    destination.x = uiClamp16(x); destination.y = uiClamp16(y);
    status = decoder.begin(source, destination, workBuffer, workBytes);
    if (uiOk(status)) {
        UiGfxImageSink sink(*this);
        do { status = decoder.step(sink, 4); }
        while (status == UiStatus::Busy);
        if (uiOk(status) && !decoder.finished()) status = UiStatus::DataError;
    }
    decoder.cancel();
    delete[] ownedWork;
    return status;
}

UiStatus Seeed_GFX::drawImage(int32_t x, int32_t y, IUiDataSource& source,
                              UiImageFormat format, void* workBuffer,
                              size_t workBytes) {
    BmpDecoder bmp;
    QoiDecoder qoi;
    IUiImageDecoder* decoder = nullptr;
    if (format == UiImageFormat::BMP) decoder = &bmp;
    else if (format == UiImageFormat::QOI) decoder = &qoi;
    else if (format == UiImageFormat::Auto) {
        if (bmp.probe(source)) decoder = &bmp;
        else if (qoi.probe(source)) decoder = &qoi;
    }
    if (!decoder) return UiStatus::Unsupported;
    return drawImage(x, y, source, *decoder, workBuffer, workBytes);
}

UiStatus Seeed_GFX::drawImage(int32_t x, int32_t y, const void* data,
                              size_t dataBytes, UiImageFormat format,
                              void* workBuffer, size_t workBytes) {
    if (!data || !dataBytes || dataBytes > 0xFFFFFFFFULL)
        return UiStatus::InvalidArgument;
    UiMemorySource source(data, static_cast<uint32_t>(dataBytes));
    return drawImage(x, y, source, format, workBuffer, workBytes);
}

bool Seeed_GFX::drawImageRaw(int32_t x, int32_t y, int32_t w, int32_t h,
                             const void* data, size_t dataBytes,
                             UiImageFormat format,
                             const uint16_t* colorMap) {
    if (!data || w <= 0 || h <= 0) return false;
    const size_t widthValue = static_cast<size_t>(w);
    const size_t heightValue = static_cast<size_t>(h);
    if (widthValue > SIZE_MAX / heightValue) return false;
    const size_t pixels = widthValue * heightValue;
    if (format == UiImageFormat::RawRGB565) {
        if (pixels > SIZE_MAX / 2U || dataBytes < pixels * 2U) return false;
        pushImage(x, y, w, h, static_cast<const uint16_t*>(data));
        return true;
    }
    if (format == UiImageFormat::Mono1) {
        const size_t stride = (widthValue + 7U) / 8U;
        if (heightValue > SIZE_MAX / stride || dataBytes < stride * heightValue)
            return false;
        drawBitmap(static_cast<int16_t>(x), static_cast<int16_t>(y),
                   static_cast<const uint8_t*>(data), static_cast<int16_t>(w),
                   static_cast<int16_t>(h), bitmap_fg, bitmap_bg);
        return true;
    }
    if (format != UiImageFormat::Indexed4 &&
        format != UiImageFormat::Indexed8) return false;
    if (format == UiImageFormat::Indexed4 && !colorMap) return false;
    const size_t stride = format == UiImageFormat::Indexed4
        ? (widthValue + 1U) / 2U : widthValue;
    if (heightValue > SIZE_MAX / stride || dataBytes < stride * heightValue)
        return false;
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    uint16_t row[64];
    for (int32_t yy = 0; yy < h; ++yy) {
        for (int32_t offset = 0; offset < w; offset += 64) {
            const int32_t count = std::min<int32_t>(64, w - offset);
            for (int32_t i = 0; i < count; ++i) {
                const int32_t sourceX = offset + i;
                uint8_t index;
                if (format == UiImageFormat::Indexed4) {
                    const uint8_t packed = pgm_read_byte(
                        bytes + static_cast<size_t>(yy) * stride + (sourceX >> 1));
                    index = static_cast<uint8_t>((sourceX & 1)
                        ? (packed & 0x0FU) : (packed >> 4));
                } else {
                    index = pgm_read_byte(
                        bytes + static_cast<size_t>(yy) * stride + sourceX);
                }
                row[i] = colorMap ? pgm_read_word(colorMap + index)
                                  : color8to16(index);
            }
            pushImage(x + offset, y + yy, count, 1, row);
        }
    }
    return true;
}
#endif

// Text API

int16_t Seeed_GFX::drawString(const char *string, int32_t x, int32_t y, uint8_t font) {
    if (!string) return 0;
    int16_t sumX = 0;
    uint16_t len = strlen(string);
    uint16_t n = 0;
    uint8_t prevFont = textfont;

    // Calculate string width for alignment
    uint16_t cwidth = textWidth(string, font);
    uint16_t cheight = fontHeight(font);

    // GFX FreeFonts render relative to a baseline (their glyph yOffset is
    // normally negative), whereas GLCD/built-in/smooth fonts render from a
    // top-left cursor. Convert datum coordinates through a common top edge,
    // then restore the FreeFont baseline before drawing.
    const bool freeFont = gfxFont && font == FONT_GLCD && !_smoothFont.isLoaded();
    const int16_t freeAscent = freeFont
        ? static_cast<int16_t>(glyph_ab * textsize) : 0;
    const int16_t freeDescent = freeFont
        ? static_cast<int16_t>(glyph_bb * textsize) : 0;
    if (freeFont) cheight = static_cast<uint16_t>(freeAscent + freeDescent);

    // Handle text datum
    int32_t poX = x;
    int32_t poY = y;

    if (textdatum || padX) {
        switch (textdatum) {
            case TC_DATUM: poX -= cwidth / 2; break;
            case TR_DATUM: poX -= cwidth; break;
            case ML_DATUM: poY -= cheight / 2; break;
            case MC_DATUM: poX -= cwidth / 2; poY -= cheight / 2; break;
            case MR_DATUM: poX -= cwidth; poY -= cheight / 2; break;
            case BL_DATUM: poY -= cheight; break;
            case BC_DATUM: poX -= cwidth / 2; poY -= cheight; break;
            case BR_DATUM: poX -= cwidth; poY -= cheight; break;
            case L_BASELINE: poY -= freeFont ? freeAscent : cheight; break;
            case C_BASELINE: poX -= cwidth / 2;
                             poY -= freeFont ? freeAscent : cheight; break;
            case R_BASELINE: poX -= cwidth;
                             poY -= freeFont ? freeAscent : cheight; break;
            default: break;
        }
    }

    const int32_t drawY = freeFont ? poY + freeAscent : poY;
    setCursor(poX, drawY);
    textfont = font;
    const bool savedFillBackground = _fillbg;
    // TFT_eSPI-compatible behavior: text padding is explicitly intended to
    // erase the previous value, so smooth glyph cells must be opaque even
    // when setTextColor() did not request background fill separately.
    if (_smoothFont.isLoaded() && padX) _fillbg = true;

    while (n < len) {
        uint16_t uniCode = decodeUTF8((uint8_t*)string, &n, len - n);
        if (uniCode == 0) continue;
        int16_t cw = 0;
        if (_smoothFont.isLoaded()) {
            smoothFontTarget = this;
            cw = static_cast<int16_t>(_smoothFont.drawChar(
                cursor_x, cursor_y, uniCode,
                static_cast<uint16_t>(textcolor),
                static_cast<uint16_t>(textbgcolor), _fillbg));
        } else {
            cw = drawChar(uniCode, cursor_x, cursor_y, font);
        }
        sumX += cw;
        cursor_x += cw;
    }

    _fillbg = savedFillBackground;
    textfont = prevFont;

    // Handle padding
    if (padX > cwidth && textcolor != textbgcolor) {
        const int32_t remaining = static_cast<int32_t>(padX) - cwidth;
        switch (textdatum) {
            case TC_DATUM:
            case MC_DATUM:
            case BC_DATUM:
            case C_BASELINE: {
                const int32_t left = remaining / 2;
                const int32_t right = remaining - left;
                if (left) fillRect(poX - left, poY, left, cheight, textbgcolor);
                if (right) fillRect(poX + cwidth, poY, right, cheight,
                                    textbgcolor);
                break;
            }
            case TR_DATUM:
            case MR_DATUM:
            case BR_DATUM:
            case R_BASELINE:
                fillRect(poX - remaining, poY, remaining, cheight,
                         textbgcolor);
                break;
            case TL_DATUM:
            default:
                fillRect(poX + cwidth, poY, remaining, cheight, textbgcolor);
                break;
        }
    }

    return sumX;
}

int16_t Seeed_GFX::drawString(const char *string, int32_t x, int32_t y) {
    return drawString(string, x, y, textfont);
}

int16_t Seeed_GFX::drawString(const String &string, int32_t x, int32_t y, uint8_t font) {
    return drawString(string.c_str(), x, y, font);
}

int16_t Seeed_GFX::drawString(const String &string, int32_t x, int32_t y) {
    return drawString(string.c_str(), x, y, textfont);
}

int16_t Seeed_GFX::drawCentreString(const char *string, int32_t x, int32_t y, uint8_t font) {
    const uint8_t previousDatum = textdatum;
    textdatum = TC_DATUM;
    const int16_t drawnWidth = drawString(string, x, y, font);
    textdatum = previousDatum;
    return drawnWidth;
}

int16_t Seeed_GFX::drawRightString(const char *string, int32_t x, int32_t y, uint8_t font) {
    const uint8_t previousDatum = textdatum;
    textdatum = TR_DATUM;
    const int16_t drawnWidth = drawString(string, x, y, font);
    textdatum = previousDatum;
    return drawnWidth;
}

int16_t Seeed_GFX::drawNumber(long intNumber, int32_t x, int32_t y, uint8_t font) {
    isDigits = true;
    char buf[16];
    snprintf(buf, sizeof(buf), "%ld", intNumber);
    return drawString(buf, x, y, font);
}

int16_t Seeed_GFX::drawNumber(long intNumber, int32_t x, int32_t y) {
    return drawNumber(intNumber, x, y, textfont);
}

int16_t Seeed_GFX::drawFloat(float floatNumber, uint8_t decimal, int32_t x, int32_t y, uint8_t font) {
    isDigits = true;
    if (decimal > 7) decimal = 7;
    char buf[32];
    snprintf(buf, sizeof(buf), "%.*f", (int)decimal, (double)floatNumber);
    return drawString(buf, x, y, font);
}

int16_t Seeed_GFX::drawFloat(float floatNumber, uint8_t decimal, int32_t x, int32_t y) {
    return drawFloat(floatNumber, decimal, x, y, textfont);
}

void Seeed_GFX::setCursor(int16_t x, int16_t y) {
    cursor_x = x;
    cursor_y = y;
}

void Seeed_GFX::setCursor(int16_t x, int16_t y, uint8_t font) {
    setTextFont(font);
    cursor_x = x;
    cursor_y = y;
}

int16_t Seeed_GFX::getCursorX() const { return cursor_x; }
int16_t Seeed_GFX::getCursorY() const { return cursor_y; }

void Seeed_GFX::setTextColor(uint16_t color) {
    textcolor = color;
    textbgcolor = color;
    _fillbg = false;
}

void Seeed_GFX::setTextColor(uint16_t fgcolor, uint16_t bgcolor, bool bgfill) {
    textcolor = fgcolor;
    textbgcolor = bgcolor;
    _fillbg = bgfill;
}

void Seeed_GFX::setTextSize(uint8_t size) {
    if (size > 7) size = 7;
    textsize = (size > 0) ? size : 1;
}

void Seeed_GFX::setTextWrap(bool wrapX, bool wrapY) {
    textwrapX = wrapX;
    textwrapY = wrapY;
}

void Seeed_GFX::setTextDatum(uint8_t datum) {
    textdatum = datum;
}

uint8_t Seeed_GFX::getTextDatum() const {
    return textdatum;
}

void Seeed_GFX::setTextPadding(uint16_t x_width) {
    padX = x_width;
}

uint16_t Seeed_GFX::getTextPadding() const {
    return padX;
}

void Seeed_GFX::setTextFont(uint8_t font) {
    BuiltinFontView view = {};
    textfont = (font == FONT_GLCD || builtinFontView(font, view)) ? font : FONT_GLCD;
    gfxFont = nullptr;
}

int16_t Seeed_GFX::textWidth(const char *string) {
    return textWidth(string, textfont);
}

int16_t Seeed_GFX::textWidth(const char *string, uint8_t font) {
    if (!string) return 0;
    int32_t str_width = 0;

    if (_smoothFont.isLoaded()) {
        const uint16_t len = static_cast<uint16_t>(strlen(string));
        uint16_t index = 0;
        while (index < len) {
            const uint16_t code = decodeUTF8(
                reinterpret_cast<uint8_t*>(const_cast<char*>(string)),
                &index, len - index);
            if (code) str_width += _smoothFont.getCharWidth(code);
        }
        return static_cast<int16_t>(str_width);
    }

    if (gfxFont && font == 1) {
        // GFX font
        while (*string) {
            uint16_t uniCode = *(string++);
            if ((uniCode >= pgm_read_word(&gfxFont->first)) && (uniCode <= pgm_read_word(&gfxFont->last))) {
                uniCode -= pgm_read_word(&gfxFont->first);
                GFXglyph *glyph = &(static_cast<GFXglyph*>(SEEED_PGM_PTR(&gfxFont->glyph))[uniCode]);
                if (*string || isDigits) str_width += pgm_read_byte(&glyph->xAdvance);
                else str_width += ((int8_t)pgm_read_byte(&glyph->xOffset) + pgm_read_byte(&glyph->width));
            }
        }
    } else {
        BuiltinFontView view = {};
        if (builtinFontView(font, view)) {
            while (*string) {
                const uint8_t c = static_cast<uint8_t>(*string++);
                const uint8_t index = (c >= 32 && c <= 127) ? c - 32 : 0;
                str_width += pgm_read_byte(view.widths + index);
            }
        } else {
            // GLCD font: 6 pixels per character
            while (*string++) str_width += 6;
        }
    }

    isDigits = false;
    return str_width * textsize;
}

int16_t Seeed_GFX::textWidth(const String &string, uint8_t font) {
    return textWidth(string.c_str(), font);
}

int16_t Seeed_GFX::textWidth(const String &string) {
    return textWidth(string.c_str(), textfont);
}

int16_t Seeed_GFX::fontHeight(uint8_t font) {
    if (_smoothFont.isLoaded())
        return static_cast<int16_t>(_smoothFont.fontHeight());
    if (gfxFont && font == FONT_GLCD) {
        return pgm_read_byte(&gfxFont->yAdvance) * textsize;
    }
    BuiltinFontView view = {};
    if (builtinFontView(font, view)) return view.height * textsize;
    return 8 * textsize;
}

int16_t Seeed_GFX::fontHeight() {
    return fontHeight(textfont);
}

size_t Seeed_GFX::write(uint8_t c) {
    if (_vpOoB) return 1;

    if (c == '\n') {
        _utf8BytesRemaining = 0;
        cursor_x = 0;
        cursor_y += _smoothFont.isLoaded() ? _smoothFont.fontHeight() : fontHeight();
        if (textwrapY && cursor_y >= height()) cursor_y = 0;
        return 1;
    }
    // Carriage return is ignored, as it is by Arduino Print/TFT_eSPI.
    if (c == '\r') return 1;

    const uint16_t uniCode = decodeUTF8(c);
    if (!uniCode) return 1;

    int16_t characterWidth = 0;
    if (_smoothFont.isLoaded()) {
        characterWidth = static_cast<int16_t>(_smoothFont.getCharWidth(uniCode));
    } else if (gfxFont && textfont == FONT_GLCD) {
        const uint16_t first = pgm_read_word(&gfxFont->first);
        const uint16_t last = pgm_read_word(&gfxFont->last);
        if (uniCode < first || uniCode > last) return 1;
        GFXglyph* glyph = &(static_cast<GFXglyph*>(SEEED_PGM_PTR(&gfxFont->glyph))[
            uniCode - first]);
        const int16_t rightEdge = static_cast<int8_t>(pgm_read_byte(&glyph->xOffset)) +
                                  pgm_read_byte(&glyph->width);
        characterWidth = std::max<int16_t>(0, rightEdge * textsize);
    } else {
        BuiltinFontView view = {};
        if (builtinFontView(textfont, view)) {
            if (uniCode < 32 || uniCode > 127) return 1;
            characterWidth = pgm_read_byte(view.widths + uniCode - 32) * textsize;
        } else {
            characterWidth = 6 * textsize;
        }
    }

    if (textwrapX && cursor_x + characterWidth > width()) {
        cursor_x = 0;
        cursor_y += _smoothFont.isLoaded() ? _smoothFont.fontHeight() : fontHeight();
    }
    if (textwrapY && cursor_y >= height()) cursor_y = 0;

    if (_smoothFont.isLoaded()) drawGlyph(uniCode);
    else cursor_x += drawChar(uniCode, cursor_x, cursor_y, textfont);
    return 1;
}

void Seeed_GFX::setCallback(getColorCallback getCol) {
    getColor = getCol;
}

uint16_t Seeed_GFX::fontsLoaded() const {
    return fontsloaded;
}

bool Seeed_GFX::loadFont(const uint8_t* fontData) {
    smoothFontTarget = this;
    _smoothFont.begin(drawSmoothPixel, drawSmoothHLine, fillSmoothRect,
                      &Seeed_GFX::readSmoothFontPixel);
    return _smoothFont.loadFont(fontData);
}

#if SEEED_GFX_HAS_FS
bool Seeed_GFX::loadFont(const char* path, fs::FS& fileSystem) {
    smoothFontTarget = this;
    _smoothFont.begin(drawSmoothPixel, drawSmoothHLine, fillSmoothRect,
                      &Seeed_GFX::readSmoothFontPixel);
    return _smoothFont.loadFont(path, fileSystem);
}
#endif

uint16_t Seeed_GFX::readSmoothFontPixel(int32_t x, int32_t y) {
    if (!smoothFontTarget) return TFT_BLACK;
    if (smoothFontTarget->getColor) {
        return smoothFontTarget->getColor(static_cast<uint16_t>(x),
                                          static_cast<uint16_t>(y));
    }
    return smoothFontTarget->readPixel(x, y);
}

void Seeed_GFX::unloadFont() {
    _smoothFont.unloadFont();
    if (smoothFontTarget == this) smoothFontTarget = nullptr;
}

bool Seeed_GFX::smoothFontLoaded() const { return _smoothFont.isLoaded(); }

uint16_t Seeed_GFX::drawGlyph(uint16_t code) {
    if (!_smoothFont.isLoaded()) return 0;
    smoothFontTarget = this;
    const uint16_t advance = _smoothFont.drawChar(cursor_x, cursor_y, code,
                                                    static_cast<uint16_t>(textcolor),
                                                    static_cast<uint16_t>(textbgcolor), _fillbg);
    cursor_x += advance;
    return advance;
}

void Seeed_GFX::showFont(uint32_t pageDelay) {
    if (!_smoothFont.isLoaded() || width() <= 0 || height() <= 0) return;
    fillScreen(textbgcolor);
    // SmoothFont owns the glyph enumeration. Its callback bridge targets this
    // display, so it can render the diagnostic grid without exposing internals.
    smoothFontTarget = this;
    _smoothFont.showFont(static_cast<uint16_t>(width()), static_cast<uint16_t>(height()),
                         static_cast<uint16_t>(textcolor), static_cast<uint16_t>(textbgcolor),
                         pageDelay);
}

// Transaction control

void Seeed_GFX::startWrite() {
    if (_panel) _panel->beginWrite();
}

void Seeed_GFX::endWrite() {
    if (_panel) _panel->endWrite();
}

void Seeed_GFX::writecommand(uint8_t command) {
    if (!_panel) return;
    IBus& bus = _panel->driver().bus();
    bus.beginWrite(); bus.writeCommand(command); bus.endWrite();
}

void Seeed_GFX::writedata(uint8_t data) {
    if (!_panel) return;
    IBus& bus = _panel->driver().bus();
    bus.beginWrite(); bus.writeData(data); bus.endWrite();
}

void Seeed_GFX::writendata(const uint8_t* data, uint16_t length) {
    if (!_panel || !data || !length) return;
    IBus& bus = _panel->driver().bus();
    bus.beginWrite(); bus.writeData(data, length); bus.endWrite();
}

void Seeed_GFX::writecommanddata(uint8_t command, const uint8_t* data, uint16_t length) {
    if (!_panel) return;
    IBus& bus = _panel->driver().bus();
    bus.beginWrite(); bus.writeCommand(command);
    if (data && length) bus.writeData(data, length);
    bus.endWrite();
}

uint8_t Seeed_GFX::readcommand8(uint8_t command, uint8_t index) {
    if (!_panel) return 0;
    IBus& bus = _panel->driver().bus();
    bus.beginRead(); bus.writeCommand(command);
    while (index--) (void)bus.readData();
    const uint8_t value = bus.readData(); bus.endRead();
    return value;
}

uint16_t Seeed_GFX::readcommand16(uint8_t command, uint8_t index) {
    if (!_panel) return 0;
    IBus& bus = _panel->driver().bus();
    bus.beginRead(); bus.writeCommand(command);
    while (index--) (void)bus.readData();
    const uint16_t value = static_cast<uint16_t>(bus.readData()) << 8 | bus.readData();
    bus.endRead();
    return value;
}

uint32_t Seeed_GFX::readcommand32(uint8_t command, uint8_t index) {
    uint32_t value = 0;
    if (!_panel) return value;
    IBus& bus = _panel->driver().bus();
    bus.beginRead(); bus.writeCommand(command);
    while (index--) (void)bus.readData();
    for (uint8_t i = 0; i < 4; ++i) value = (value << 8) | bus.readData();
    bus.endRead();
    return value;
}

// DMA support

bool Seeed_GFX::initDMA(bool ctrl_cs) {
    (void)ctrl_cs;
    DMA_Enabled = _panel && _panel->enableDMA(true);
    return DMA_Enabled;
}

void Seeed_GFX::deInitDMA() {
    if (_panel) _panel->enableDMA(false);
    DMA_Enabled = false;
}

void Seeed_GFX::pushImageDMA(int32_t x, int32_t y, int32_t w, int32_t h,
                             uint16_t* data, uint16_t* buffer) {
    // The legacy mutable overload permits in-place clipping/byte swapping.
    _lastDmaTransfer = submitImageAsync(x, y, w, h, data,
                                        buffer ? buffer : data);
}

void Seeed_GFX::pushImageDMA(int32_t x, int32_t y, int32_t w, int32_t h,
                             const uint16_t* data) {
    _lastDmaTransfer = submitImageAsync(x, y, w, h, data, nullptr);
}

DmaTransferResult Seeed_GFX::submitImageAsync(
        int32_t x, int32_t y, int32_t w, int32_t h,
        const uint16_t* data, uint16_t* copyBuffer) {
    if (!_panel) return DmaTransferResult::Unsupported;
    if (!data || w <= 0 || h <= 0) return DmaTransferResult::InvalidArgument;
    if (!DMA_Enabled) {
        pushImage(x, y, w, h, data);
        return DmaTransferResult::SynchronousFallback;
    }
    int32_t rx = x, ry = y, rw = w, rh = h;
    if (!clipAddrWindow(&rx, &ry, &rw, &rh))
        return DmaTransferResult::ClippedOut;

    const int32_t sourceX = rx - (x + _xDatum);
    const int32_t sourceY = ry - (y + _yDatum);
    const size_t pixelCount = static_cast<size_t>(rw) * rh;
    const bool requiresCopy = rw != w || rh != h || _swapBytes;
    if (requiresCopy && !copyBuffer) {
        // A clipped or byte-swapped asynchronous transfer requires a stable,
        // contiguous destination buffer. Preserve const caller storage and
        // perform the operation synchronously when none was supplied.
        pushImage(x, y, w, h, data);
        return DmaTransferResult::SynchronousFallback;
    }
    uint16_t* transfer = copyBuffer
        ? copyBuffer : const_cast<uint16_t*>(data);

    // A previous asynchronous transfer may still own either caller buffer.
    dmaWait();
    if (requiresCopy || copyBuffer) {
        for (int32_t row = 0; row < rh; ++row) {
            for (int32_t col = 0; col < rw; ++col) {
                uint16_t value = data[(sourceY + row) * w + sourceX + col];
                if (_swapBytes)
                    value = static_cast<uint16_t>((value >> 8) | (value << 8));
                transfer[static_cast<size_t>(row) * rw + col] = value;
            }
        }
    }

    _panel->setAddrWindow(rx, ry, rx + rw - 1, ry + rh - 1);
    if (_panel->writePixelsDMA(transfer, pixelCount))
        return DmaTransferResult::Queued;
    _panel->writePixels(transfer, pixelCount);
    return DmaTransferResult::SynchronousFallback;
}

void Seeed_GFX::pushPixelsDMA(uint16_t* data, uint32_t len) {
    _lastDmaTransfer = submitPixelsAsync(data, len);
}

DmaTransferResult Seeed_GFX::submitPixelsAsync(uint16_t* data, uint32_t len) {
    if (!_panel) return DmaTransferResult::Unsupported;
    if (!data || !len) return DmaTransferResult::InvalidArgument;
    if (!DMA_Enabled) {
        pushPixels(data, len);
        return DmaTransferResult::SynchronousFallback;
    }
    dmaWait();
    if (_swapBytes) {
        for (uint32_t i = 0; i < len; ++i) {
            data[i] = static_cast<uint16_t>((data[i] >> 8) | (data[i] << 8));
        }
    }
    if (_panel->writePixelsDMA(data, len)) return DmaTransferResult::Queued;
    _panel->writePixels(data, len);
    return DmaTransferResult::SynchronousFallback;
}

bool Seeed_GFX::dmaBusy() {
    return DMA_Enabled && _panel && _panel->dmaBusy();
}
void Seeed_GFX::dmaWait() {
    while (dmaBusy()) {
        yield();
    }
}

// Color utilities

uint16_t Seeed_GFX::color565(uint8_t red, uint8_t green, uint8_t blue) {
    return ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3);
}

uint16_t Seeed_GFX::color8to16(uint8_t color332) {
    uint8_t r = (color332 >> 5) & 0x07;
    uint8_t g = (color332 >> 2) & 0x07;
    uint8_t b = color332 & 0x03;
    uint16_t rr = (r * 255 / 7) >> 3;
    uint16_t gg = (g * 255 / 7) >> 2;
    uint16_t bb = (b * 255 / 3) >> 3;
    return (rr << 11) | (gg << 5) | bb;
}

uint8_t Seeed_GFX::color16to8(uint16_t color565) {
    uint8_t r = static_cast<uint8_t>(((color565 >> 11) & 0x1F) * 7 / 31);
    uint8_t g = static_cast<uint8_t>(((color565 >> 5) & 0x3F) * 7 / 63);
    uint8_t b = static_cast<uint8_t>((color565 & 0x1F) * 3 / 31);
    return (r << 5) | (g << 2) | b;
}

uint32_t Seeed_GFX::color16to24(uint16_t color565) {
    uint8_t r = (color565 >> 11) & 0x1F;
    uint8_t g = (color565 >> 5) & 0x3F;
    uint8_t b = color565 & 0x1F;
    return ((uint32_t)(r * 255 / 31) << 16) | ((uint32_t)(g * 255 / 63) << 8) | (uint32_t)(b * 255 / 31);
}

uint32_t Seeed_GFX::color24to16(uint32_t color888) {
    uint8_t r = (color888 >> 16) & 0xFF;
    uint8_t g = (color888 >> 8) & 0xFF;
    uint8_t b = color888 & 0xFF;
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

uint16_t Seeed_GFX::alphaBlend(uint8_t alpha, uint16_t fgc, uint16_t bgc) {
    uint16_t fgR = (fgc >> 11) & 0x1F;
    uint16_t fgG = (fgc >> 5) & 0x3F;
    uint16_t fgB = fgc & 0x1F;
    uint16_t bgR = (bgc >> 11) & 0x1F;
    uint16_t bgG = (bgc >> 5) & 0x3F;
    uint16_t bgB = bgc & 0x1F;

    uint16_t r = ((fgR * alpha) + (bgR * (255 - alpha))) / 255;
    uint16_t g = ((fgG * alpha) + (bgG * (255 - alpha))) / 255;
    uint16_t b = ((fgB * alpha) + (bgB * (255 - alpha))) / 255;

    return (r << 11) | (g << 5) | b;
}

uint16_t Seeed_GFX::alphaBlend(uint8_t alpha, uint16_t fgc, uint16_t bgc, uint8_t dither) {
    if (dither) {
        int16_t adjusted = static_cast<int16_t>(alpha) - dither +
                           static_cast<int16_t>(random(2U * dither + 1U));
        adjusted = std::max<int16_t>(0, std::min<int16_t>(255, adjusted));
        alpha = static_cast<uint8_t>(adjusted);
    }
    return alphaBlend(alpha, fgc, bgc);
}

uint32_t Seeed_GFX::alphaBlend24(uint8_t alpha, uint32_t fgc, uint32_t bgc, uint8_t dither) {
    if (dither) {
        int16_t adjusted = static_cast<int16_t>(alpha) - dither +
                           static_cast<int16_t>(random(2U * dither + 1U));
        adjusted = std::max<int16_t>(0, std::min<int16_t>(255, adjusted));
        alpha = static_cast<uint8_t>(adjusted);
    }
    uint8_t fgR = (fgc >> 16) & 0xFF, fgG = (fgc >> 8) & 0xFF, fgB = fgc & 0xFF;
    uint8_t bgR = (bgc >> 16) & 0xFF, bgG = (bgc >> 8) & 0xFF, bgB = bgc & 0xFF;
    uint8_t r = ((fgR * alpha) + (bgR * (255 - alpha))) / 255;
    uint8_t g = ((fgG * alpha) + (bgG * (255 - alpha))) / 255;
    uint8_t b = ((fgB * alpha) + (bgB * (255 - alpha))) / 255;
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

// Configuration

void Seeed_GFX::setAttribute(uint8_t id, uint8_t a) {
    switch (id) {
        case 1: _cp437 = a; break;      // CP437
        case 2: _utf8 = a; break;       // UTF8
        case 3: _psram_enable = a; break; // PSRAM
    }
}

uint8_t Seeed_GFX::getAttribute(uint8_t id) {
    switch (id) {
        case 1: return _cp437;
        case 2: return _utf8;
        case 3: return _psram_enable;
    }
    return 0;
}

// Touch

bool Seeed_GFX::getTouch(int32_t *x, int32_t *y, uint16_t threshold) {
    if (!_touch || !x || !y) return false;
    if (_touch->supportsRawRead() && _touch->rawPressure() <= threshold) return false;
    TouchPoint pt;
    if (_touch->read(pt)) {
        *x = pt.x;
        *y = pt.y;
        return true;
    }
    return false;
}

uint8_t Seeed_GFX::getTouchPoints(TouchPoint* points, uint8_t maxPoints) {
    return _touch ? _touch->readMulti(points, maxPoints) : 0;
}

uint8_t Seeed_GFX::getTouchGesture() const {
    return _touch ? _touch->gesture() : 0;
}

uint8_t Seeed_GFX::touchPointCapacity() const {
    return _touch ? _touch->maxPoints() : 0;
}

bool Seeed_GFX::getTouchRaw(uint16_t *x, uint16_t *y) {
    return _touch && x && y && _touch->readRaw(x, y);
}

uint16_t Seeed_GFX::getTouchRawZ() {
    return _touch ? _touch->rawPressure() : 0;
}

void Seeed_GFX::convertRawXY(uint16_t* x, uint16_t* y) {
    if (_touch && x && y) _touch->convertRawXY(x, y);
}

void Seeed_GFX::calibrateTouch(uint16_t* parameters, uint32_t color_fg,
                               uint32_t color_bg, uint8_t size) {
    if (!parameters) return;
    for (uint8_t i = 0; i < 5; ++i) parameters[i] = 0;
    if (!_touch || !_panel || !_touch->supportsRawRead() ||
        !_touch->supportsCalibration() || width() < 40 || height() < 40) return;

    struct RawPoint { uint16_t x; uint16_t y; } points[4] = {};
    const int32_t margin = std::max<int32_t>(12, std::min<int32_t>(width(), height()) / 10);
    const int32_t targetX[4] = {margin, width() - 1 - margin,
                                margin, width() - 1 - margin};
    const int32_t targetY[4] = {margin, margin,
                                height() - 1 - margin, height() - 1 - margin};
    const int32_t radius = size ? size : 4;
    fillScreen(color_bg);

    for (uint8_t p = 0; p < 4; ++p) {
        drawCircle(targetX[p], targetY[p], radius + 2, color_fg);
        drawFastHLine(targetX[p] - radius, targetY[p], radius * 2 + 1, color_fg);
        drawFastVLine(targetX[p], targetY[p] - radius, radius * 2 + 1, color_fg);

        const uint32_t pressStart = millis();
        while (_touch->rawPressure() == 0) {
            if ((uint32_t)(millis() - pressStart) >= 15000U) {
                fillScreen(color_bg);
                return;
            }
            yield();
        }

        uint32_t sumX = 0, sumY = 0;
        uint8_t samples = 0;
        const uint32_t sampleStart = millis();
        while (samples < 8 && (uint32_t)(millis() - sampleStart) < 2000U) {
            uint16_t rx = 0, ry = 0;
            if (_touch->readRaw(&rx, &ry)) {
                sumX += rx;
                sumY += ry;
                ++samples;
            }
            delay(5);
        }
        if (samples == 0) {
            fillScreen(color_bg);
            return;
        }
        points[p].x = static_cast<uint16_t>(sumX / samples);
        points[p].y = static_cast<uint16_t>(sumY / samples);

        fillCircle(targetX[p], targetY[p], radius + 3, color_bg);
        const uint32_t releaseStart = millis();
        while (_touch->rawPressure() != 0 &&
               (uint32_t)(millis() - releaseStart) < 3000U) yield();
    }

    const int32_t horizontalX = abs((int32_t)points[1].x + points[3].x -
                                    points[0].x - points[2].x);
    const int32_t horizontalY = abs((int32_t)points[1].y + points[3].y -
                                    points[0].y - points[2].y);
    const bool rotated = horizontalY > horizontalX;

    const int32_t left = rotated ? ((int32_t)points[0].y + points[2].y) / 2
                                 : ((int32_t)points[0].x + points[2].x) / 2;
    const int32_t right = rotated ? ((int32_t)points[1].y + points[3].y) / 2
                                  : ((int32_t)points[1].x + points[3].x) / 2;
    const int32_t top = rotated ? ((int32_t)points[0].x + points[1].x) / 2
                                : ((int32_t)points[0].y + points[1].y) / 2;
    const int32_t bottom = rotated ? ((int32_t)points[2].x + points[3].x) / 2
                                   : ((int32_t)points[2].y + points[3].y) / 2;

    auto makeAxis = [margin](int32_t edge0, int32_t edge1, int32_t dimension,
                             uint16_t& origin, uint16_t& range, bool& inverted) -> bool {
        const int32_t screenSpan = dimension - 1 - margin * 2;
        if (screenSpan <= 0 || abs(edge1 - edge0) < 20) return false;
        const int32_t delta = edge1 - edge0;
        int32_t raw0 = edge0 - delta * margin / screenSpan;
        int32_t raw1 = edge1 + delta * margin / screenSpan;
        inverted = raw0 > raw1;
        int32_t low = inverted ? raw1 : raw0;
        int32_t high = inverted ? raw0 : raw1;
        low = std::max<int32_t>(0, std::min<int32_t>(65535, low));
        high = std::max<int32_t>(0, std::min<int32_t>(65535, high));
        if (high <= low) return false;
        origin = static_cast<uint16_t>(low);
        range = static_cast<uint16_t>(high - low);
        return range != 0;
    };

    bool invertX = false, invertY = false;
    if (!makeAxis(left, right, width(), parameters[0], parameters[1], invertX) ||
        !makeAxis(top, bottom, height(), parameters[2], parameters[3], invertY)) {
        for (uint8_t i = 0; i < 5; ++i) parameters[i] = 0;
        fillScreen(color_bg);
        return;
    }
    parameters[4] = (rotated ? 0x01 : 0x00) |
                    (invertX ? 0x02 : 0x00) |
                    (invertY ? 0x04 : 0x00);
    _touch->setCalibration(parameters);
    fillScreen(color_bg);
}

void Seeed_GFX::setTouch(uint16_t* parameters) {
    if (_touch && parameters && _touch->supportsCalibration()) {
        _touch->setCalibration(parameters);
    }
}

bool Seeed_GFX::attachTouch(ITouch& touch, IBus& bus) {
    if (!touch.begin(bus)) {
        _lastResult = GfxResult(GfxError::TouchInitFailed, "touch initialization failed");
        return false;
    }
    _touch = &touch;
    if (_panel) _touch->setDisplayRotation(_panel->rotation());
    _lastResult = GfxResult::success();
    return true;
}

void Seeed_GFX::detachTouch() {
    _touch = nullptr;
}

// Internal helpers

void Seeed_GFX::_beginWrite() {
    if (_panel) _panel->beginWrite();
}

void Seeed_GFX::_endWrite() {
    if (_panel) _panel->endWrite();
}

void Seeed_GFX::_setAddrWindow(int32_t x, int32_t y, int32_t w, int32_t h) {
    if (!_panel || !clipAddrWindow(&x, &y, &w, &h)) return;
    _panel->setAddrWindow((uint16_t)x, (uint16_t)y,
                          (uint16_t)(x + w - 1), (uint16_t)(y + h - 1));
}

// Viewport

void Seeed_GFX::setViewport(int32_t x, int32_t y, int32_t w, int32_t h, bool vpDatum) {
    if (!_panel || w <= 0 || h <= 0) {
        _vpOoB = true;
        return;
    }
    int32_t x1 = max<int32_t>(0, x);
    int32_t y1 = max<int32_t>(0, y);
    int32_t x2 = min<int32_t>(_panel->width(), x + w);
    int32_t y2 = min<int32_t>(_panel->height(), y + h);
    _vpOoB = (x1 >= x2 || y1 >= y2);
    if (_vpOoB) return;
    _vpX = x1; _vpY = y1; _vpW = x2; _vpH = y2;
    _vpDatum = vpDatum;
    _xDatum = vpDatum ? _vpX : 0;
    _yDatum = vpDatum ? _vpY : 0;
}

bool Seeed_GFX::checkViewport(int32_t x, int32_t y, int32_t w, int32_t h) {
    if (_vpOoB || w <= 0 || h <= 0) return false;
    x += _xDatum; y += _yDatum;
    return x < _vpW && y < _vpH && (x + w) > _vpX && (y + h) > _vpY;
}
int32_t Seeed_GFX::getViewportX() const { return _vpX; }
int32_t Seeed_GFX::getViewportY() const { return _vpY; }
int32_t Seeed_GFX::getViewportWidth() const { return _vpOoB ? 0 : _vpW - _vpX; }
int32_t Seeed_GFX::getViewportHeight() const { return _vpOoB ? 0 : _vpH - _vpY; }
bool Seeed_GFX::getViewportDatum() const { return _vpDatum; }
void Seeed_GFX::frameViewport(uint16_t color, int32_t thickness) {
    if (_vpOoB || thickness <= 0) return;
    const int32_t savedX = _xDatum, savedY = _yDatum;
    _xDatum = _yDatum = 0;
    for (int32_t i = 0; i < thickness; ++i) {
        drawRect(_vpX + i, _vpY + i, (_vpW - _vpX) - 2 * i,
                 (_vpH - _vpY) - 2 * i, color);
    }
    _xDatum = savedX; _yDatum = savedY;
}
void Seeed_GFX::resetViewport() {
    if (!_panel) { _vpOoB = true; return; }
    _vpX = _vpY = 0;
    _vpW = _panel->width(); _vpH = _panel->height();
    _xDatum = _yDatum = 0; _vpDatum = false; _vpOoB = false;
}
bool Seeed_GFX::clipAddrWindow(int32_t* x, int32_t* y, int32_t* w, int32_t* h) {
    if (!x || !y || !w || !h || _vpOoB) return false;
    *x += _xDatum; *y += _yDatum;
    if (*x < _vpX) { *w -= _vpX - *x; *x = _vpX; }
    if (*y < _vpY) { *h -= _vpY - *y; *y = _vpY; }
    if (*x + *w > _vpW) *w = _vpW - *x;
    if (*y + *h > _vpH) *h = _vpH - *y;
    return *w > 0 && *h > 0;
}
bool Seeed_GFX::clipWindow(int32_t* xs, int32_t* ys, int32_t* xe, int32_t* ye) {
    if (!xs || !ys || !xe || !ye || _vpOoB) return false;
    *xs += _xDatum; *xe += _xDatum; *ys += _yDatum; *ye += _yDatum;
    *xs = max(*xs, _vpX); *ys = max(*ys, _vpY);
    *xe = min(*xe, _vpW - 1); *ye = min(*ye, _vpH - 1);
    return *xs <= *xe && *ys <= *ye;
}
void Seeed_GFX::setOrigin(int32_t x, int32_t y) { _xDatum = x; _yDatum = y; }
int32_t Seeed_GFX::getOriginX() const { return _xDatum; }
int32_t Seeed_GFX::getOriginY() const { return _yDatum; }
void Seeed_GFX::setPivot(int16_t x, int16_t y) { _xPivot = x; _yPivot = y; }
int16_t Seeed_GFX::getPivotX() const { return _xPivot; }
int16_t Seeed_GFX::getPivotY() const { return _yPivot; }
void Seeed_GFX::readRect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t* data) {
    if (!data || !_panel || !_panel->capabilities().readback) return;
    for (int32_t yy = 0; yy < h; ++yy)
        for (int32_t xx = 0; xx < w; ++xx)
            *data++ = readPixel(x + xx, y + yy);
}
void Seeed_GFX::pushRect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t* data) {
    pushImage(x, y, w, h, data);
}
void Seeed_GFX::drawXBitmap(int16_t x, int16_t y, const uint8_t* bitmap, int16_t w, int16_t h, uint16_t fg) {
    if (!bitmap) return;
    const int16_t byteWidth = (w + 7) / 8;
    for (int16_t yy = 0; yy < h; ++yy)
        for (int16_t xx = 0; xx < w; ++xx)
            if (pgm_read_byte(bitmap + yy * byteWidth + xx / 8) & (1 << (xx & 7))) drawPixel(x + xx, y + yy, fg);
}
void Seeed_GFX::drawXBitmap(int16_t x, int16_t y, const uint8_t* bitmap, int16_t w, int16_t h, uint16_t fg, uint16_t bg) {
    if (!bitmap) return;
    const int16_t byteWidth = (w + 7) / 8;
    for (int16_t yy = 0; yy < h; ++yy)
        for (int16_t xx = 0; xx < w; ++xx)
            drawPixel(x + xx, y + yy,
                (pgm_read_byte(bitmap + yy * byteWidth + xx / 8) & (1 << (xx & 7))) ? fg : bg);
}
void Seeed_GFX::pushImage(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t* data, uint16_t transparent) {
    if (!data || !_panel || w <= 0 || h <= 0) return;
    for (int32_t yy = 0; yy < h; ++yy)
        for (int32_t xx = 0; xx < w; ++xx) {
            const uint16_t color = data[yy * w + xx];
            if (color != transparent) {
                const uint16_t output = _swapBytes
                    ? static_cast<uint16_t>((color >> 8) | (color << 8)) : color;
                drawPixel(x + xx, y + yy, output);
            }
        }
}
void Seeed_GFX::pushImage(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t* data) {
    if (!data || !_panel || w <= 0 || h <= 0) return;
    for (int32_t row = 0; row < h; ++row) {
        int32_t rx = x, ry = y + row, rw = w, rh = 1;
        if (!clipAddrWindow(&rx, &ry, &rw, &rh)) continue;
        const int32_t sourceOffset = row * w + (rx - (x + _xDatum));
        _panel->setAddrWindow(rx, ry, rx + rw - 1, ry);
        if (!_swapBytes) {
            _panel->writePixels(data + sourceOffset, rw);
        } else {
            for (int32_t col = 0; col < rw; ++col) {
                const uint16_t value = data[sourceOffset + col];
                _panel->writePixel(static_cast<uint16_t>((value >> 8) | (value << 8)));
            }
        }
    }
}
void Seeed_GFX::pushImage(int32_t x, int32_t y, int32_t w, int32_t h,
                          uint8_t* data, bool bpp8, uint16_t* cmap) {
    pushImage(x, y, w, h, static_cast<const uint8_t*>(data), bpp8, cmap);
}
void Seeed_GFX::pushImage(int32_t x, int32_t y, int32_t w, int32_t h,
                          uint8_t* data, uint8_t transparent, bool bpp8,
                          uint16_t* cmap) {
    if (!data || w <= 0 || h <= 0) return;
    for (int32_t yy = 0; yy < h; ++yy) {
        for (int32_t xx = 0; xx < w; ++xx) {
            uint8_t index;
            if (bpp8) index = data[yy * w + xx];
            else index = (data[yy * ((w + 7) / 8) + xx / 8] >> (7 - (xx & 7))) & 1;
            if (index == transparent) continue;
            const uint16_t color = cmap ? cmap[index] : (bpp8 ? color8to16(index)
                                                               : (index ? TFT_WHITE : TFT_BLACK));
            drawPixel(x + xx, y + yy, color);
        }
    }
}
void Seeed_GFX::pushImage(int32_t x, int32_t y, int32_t w, int32_t h,
                          const uint8_t* data, bool bpp8, uint16_t* cmap) {
    if (!data || w <= 0 || h <= 0) return;
    for (int32_t yy = 0; yy < h; ++yy) {
        for (int32_t xx = 0; xx < w; ++xx) {
            const uint8_t index = bpp8 ? data[yy * w + xx]
                : static_cast<uint8_t>((data[yy * ((w + 7) / 8) + xx / 8] >>
                                        (7 - (xx & 7))) & 1);
            const uint16_t color = cmap ? cmap[index] : (bpp8 ? color8to16(index)
                                                               : (index ? TFT_WHITE : TFT_BLACK));
            drawPixel(x + xx, y + yy, color);
        }
    }
}

bool Seeed_GFX::pushImage4BPP(int32_t x, int32_t y,
                              int32_t w, int32_t h,
                              const uint8_t* data,
                              bool dataInProgmem) {
    return _panel && _panel->pushImage4BPP(
        x + _xDatum, y + _yDatum, w, h, data, dataInProgmem);
}

bool Seeed_GFX::pushImage4BPPRotatedCW(int32_t x, int32_t y,
                                       int32_t w, int32_t h,
                                       const uint8_t* data,
                                       bool dataInProgmem) {
    return _panel && _panel->pushImage4BPPRotatedCW(
        x + _xDatum, y + _yDatum, w, h, data, dataInProgmem);
}

void Seeed_GFX::pushMaskedImage(int32_t x, int32_t y, int32_t w, int32_t h,
                                uint16_t* img, uint8_t* mask) {
    if (!img || !mask || w <= 0 || h <= 0) return;
    const int32_t maskStride = (w + 7) / 8;
    for (int32_t yy = 0; yy < h; ++yy)
        for (int32_t xx = 0; xx < w; ++xx)
            if (mask[yy * maskStride + xx / 8] & (0x80 >> (xx & 7)))
                drawPixel(x + xx, y + yy, img[yy * w + xx]);
}
void Seeed_GFX::readRectRGB(int32_t x, int32_t y, int32_t w, int32_t h,
                            uint8_t* data) {
    if (!data || !_panel || !_panel->capabilities().readback || w <= 0 || h <= 0) return;
    for (int32_t yy = 0; yy < h; ++yy) {
        for (int32_t xx = 0; xx < w; ++xx) {
            const uint32_t rgb = color16to24(readPixel(x + xx, y + yy));
            *data++ = static_cast<uint8_t>(rgb >> 16);
            *data++ = static_cast<uint8_t>(rgb >> 8);
            *data++ = static_cast<uint8_t>(rgb);
        }
    }
}
void Seeed_GFX::pushColors(uint16_t* data, uint32_t len, bool swap) {
    if (!data || !_panel) return;
    if (!swap) { _panel->writePixels(data, len); return; }
    for (uint32_t i = 0; i < len; ++i) {
        const uint16_t value = (data[i] >> 8) | (data[i] << 8);
        _panel->writePixel(value);
    }
}
void Seeed_GFX::pushColors(uint8_t* data, uint32_t len) {
    if (data && _panel) _panel->writeBytes(data, len);
}
void Seeed_GFX::drawRectVGradient(int16_t x, int16_t y, int16_t w, int16_t h,
                                  uint32_t color1, uint32_t color2) {
    if (w <= 0 || h <= 0) return;
    const uint16_t c1 = static_cast<uint16_t>(color1);
    const uint16_t c2 = static_cast<uint16_t>(color2);
    for (int32_t row = 0; row < h; ++row) {
        const uint8_t alpha = h <= 1 ? 0 : static_cast<uint8_t>((row * 255) / (h - 1));
        drawFastHLine(x, y + row, w, alphaBlend(alpha, c2, c1));
    }
}
void Seeed_GFX::drawRectHGradient(int16_t x, int16_t y, int16_t w, int16_t h,
                                  uint32_t color1, uint32_t color2) {
    if (w <= 0 || h <= 0) return;
    const uint16_t c1 = static_cast<uint16_t>(color1);
    const uint16_t c2 = static_cast<uint16_t>(color2);
    for (int32_t col = 0; col < w; ++col) {
        const uint8_t alpha = w <= 1 ? 0 : static_cast<uint8_t>((col * 255) / (w - 1));
        drawFastVLine(x + col, y, h, alphaBlend(alpha, c2, c1));
    }
}
void Seeed_GFX::writeColor(uint16_t color, uint32_t len) { if (_panel) _panel->writeFill(color, len); }
void Seeed_GFX::setWindow(int32_t xs, int32_t ys, int32_t xe, int32_t ye) {
    if (!_panel) return;
    if (!clipWindow(&xs, &ys, &xe, &ye)) return;
    _panel->setAddrWindow(static_cast<uint16_t>(xs), static_cast<uint16_t>(ys),
                          static_cast<uint16_t>(xe), static_cast<uint16_t>(ye));
}
void Seeed_GFX::setAddrWindow(int32_t x, int32_t y, int32_t w, int32_t h) {
    _setAddrWindow(x, y, w, h);
}
uint16_t Seeed_GFX::drawPixel(int32_t x, int32_t y, uint32_t color, uint8_t alpha,
                              uint32_t bgColor) {
    const uint16_t bg = (bgColor == 0x00FFFFFF && _panel)
        ? readPixel(x, y) : static_cast<uint16_t>(bgColor);
    const uint16_t blended = alphaBlend(alpha, static_cast<uint16_t>(color), bg);
    drawPixel(x, y, blended);
    return blended;
}
void Seeed_GFX::drawArc(int32_t x, int32_t y, int32_t r, int32_t ir,
                        uint32_t startAngle, uint32_t endAngle, uint32_t fg,
                        uint32_t bg, bool smoothArc) {
    if (r < 0 || ir < 0 || ir > r) return;
    float sweep = static_cast<float>(endAngle) - static_cast<float>(startAngle);
    while (sweep < 0.0f) sweep += 360.0f;
    if (sweep == 0.0f && endAngle != startAngle) sweep = 360.0f;
    const float start = normalizedAngle(static_cast<float>(startAngle));
    const float outer = static_cast<float>(r) + 0.5f;
    const float inner = std::max(0.0f, static_cast<float>(ir) - 0.5f);
    const uint8_t samples = smoothArc ? 4 : 1;
    for (int32_t py = y - r - 1; py <= y + r + 1; ++py) {
        for (int32_t px = x - r - 1; px <= x + r + 1; ++px) {
            const uint8_t coverage = sampleCoverage(px, py,
                [=](float sx, float sy) {
                    const float dx = sx - x;
                    const float dy = sy - y;
                    const float distance2 = dx * dx + dy * dy;
                    if (distance2 > outer * outer || distance2 < inner * inner)
                        return false;
                    const float angle = normalizedAngle(
                        atan2f(dy, dx) * 57.2957795131f);
                    return angleInSweep(angle, start, sweep);
                }, samples);
            drawCoveredPixel(*this, px, py, fg, bg, coverage);
        }
    }
}
void Seeed_GFX::drawSmoothArc(int32_t x, int32_t y, int32_t r, int32_t ir,
                              uint32_t start, uint32_t end, uint32_t fg,
                              uint32_t bg, bool roundEnds) {
    drawArc(x, y, r, ir, start, end, fg, bg, true);
    if (roundEnds && r >= ir) {
        const float mid = (r + ir) * 0.5f;
        const float radius = std::max(0.5f, (r - ir + 1) * 0.5f);
        for (uint32_t a : {start, end}) {
            const float rad = static_cast<float>(a) * 0.01745329252f;
            drawSpot(x + cosf(rad) * mid, y + sinf(rad) * mid, radius, fg, bg);
        }
    }
}
void Seeed_GFX::drawSmoothCircle(int32_t x, int32_t y, int32_t r,
                                 uint32_t fg, uint32_t bg) {
    if (r < 0) return;
    drawArc(x, y, r, std::max<int32_t>(0, r - 1), 0, 360, fg, bg, true);
}
void Seeed_GFX::fillSmoothCircle(int32_t x, int32_t y, int32_t r,
                                 uint32_t color, uint32_t bg) {
    drawSpot(static_cast<float>(x), static_cast<float>(y),
             static_cast<float>(r), color, bg);
}
void Seeed_GFX::drawSmoothRoundRect(int32_t x, int32_t y, int32_t r, int32_t ir,
                                    int32_t w, int32_t h, uint32_t fg,
                                    uint32_t bg, uint8_t quadrants) {
    if (w <= 0 || h <= 0 || r < 0 || ir < 0 || ir > r || quadrants == 0) return;
    const float thickness = std::max(1.0f, static_cast<float>(r - ir));
    for (int32_t py = y - 1; py <= y + h; ++py) {
        for (int32_t px = x - 1; px <= x + w; ++px) {
            const uint8_t coverage = sampleCoverage(px, py, [=](float sx, float sy) {
                if (!roundedRectContains(sx, sy, x, y, w, h, r)) return false;
                const bool inInner = roundedRectContains(
                    sx, sy, x + thickness, y + thickness,
                    w - 2.0f * thickness, h - 2.0f * thickness, ir);
                if (inInner) return false;
                // Quadrant mask follows TFT_eSPI: TL=1, TR=2, BR=4, BL=8.
                const bool left = sx < x + r;
                const bool right = sx > x + w - r;
                const bool top = sy < y + r;
                const bool bottom = sy > y + h - r;
                if (left && top) return (quadrants & 0x1U) != 0;
                if (right && top) return (quadrants & 0x2U) != 0;
                if (right && bottom) return (quadrants & 0x4U) != 0;
                if (left && bottom) return (quadrants & 0x8U) != 0;
                return true;
            });
            drawCoveredPixel(*this, px, py, fg, bg, coverage);
        }
    }
}
void Seeed_GFX::fillSmoothRoundRect(int32_t x, int32_t y, int32_t w, int32_t h,
                                    int32_t radius, uint32_t color, uint32_t bg) {
    if (w <= 0 || h <= 0 || radius < 0) return;
    for (int32_t py = y - 1; py <= y + h; ++py) {
        for (int32_t px = x - 1; px <= x + w; ++px) {
            const uint8_t coverage = sampleCoverage(px, py, [=](float sx, float sy) {
                return roundedRectContains(sx, sy, x, y, w, h, radius);
            });
            drawCoveredPixel(*this, px, py, color, bg, coverage);
        }
    }
}
void Seeed_GFX::drawSpot(float x, float y, float r, uint32_t fg, uint32_t bg) {
    if (r < 0.0f) return;
    const int32_t left = static_cast<int32_t>(floorf(x - r - 1.0f));
    const int32_t right = static_cast<int32_t>(ceilf(x + r + 1.0f));
    const int32_t top = static_cast<int32_t>(floorf(y - r - 1.0f));
    const int32_t bottom = static_cast<int32_t>(ceilf(y + r + 1.0f));
    const float radius2 = r * r;
    for (int32_t py = top; py <= bottom; ++py) {
        for (int32_t px = left; px <= right; ++px) {
            const uint8_t coverage = sampleCoverage(px, py, [=](float sx, float sy) {
                const float dx = sx - x;
                const float dy = sy - y;
                return dx * dx + dy * dy <= radius2;
            });
            drawCoveredPixel(*this, px, py, fg, bg, coverage);
        }
    }
}
void Seeed_GFX::drawWideLine(float ax, float ay, float bx, float by, float wd,
                             uint32_t fg, uint32_t bg) {
    drawWedgeLine(ax, ay, bx, by, wd, wd, fg, bg);
}
void Seeed_GFX::drawWedgeLine(float ax, float ay, float bx, float by, float aw,
                              float bw, uint32_t fg, uint32_t bg) {
    if (aw < 0.0f || bw < 0.0f) return;
    const float maxRadius = std::max(aw, bw) * 0.5f;
    const int32_t left = static_cast<int32_t>(floorf(std::min(ax, bx) - maxRadius - 1.0f));
    const int32_t right = static_cast<int32_t>(ceilf(std::max(ax, bx) + maxRadius + 1.0f));
    const int32_t top = static_cast<int32_t>(floorf(std::min(ay, by) - maxRadius - 1.0f));
    const int32_t bottom = static_cast<int32_t>(ceilf(std::max(ay, by) + maxRadius + 1.0f));
    const float vx = bx - ax;
    const float vy = by - ay;
    const float length2 = vx * vx + vy * vy;
    for (int32_t py = top; py <= bottom; ++py) {
        for (int32_t px = left; px <= right; ++px) {
            const uint8_t coverage = sampleCoverage(px, py, [=](float sx, float sy) {
                float t = length2 > 0.0f
                    ? ((sx - ax) * vx + (sy - ay) * vy) / length2 : 0.0f;
                t = std::max(0.0f, std::min(1.0f, t));
                const float cx = ax + vx * t;
                const float cy = ay + vy * t;
                const float radius = (aw + (bw - aw) * t) * 0.5f;
                const float dx = sx - cx;
                const float dy = sy - cy;
                return dx * dx + dy * dy <= radius * radius;
            });
            drawCoveredPixel(*this, px, py, fg, bg, coverage);
        }
    }
}
int16_t Seeed_GFX::drawChar(uint16_t uniCode, int32_t x, int32_t y, uint8_t font) {
    if (!uniCode) return 0;

    if (gfxFont && font == 1) {
        drawCharGfx(x, y, uniCode, textcolor, textbgcolor, textsize);
        if ((uniCode >= pgm_read_word(&gfxFont->first)) && (uniCode <= pgm_read_word(&gfxFont->last))) {
            uint16_t c2 = uniCode - pgm_read_word(&gfxFont->first);
            GFXglyph *glyph = &(static_cast<GFXglyph*>(SEEED_PGM_PTR(&gfxFont->glyph))[c2]);
            return pgm_read_byte(&glyph->xAdvance) * textsize;
        }
        return 0;
    }

    BuiltinFontView view = {};
    if (builtinFontView(font, view)) {
        if (uniCode < 32 || uniCode > 127) return 0;
        const uint8_t glyphIndex = static_cast<uint8_t>(uniCode - 32);
        const uint8_t glyphWidth = pgm_read_byte(view.widths + glyphIndex);
        const uint8_t* glyph = static_cast<const uint8_t*>(
            pgm_read_ptr(view.glyphs + glyphIndex));
        if (!glyph || glyphWidth == 0) return 0;

        const bool opaque = textcolor != textbgcolor;
        if (!view.rle) {
            const uint8_t bytesPerRow = static_cast<uint8_t>((glyphWidth + 7U) / 8U);
            for (uint8_t row = 0; row < view.height; ++row) {
                for (uint8_t col = 0; col < glyphWidth; ++col) {
                    const uint8_t bits = pgm_read_byte(glyph +
                        static_cast<size_t>(row) * bytesPerRow + (col >> 3));
                    const bool set = (bits & (0x80U >> (col & 7))) != 0;
                    if (set || opaque) {
                        const uint32_t color = set ? textcolor : textbgcolor;
                        if (textsize == 1) drawPixel(x + col, y + row, color);
                        else fillRect(x + col * textsize, y + row * textsize,
                                      textsize, textsize, color);
                    }
                }
            }
        } else {
            const uint32_t pixelCount = static_cast<uint32_t>(glyphWidth) * view.height;
            uint32_t pixel = 0;
            while (pixel < pixelCount) {
                uint8_t encoded = pgm_read_byte(glyph++);
                const bool set = (encoded & 0x80U) != 0;
                uint16_t run = static_cast<uint16_t>(encoded & 0x7FU) + 1U;
                if (run > pixelCount - pixel)
                    run = static_cast<uint16_t>(pixelCount - pixel);
                while (run--) {
                    const int32_t col = pixel % glyphWidth;
                    const int32_t row = pixel / glyphWidth;
                    if (set || opaque) {
                        const uint32_t color = set ? textcolor : textbgcolor;
                        if (textsize == 1) drawPixel(x + col, y + row, color);
                        else fillRect(x + col * textsize, y + row * textsize,
                                      textsize, textsize, color);
                    }
                    ++pixel;
                }
            }
        }
        return glyphWidth * textsize;
    }

    // GLCD font
    if (uniCode > 255) return 0;
    drawCharGlcd(x, y, uniCode, textcolor, textbgcolor, textsize);
    return 6 * textsize;
}

int16_t Seeed_GFX::drawChar(uint16_t uniCode, int32_t x, int32_t y) {
    return drawChar(uniCode, x, y, textfont);
}

void Seeed_GFX::drawChar(int32_t x, int32_t y, uint16_t c, uint32_t color, uint32_t bg, uint8_t size) {
    if (gfxFont) {
        drawCharGfx(x, y, c, color, bg, size);
    } else {
        drawCharGlcd(x, y, c, color, bg, size);
    }
}

// drawCharGlcd - GLCD 5x7 font rendering

void Seeed_GFX::drawCharGlcd(int32_t x, int32_t y, uint16_t c, uint32_t color, uint32_t bg, uint8_t size) {
    if (c > 255) return;

    // Handle CP437 code page offset
    if (!_cp437 && c > 175) c++;

    bool fillbg = (bg != color);

    for (int8_t i = 0; i < 6; i++) {
        uint8_t line;
        if (i == 5)
            line = 0x0;  // Spacing column
        else
            line = pgm_read_byte(&glcd_font[0] + (c * 5) + i);

        if (size == 1 && !fillbg) {
            // Fast path: size=1, no background fill
            for (int8_t j = 0; j < 8; j++) {
                if (line & 0x1) drawPixel(x + i, y + j, color);
                line >>= 1;
            }
        } else {
            // Scaled or background fill
            for (int8_t j = 0; j < 8; j++) {
                if (line & 0x1) {
                    if (size == 1) drawPixel(x + i, y + j, color);
                    else fillRect(x + (i * size), y + (j * size), size, size, color);
                } else if (fillbg) {
                    if (size == 1) drawPixel(x + i, y + j, bg);
                    else fillRect(x + (i * size), y + (j * size), size, size, bg);
                }
                line >>= 1;
            }
        }
    }
}

// drawCharGfx - GFX FreeFont rendering

void Seeed_GFX::drawCharGfx(int32_t x, int32_t y, uint16_t c, uint32_t color, uint32_t bg, uint8_t size) {
    if (!gfxFont) return;

    // Filter out characters not present in font
    if ((c < pgm_read_word(&gfxFont->first)) || (c > pgm_read_word(&gfxFont->last))) return;

    c -= pgm_read_word(&gfxFont->first);
    GFXglyph *glyph = &(static_cast<GFXglyph*>(SEEED_PGM_PTR(&gfxFont->glyph))[c]);
    uint8_t *bitmap = static_cast<uint8_t*>(SEEED_PGM_PTR(&gfxFont->bitmap));

    uint32_t bo = pgm_read_word(&glyph->bitmapOffset);
    uint8_t w = pgm_read_byte(&glyph->width),
            h = pgm_read_byte(&glyph->height);
    int8_t xo = pgm_read_byte(&glyph->xOffset),
           yo = pgm_read_byte(&glyph->yOffset);

    uint8_t xx, yy, bits = 0, bit = 0;
    int16_t xo16 = 0, yo16 = 0;

    if (size > 1) {
        xo16 = xo;
        yo16 = yo;
    }

    bool fillbg = (bg != color);

    // If background fill is enabled, draw background rectangle first
    if (fillbg) {
        fillRect(x + xo * size, y + yo * size, w * size, h * size, bg);
    }

    // Render glyph bitmap
    uint16_t hpc = 0; // Horizontal foreground pixel count
    for (yy = 0; yy < h; yy++) {
        for (xx = 0; xx < w; xx++) {
            if (bit == 0) {
                bits = pgm_read_byte(&bitmap[bo++]);
                bit = 0x80;
            }
            if (bits & bit) hpc++;
            else {
                if (hpc) {
                    if (size == 1) drawFastHLine(x + xo + xx - hpc, y + yo + yy, hpc, color);
                    else fillRect(x + (xo16 + xx - hpc) * size, y + (yo16 + yy) * size, size * hpc, size, color);
                    hpc = 0;
                }
            }
            bit >>= 1;
        }
        // Draw remaining pixels for this line
        if (hpc) {
            if (size == 1) drawFastHLine(x + xo + xx - hpc, y + yo + yy, hpc, color);
            else fillRect(x + (xo16 + xx - hpc) * size, y + (yo16 + yy) * size, size * hpc, size, color);
            hpc = 0;
        }
    }
}
uint16_t Seeed_GFX::decodeUTF8(uint8_t *buf, uint16_t *index, uint16_t remaining) {
    if (!buf || !index || remaining == 0) return 0;
    uint8_t c = buf[*index];
    (*index)++;

    if (!_utf8) return c;

    // Single-byte (ASCII)
    if (c < 0x80) return c;

    // Two-byte sequence
    if ((c & 0xE0) == 0xC0 && remaining >= 2) {
        uint8_t c2 = buf[*index];
        if ((c2 & 0xC0) != 0x80 || c < 0xC2) return 0xFFFD;
        (*index)++;
        return ((c & 0x1F) << 6) | (c2 & 0x3F);
    }

    // Three-byte sequence
    if ((c & 0xF0) == 0xE0 && remaining >= 3) {
        uint8_t c2 = buf[*index];
        uint8_t c3 = buf[*index + 1];
        if ((c2 & 0xC0) != 0x80 || (c3 & 0xC0) != 0x80) return 0xFFFD;
        if (c == 0xE0 && c2 < 0xA0) return 0xFFFD;
        if (c == 0xED && c2 >= 0xA0) return 0xFFFD;
        (*index) += 2;
        return ((c & 0x0F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
    }

    return 0xFFFD;
}

uint16_t Seeed_GFX::decodeUTF8(uint8_t c) {
    if (!_utf8) return c;

    if (_utf8BytesRemaining == 0) {
        if (c < 0x80) return c;
        if (c >= 0xC2 && c <= 0xDF) {
            _utf8Codepoint = c & 0x1F;
            _utf8BytesRemaining = 1;
            return 0;
        }
        if (c >= 0xE0 && c <= 0xEF) {
            _utf8Codepoint = c & 0x0F;
            _utf8BytesRemaining = 2;
            return 0;
        }
        if (c >= 0xF0 && c <= 0xF4) {
            _utf8Codepoint = c & 0x07;
            _utf8BytesRemaining = 3;
            return 0;
        }
        return 0xFFFD;
    }

    if ((c & 0xC0) != 0x80) {
        _utf8BytesRemaining = 0;
        _utf8Codepoint = 0;
        return 0xFFFD;
    }
    _utf8Codepoint = (_utf8Codepoint << 6) | (c & 0x3F);
    if (--_utf8BytesRemaining != 0) return 0;

    const uint32_t codepoint = _utf8Codepoint;
    _utf8Codepoint = 0;
    if (codepoint > 0xFFFF || (codepoint >= 0xD800 && codepoint <= 0xDFFF))
        return 0xFFFD;
    return static_cast<uint16_t>(codepoint);
}

void Seeed_GFX::setFreeFont(const GFXfont *f) {
    if (f == nullptr) {
        setTextFont(1);
        return;
    }
    textfont = 1;
    gfxFont = (GFXfont *)f;

    glyph_ab = 0;
    glyph_bb = 0;
    uint16_t numChars = pgm_read_word(&gfxFont->last) -
                        pgm_read_word(&gfxFont->first) + 1U;

    // Find the biggest above and below baseline offsets
    for (uint16_t c = 0; c < numChars; c++) {
        GFXglyph *glyph1 = &(static_cast<GFXglyph*>(SEEED_PGM_PTR(&gfxFont->glyph))[c]);
        int8_t ab = -pgm_read_byte(&glyph1->yOffset);
        if (ab > glyph_ab) glyph_ab = ab;
        int8_t bb = pgm_read_byte(&glyph1->height) - ab;
        if (bb > glyph_bb) glyph_bb = bb;
    }
}
