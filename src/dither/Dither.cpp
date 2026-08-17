// Unified dithering implementation for Seeed ePaper displays.
//
// Design notes:
//   - Luminance:       Y = 0.2126*R + 0.7152*G + 0.0722*B  (BT.709, Q15 fixed-point)
//   - Gamma:           x' = pow(x/255, 1/g) * 255
//   - BW threshold:    <128 → black, ≥128 → white  (Bayer adds ordered perturbation)
//   - Color quant:     Euclidean distance² in RGB space
//   - Serpentine scan: odd rows go right-to-left, mirror dx of kernel taps
//   - Error buffer:    2 or 3 scan lines, selected from the kernel reach

#include "Dither.h"
#include "Kernels.h"

#include <Arduino.h>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace SeeedDither {

// ===========================================================================
// Internal helpers
// ===========================================================================

namespace {

static inline int clampU8(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }

// Wide clamp for color-diffusion buffer reads.  Deliberately wider than [0,255]:
// off-gamut targets (e.g. magenta, r=b=255) drive accumulated channels past
// the RGB cube, and the quantizer MUST see that excess — otherwise the
// deficit is clipped away before the nearest-color decision and error
// diffusion can never correct it.  The bounds keep redmeanDist2 weights
// positive and all arithmetic bounded; the row-wise buffer refill limits
// real excursions to a few hundred.
static inline int clampBuf(int v) { return v < -255 ? -255 : (v > 510 ? 510 : v); }

// Divide a signed error by a power of two without right-shifting a negative
// value.  C++ leaves negative signed shifts implementation-defined and common
// compilers round them toward negative infinity, which makes tiny negative
// errors propagate while equally small positive errors disappear.  Mirroring
// the sign gives the truncation-toward-zero behavior of integer division while
// preserving the cheap power-of-two implementation.
static inline int scaleError(int error, int numerator, int shift) {
    const int value = error * numerator;
    return value >= 0 ? (value >> shift) : -((-value) >> shift);
}

static bool validImageDimensions(int width, int height, size_t& pixels) {
    if (width <= 0 || height <= 0) return false;
    const size_t w = static_cast<size_t>(width);
    const size_t h = static_cast<size_t>(height);
    if (w > std::numeric_limits<size_t>::max() / h) return false;
    pixels = w * h;
    // Several hot loops intentionally use int coordinates/indices.  Reject
    // dimensions that cannot be represented instead of invoking signed UB.
    if (pixels > static_cast<size_t>(INT_MAX)) return false;
    return pixels <= std::numeric_limits<size_t>::max() / 3u;
}

static inline int applyGammaRaw(int gray, float g) {
    // !(g > 0.0f) catches NaN, negative, and zero in one comparison.
    if (!(g > 0.0f) || (g > 0.999f && g < 1.001f)) return gray;
    const float x = gray / 255.0f;
    const float y = powf(x, 1.0f / g);
    return clampU8(static_cast<int>(y * 255.0f + 0.5f));
}

static inline void buildGammaLut(float gamma, uint8_t lut[256]) {
    for (int i = 0; i < 256; ++i) {
        lut[i] = static_cast<uint8_t>(applyGammaRaw(i, gamma));
    }
}

static inline int applyGamma(int gray, const uint8_t lut[256]) {
    return lut[gray];
}

// Bayer 8×8 threshold matrix.
static const uint8_t kBayer8[64] = {
     0, 48, 12, 60,  3, 51, 15, 63,
    32, 16, 44, 28, 35, 19, 47, 31,
     8, 56,  4, 52, 11, 59,  7, 55,
    40, 24, 36, 20, 43, 27, 39, 23,
     2, 50, 14, 62,  1, 49, 13, 61,
    34, 18, 46, 30, 33, 17, 45, 29,
    10, 58,  6, 54,  9, 57,  5, 53,
    42, 26, 38, 22, 41, 25, 37, 21,
};

// Nearest-color quantization for grayscale palettes.
static inline int nearestGray4(int gray) {
    int q = (gray + 42) / 85;
    if (q < 0) q = 0;
    if (q > 3) q = 3;
    return q;
}

static inline int nearestGray16(int gray) {
    int q = (gray + 8) / 17;
    if (q < 0) q = 0;
    if (q > 15) q = 15;
    return q;
}

// Pick the kernel for a given method.  Returns nullptr for NONE / BAYER8.
static const KernelTap* pickKernel(DitherMethod m, size_t& count) {
    switch (m) {
        case DITHER_FS:       return kFloydSteinberg(count);
        case DITHER_ATKINSON: return kAtkinson(count);
        case DITHER_BURKES:   return kBurkes(count);
        case DITHER_SIERRA3:  return kSierra3(count);
        default:              count = 0; return nullptr;
    }
}

// Allocate working memory, preferring PSRAM only on ESP32.  Other Arduino
// cores (for example nRF52840) do not provide ps_malloc().
static int32_t* allocBuffer(size_t bytes) {
    int32_t* buf = nullptr;
#if defined(ARDUINO_ARCH_ESP32) || defined(ESP32)
    buf = static_cast<int32_t*>(ps_malloc(bytes));
#endif
    if (!buf) buf = static_cast<int32_t*>(malloc(bytes));
    return buf;
}

static int kernelRowCount(const KernelTap* kernel, size_t count) {
    int maxDy = 0;
    for (size_t i = 0; i < count; ++i)
        if (kernel[i].dy > maxDy) maxDy = kernel[i].dy;
    return maxDy + 1;
}

// A single algorithm implementation can target either the traditional
// byte-per-pixel index buffer or packed 4bpp output.  The packed writer keeps
// the unused nibble of an odd-width row at the caller-selected padding value.
class OutputWriter {
public:
    OutputWriter(uint8_t* data, int width, int height, bool packed,
                 uint8_t paddingNibble, uint8_t xorMask)
        : data_(data), width_(width), packed_(packed), xorMask_(xorMask & 0x0F),
          stride_((static_cast<size_t>(width) + 1u) / 2u) {
        if (packed_) {
            const uint8_t pad = paddingNibble & 0x0F;
            memset(data_, static_cast<int>((pad << 4) | pad),
                   stride_ * static_cast<size_t>(height));
        }
    }

    inline void set(int x, int y, uint8_t value) {
        value = static_cast<uint8_t>((value ^ xorMask_) & 0x0F);
        if (!packed_) {
            data_[static_cast<size_t>(y) * width_ + x] = value;
            return;
        }
        uint8_t& byte = data_[static_cast<size_t>(y) * stride_ + (x >> 1)];
        if ((x & 1) == 0)
            byte = static_cast<uint8_t>((byte & 0x0F) | (value << 4));
        else
            byte = static_cast<uint8_t>((byte & 0xF0) | value);
    }

    inline void setLinear(size_t index, uint8_t value) {
        const int y = static_cast<int>(index / static_cast<size_t>(width_));
        const int x = static_cast<int>(index - static_cast<size_t>(y) * width_);
        set(x, y, value);
    }

private:
    uint8_t* data_;
    int width_;
    bool packed_;
    uint8_t xorMask_;
    size_t stride_;
};

// Saturation boost: increase chroma while preserving luminance.
// boost ∈ [0, 1].  0 = no change.
static inline void saturate(int& r, int& g, int& b, float boost) {
    if (boost <= 0.0f) return;
    const int gray = luma(r, g, b);
    r = clampU8(static_cast<int>(gray + (r - gray) * (1.0f + boost)));
    g = clampU8(static_cast<int>(gray + (g - gray) * (1.0f + boost)));
    b = clampU8(static_cast<int>(gray + (b - gray) * (1.0f + boost)));
}

// Darkness bias: darken the image uniformly before dithering.
// bias ∈ [0, 0.5].  0 = no change.  Compensates for bright ePaper panels.
static inline void darken(int& r, int& g, int& b, float bias) {
    if (bias <= 0.0f) return;
    const float factor = 1.0f - bias;
    r = clampU8(static_cast<int>(r * factor));
    g = clampU8(static_cast<int>(g * factor));
    b = clampU8(static_cast<int>(b * factor));
}

// Contrast: 1.0 = no change.  >1 stretches midtones, <1 flattens.
static inline void contrast(int& r, int& g, int& b, float c) {
    if (c <= 0.0f || (c > 0.999f && c < 1.001f)) return;
    const int gray = luma(r, g, b);
    r = clampU8(static_cast<int>(gray + (r - gray) * c));
    g = clampU8(static_cast<int>(gray + (g - gray) * c));
    b = clampU8(static_cast<int>(gray + (b - gray) * c));
}

// Warmth: color temperature shift.  [-1, 1], 0 = neutral.
// Positive → warmer (more red, less blue).  Negative → cooler (more blue, less red).
static inline void warmth(int& r, int& g, int& b, float w) {
    if (w <= -0.001f || w >= 0.001f) {
        r = clampU8(static_cast<int>(r + w * 50.0f));
        b = clampU8(static_cast<int>(b - w * 50.0f));
    }
}

static inline void preprocessRgb(const uint8_t* src, size_t offset,
                                 const uint8_t gammaLut[256], float boost,
                                 float dBias, float cont, float warm,
                                 int& r, int& g, int& b) {
    r = applyGamma(src[offset + 0], gammaLut);
    g = applyGamma(src[offset + 1], gammaLut);
    b = applyGamma(src[offset + 2], gammaLut);
    darken(r, g, b, dBias);
    saturate(r, g, b, boost);
    contrast(r, g, b, cont);
    warmth(r, g, b, warm);
}

struct PaletteView {
    const Rgb* rgb;
    const uint8_t* code;
    int count;
};

static bool makePaletteView(const DitherConfig& cfg, PaletteView& out) {
    if (cfg.customPaletteRgb || cfg.customPaletteCode || cfg.customPaletteCount) {
        if (!cfg.customPaletteRgb || !cfg.customPaletteCode ||
            cfg.customPaletteCount == 0 || cfg.customPaletteCount > 16)
            return false;
        out.rgb = cfg.customPaletteRgb;
        out.code = cfg.customPaletteCode;
        out.count = cfg.customPaletteCount;
        for (int i = 0; i < out.count; ++i)
            if (out.code[i] > 0x0F) return false;
        return true;
    }
    out.rgb = paletteRgb(cfg.palette);
    out.code = paletteCode(cfg.palette);
    out.count = paletteCount(cfg.palette);
    return out.rgb && out.code && out.count > 0;
}

static int nearestPaletteIndex(int r, int g, int b, const PaletteView& pal,
                               ColorMetric metric) {
    int best = 0;
    int bestD = paletteDist2(r, g, b, pal.rgb[0], metric);
    for (int i = 1; i < pal.count; ++i) {
        const int d = paletteDist2(r, g, b, pal.rgb[i], metric);
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}

}  // anonymous namespace

// Friend bridge keeps the reusable storage private without exposing allocator
// details in the public header.
struct DitherContextAccess {
    static bool prepare(DitherContext& context, float gamma, size_t bytes) {
        if (!context.gammaValid_ || context.gamma_ != gamma) {
            buildGammaLut(gamma, context.gammaLut_);
            context.gamma_ = gamma;
            context.gammaValid_ = true;
        }
        if (bytes <= context.errorCapacity_) return true;
        int32_t* replacement = allocBuffer(bytes);
        if (!replacement) return false;
        free(context.errorBuffer_);
        context.errorBuffer_ = replacement;
        context.errorCapacity_ = bytes;
        return true;
    }

    static const uint8_t* gammaLut(const DitherContext& context) {
        return context.gammaLut_;
    }

    static int32_t* errorBuffer(DitherContext& context) {
        return context.errorBuffer_;
    }
};

DitherContext::DitherContext()
    : gamma_ (1.0f), gammaValid_(false), errorBuffer_(nullptr),
      errorCapacity_(0) {
    memset(gammaLut_, 0, sizeof(gammaLut_));
}

DitherContext::~DitherContext() {
    free(errorBuffer_);
}

void DitherContext::reset() {
    free(errorBuffer_);
    errorBuffer_ = nullptr;
    errorCapacity_ = 0;
    gammaValid_ = false;
}

size_t DitherContext::workingMemoryCapacity() const {
    return errorCapacity_;
}

// ===========================================================================
// BW dithering
// ===========================================================================

namespace {

static void bw_none(const uint8_t* rgb, int W, int H, const uint8_t gammaLut[256], OutputWriter& out) {
    for (int p = 0, i = 0; p < W * H; ++p, i += 3) {
        int g = applyGamma(luma(rgb[i], rgb[i + 1], rgb[i + 2]), gammaLut);
        out.setLinear(static_cast<size_t>(p), (g < 128) ? 0 : 1);
    }
}

static void bw_bayer(const uint8_t* rgb, int W, int H, const uint8_t gammaLut[256], OutputWriter& out) {
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            const int i = (y * W + x) * 3;
            const int g = applyGamma(luma(rgb[i], rgb[i + 1], rgb[i + 2]), gammaLut);
            const int t = (kBayer8[(y & 7) * 8 + (x & 7)] * 4) + 2;
            out.set(x, y, (g < t) ? 0 : 1);
        }
    }
}

static void fillLumaRow(int32_t* dst, const uint8_t* rgb, int W, int y,
                        const uint8_t gammaLut[256]) {
    const size_t row = static_cast<size_t>(y) * W;
    for (int x = 0; x < W; ++x) {
        const size_t i = (row + x) * 3;
        dst[x] = static_cast<int32_t>(
            applyGamma(luma(rgb[i], rgb[i + 1], rgb[i + 2]), gammaLut));
    }
}

static bool bw_diffuse(const uint8_t* rgb, int W, int H, const uint8_t gammaLut[256],
                        DitherMethod method, bool serp, int32_t* buf,
                        OutputWriter& out) {
    size_t kn = 0;
    const KernelTap* K = pickKernel(method, kn);
    if (!K || kn == 0) return false;
    const int rows = kernelRowCount(K, kn);
    if (!buf) return false;
    for (int y = 0; y < rows && y < H; ++y)
        fillLumaRow(buf + static_cast<size_t>(y) * W, rgb, W, y, gammaLut);

    for (int y = 0; y < H; ++y) {
        const bool rtl = serp && (y & 1);
        for (int xi = 0; xi < W; ++xi) {
            const int x = rtl ? (W - 1 - xi) : xi;
            const size_t wi = static_cast<size_t>(y % rows) * W + x;
            const int old = clampU8(buf[wi]);
            const int nv = (old < 128) ? 0 : 255;
            out.set(x, y, (nv == 0) ? 0 : 1);
            const int err = old - nv;
            for (size_t k = 0; k < kn; ++k) {
                const int dx = (rtl ? -K[k].dx : K[k].dx);
                const int nx = x + dx;
                const int ny = y + K[k].dy;
                if (nx < 0 || nx >= W || ny < 0 || ny >= H) continue;
                const size_t ni = static_cast<size_t>(ny % rows) * W + nx;
                buf[ni] = static_cast<int32_t>(buf[ni] + scaleError(err, K[k].num, K[k].shift));
            }
        }
        const int next = y + rows;
        if (next < H)
            fillLumaRow(buf + static_cast<size_t>(y % rows) * W,
                        rgb, W, next, gammaLut);
    }
    return true;
}

}  // anonymous namespace

// ===========================================================================
// Grayscale dithering (Gray4 / Gray16)
// ===========================================================================

namespace {

// Generic grayscale error-diffusion.  `levels` = 4 or 16.
static bool gray_diffuse(const uint8_t* rgb, int W, int H, const uint8_t gammaLut[256],
                          DitherMethod method, bool serp, int levels,
                          int32_t* buf, OutputWriter& out) {
    const int step = (levels == 4) ? 85 : 17;
    size_t kn = 0;
    const KernelTap* K = pickKernel(method, kn);
    if (!K || kn == 0) return false;
    const int rows = kernelRowCount(K, kn);
    if (!buf) return false;
    for (int y = 0; y < rows && y < H; ++y)
        fillLumaRow(buf + static_cast<size_t>(y) * W, rgb, W, y, gammaLut);

    for (int y = 0; y < H; ++y) {
        const bool rtl = serp && (y & 1);
        for (int xi = 0; xi < W; ++xi) {
            const int x = rtl ? (W - 1 - xi) : xi;
            const size_t wi = static_cast<size_t>(y % rows) * W + x;
            const int old = clampU8(buf[wi]);
            const int q = (levels == 4) ? nearestGray4(old) : nearestGray16(old);
            out.set(x, y, static_cast<uint8_t>(q));
            const int err = old - q * step;
            for (size_t k = 0; k < kn; ++k) {
                const int dx = (rtl ? -K[k].dx : K[k].dx);
                const int nx = x + dx;
                const int ny = y + K[k].dy;
                if (nx < 0 || nx >= W || ny < 0 || ny >= H) continue;
                const size_t ni = static_cast<size_t>(ny % rows) * W + nx;
                buf[ni] = static_cast<int32_t>(buf[ni] + scaleError(err, K[k].num, K[k].shift));
            }
        }
        const int next = y + rows;
        if (next < H)
            fillLumaRow(buf + static_cast<size_t>(y % rows) * W,
                        rgb, W, next, gammaLut);
    }
    return true;
}

static void gray_bayer(int levels, const uint8_t* rgb, int W, int H,
                        const uint8_t gammaLut[256], OutputWriter& out) {
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            const int i = (y * W + x) * 3;
            const int g = applyGamma(luma(rgb[i], rgb[i + 1], rgb[i + 2]), gammaLut);
            // Ordered multi-level dithering must choose only between the two
            // levels surrounding the input.  A fixed +/-32 perturbation made
            // Gray16 jump across as many as four levels because its step is 17.
            const int scaled = g * (levels - 1);
            const int low = scaled / 255;
            const int remainder = scaled % 255;
            const int threshold = kBayer8[(y & 7) * 8 + (x & 7)];
            const bool chooseHigh = low + 1 < levels &&
                                    threshold * 255 < remainder * 64;
            out.set(x, y, static_cast<uint8_t>(low + (chooseHigh ? 1 : 0)));
        }
    }
}

}  // anonymous namespace

// ===========================================================================
// Color palette dithering (E6 / BWRY)
// ===========================================================================

namespace {

// Ordered (Bayer) dithering for arbitrary color palettes.
static void color_bayer(const PaletteView& pal, const uint8_t* rgb, int W, int H,
                        const uint8_t gammaLut[256], float satBoost, float dBias,
                        float cont, float warm, ColorMetric metric, OutputWriter& out) {
    const int spread = 64;
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            const size_t i = (static_cast<size_t>(y) * W + x) * 3;
            int r, g, b;
            preprocessRgb(rgb, i, gammaLut, satBoost, dBias, cont, warm, r, g, b);
            const int mod = ((kBayer8[(y & 7) * 8 + (x & 7)] * spread) >> 6)
                            - (spread >> 1);
            r = clampU8(r + mod);
            g = clampU8(g + mod);
            b = clampU8(b + mod);
            const int q = nearestPaletteIndex(r, g, b, pal, metric);
            out.set(x, y, pal.code[q]);
        }
    }
}

// Nearest-color (no dithering) for arbitrary color palettes.
static void color_none(const PaletteView& pal, const uint8_t* rgb, int W, int H,
                       const uint8_t gammaLut[256], float satBoost, float dBias,
                       float cont, float warm, ColorMetric metric, OutputWriter& out) {
    const size_t n = static_cast<size_t>(W) * H;
    for (size_t p = 0; p < n; ++p) {
        int r, g, b;
        preprocessRgb(rgb, p * 3, gammaLut, satBoost, dBias, cont, warm, r, g, b);
        const int q = nearestPaletteIndex(r, g, b, pal, metric);
        out.setLinear(p, pal.code[q]);
    }
}

static void fillColorRow(int32_t* dst, const uint8_t* rgb, int W, int y,
                         const uint8_t gammaLut[256], float satBoost, float dBias,
                         float cont, float warm) {
    const size_t row = static_cast<size_t>(y) * W;
    for (int x = 0; x < W; ++x) {
        int r, g, b;
        preprocessRgb(rgb, (row + x) * 3, gammaLut, satBoost, dBias, cont, warm, r, g, b);
        dst[x * 3 + 0] = static_cast<int32_t>(r);
        dst[x * 3 + 1] = static_cast<int32_t>(g);
        dst[x * 3 + 2] = static_cast<int32_t>(b);
    }
}

// Error-diffusion dithering for arbitrary color palettes (E6 / BWRY).
// Uses only the 2 or 3 scan lines reached by the selected kernel.
static bool color_diffuse(const PaletteView& pal, const uint8_t* rgb, int W, int H,
                          DitherMethod method, bool serp, const uint8_t gammaLut[256],
                          float satBoost, float dBias, float cont, float warm,
                          ColorMetric metric, bool legacyClamp, float diffStrength,
                          int32_t* buf, OutputWriter& out) {
    size_t kn = 0;
    const KernelTap* K = pickKernel(method, kn);
    if (!K || kn == 0) return false;
    const int rows = kernelRowCount(K, kn);
    if (!buf) return false;
    for (int y = 0; y < rows && y < H; ++y)
        fillColorRow(buf + static_cast<size_t>(y) * W * 3,
                     rgb, W, y, gammaLut, satBoost, dBias, cont, warm);

    for (int y = 0; y < H; ++y) {
        const bool rtl = serp && (y & 1);
        for (int xi = 0; xi < W; ++xi) {
            const int x = rtl ? (W - 1 - xi) : xi;
            const size_t o = (static_cast<size_t>(y % rows) * W + x) * 3;
            // Select clamp strategy: wide [-255,510] preserves off-gamut color
            // mixing (magenta, cyan); narrow [0,255] damps oscillation at
            // pure-color boundaries (better for color cards / UI elements).
            const int r = legacyClamp ? clampU8(buf[o + 0]) : clampBuf(buf[o + 0]);
            const int g = legacyClamp ? clampU8(buf[o + 1]) : clampBuf(buf[o + 1]);
            const int b = legacyClamp ? clampU8(buf[o + 2]) : clampBuf(buf[o + 2]);

            // Find nearest palette entry.
            const int best = nearestPaletteIndex(r, g, b, pal, metric);
            out.set(x, y, pal.code[best]);

            const int er = static_cast<int>((r - pal.rgb[best].r) * diffStrength);
            const int eg = static_cast<int>((g - pal.rgb[best].g) * diffStrength);
            const int eb = static_cast<int>((b - pal.rgb[best].b) * diffStrength);

            for (size_t k = 0; k < kn; ++k) {
                const int dx = (rtl ? -K[k].dx : K[k].dx);
                const int nx = x + dx;
                const int ny = y + K[k].dy;
                if (nx < 0 || nx >= W || ny < 0 || ny >= H) continue;
                const size_t no = (static_cast<size_t>(ny % rows) * W + nx) * 3;
                const int num = K[k].num;
                const int shift = K[k].shift;
                const int v0 = buf[no + 0] + scaleError(er, num, shift);
                const int v1 = buf[no + 1] + scaleError(eg, num, shift);
                const int v2 = buf[no + 2] + scaleError(eb, num, shift);
                buf[no + 0] = static_cast<int32_t>(legacyClamp ? clampU8(v0) : v0);
                buf[no + 1] = static_cast<int32_t>(legacyClamp ? clampU8(v1) : v1);
                buf[no + 2] = static_cast<int32_t>(legacyClamp ? clampU8(v2) : v2);
            }
        }
        const int next = y + rows;
        if (next < H)
            fillColorRow(buf + static_cast<size_t>(y % rows) * W * 3,
                         rgb, W, next, gammaLut, satBoost, dBias, cont, warm);
    }
    return true;
}

static void color_palette_mix(const PaletteView& pal, const uint8_t* rgb, int W, int H,
                              const uint8_t gammaLut[256], float satBoost, float dBias,
                              float cont, float warm, OutputWriter& out) {
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            const size_t i = (static_cast<size_t>(y) * W + x) * 3;
            int r, g, b;
            preprocessRgb(rgb, i, gammaLut, satBoost, dBias, cont, warm, r, g, b);

            // PALETTE_MIX blends two palette colors by LINEAR RGB interpolation
            // (see projection/mix/mr below).  The distance used to judge a mix
            // MUST therefore be plain RGB Euclidean so it is consistent with the
            // geometry.  Using a perceptual metric here made the mix-ratio (which
            // is optimized for RGB) disagree with the evaluation metric, causing
            // wrong color pairs to be chosen (e.g. blue bleeding into green).
            // So this function deliberately stays RGB-based, unlike the
            // nearest-color-only paths which benefit from redmeanDist2().
            int bestA = 0;
            int bestD = rgbDist2(r, g, b, pal.rgb[0]);
            for (int pi = 1; pi < pal.count; ++pi) {
                const int d0 = rgbDist2(r, g, b, pal.rgb[pi]);
                if (d0 < bestD) { bestD = d0; bestA = pi; }
            }
            int bestB = bestA;
            int bestMix = 0;

            for (int a = 0; a < pal.count; ++a) {
                for (int bb = a + 1; bb < pal.count; ++bb) {
                    const int dr = static_cast<int>(pal.rgb[bb].r) - pal.rgb[a].r;
                    const int dg = static_cast<int>(pal.rgb[bb].g) - pal.rgb[a].g;
                    const int db = static_cast<int>(pal.rgb[bb].b) - pal.rgb[a].b;
                    const int den = dr * dr + dg * dg + db * db;
                    if (den == 0) continue;
                    int projection = (r - pal.rgb[a].r) * dr +
                                     (g - pal.rgb[a].g) * dg +
                                     (b - pal.rgb[a].b) * db;
                    if (projection < 0) projection = 0;
                    if (projection > den) projection = den;
                    const int mix = (projection * 64 + den / 2) / den;
                    const int mr = (pal.rgb[a].r * (64 - mix) + pal.rgb[bb].r * mix + 32) >> 6;
                    const int mg = (pal.rgb[a].g * (64 - mix) + pal.rgb[bb].g * mix + 32) >> 6;
                    const int mb = (pal.rgb[a].b * (64 - mix) + pal.rgb[bb].b * mix + 32) >> 6;
                    const int er = r - mr, eg = g - mg, eb = b - mb;
                    const int d = er * er + eg * eg + eb * eb;
                    if (d < bestD) {
                        bestD = d;
                        bestA = a;
                        bestB = bb;
                        bestMix = mix;
                    }
                }
            }
            const int threshold = kBayer8[(y & 7) * 8 + (x & 7)];
            out.set(x, y, pal.code[(threshold < bestMix) ? bestB : bestA]);
        }
    }
}

}  // anonymous namespace

// ===========================================================================
// Public API: dither_image()
// ===========================================================================

bool dither_image(const uint8_t* rgb888, int width, int height,
                  DitherPalette palette, DitherMethod method,
                  float gamma, bool invert,
                  uint8_t* out_index) {
    // Forward to the extended API with default config.
    DitherConfig cfg;
    cfg.method    = method;
    cfg.palette   = palette;
    cfg.gamma     = gamma;
    cfg.invert    = invert;
    cfg.serpentine = true;
    cfg.saturationBoost = 0.0f;
    return dither_image_ex(rgb888, width, height, cfg, out_index);
}

size_t dither_working_memory_bytes(int width, DitherPalette palette,
                                   DitherMethod method) {
    if (width <= 0) return 0;
    if (palette < PAL_BW || palette > PAL_BWRY) return 0;
    size_t count = 0;
    const KernelTap* kernel = pickKernel(method, count);
    if (!kernel || count == 0) return 0;
    const size_t rows = static_cast<size_t>(kernelRowCount(kernel, count));
    const size_t channels = (palette == PAL_E6 || palette == PAL_BWRY) ? 3u : 1u;
    const size_t w = static_cast<size_t>(width);
    if (rows > std::numeric_limits<size_t>::max() / w) return 0;
    const size_t rowElements = rows * w;
    if (channels > std::numeric_limits<size_t>::max() / rowElements) return 0;
    const size_t elements = rowElements * channels;
    if (elements > std::numeric_limits<size_t>::max() / sizeof(int32_t)) return 0;
    return elements * sizeof(int32_t);
}

const char* dither_method_name(DitherMethod method) {
    switch (method) {
        case DITHER_NONE:        return "NONE";
        case DITHER_BAYER8:      return "BAYER8";
        case DITHER_FS:          return "FS";
        case DITHER_ATKINSON:    return "ATKINSON";
        case DITHER_BURKES:      return "BURKES";
        case DITHER_SIERRA3:     return "SIERRA3";
        case DITHER_PALETTE_MIX: return "PALETTE_MIX";
    }
    return "UNKNOWN";
}

namespace {

static bool ditherImageImpl(const uint8_t* rgb888, int width, int height,
                            const DitherConfig& cfg, DitherContext& context,
                            uint8_t* output, bool packed4bpp,
                            uint8_t paddingNibble) {
    if (!rgb888 || !output) return false;
    size_t pixelCount = 0;
    if (!validImageDimensions(width, height, pixelCount)) return false;
    if (pixelCount == 0) return false;

    const DitherPalette pal = cfg.palette;
    const DitherMethod  method = cfg.method;
    const float         gamma  = cfg.gamma;
    const bool          serp   = cfg.serpentine;
    const float         satBoost = cfg.saturationBoost;
    const float         dBias    = cfg.darknessBias;
    const float         cont     = cfg.contrast;
    const float         warm     = cfg.warmth;
    const float         diffStr  = cfg.errorDiffusionStrength;
    if (method < DITHER_NONE || method > DITHER_PALETTE_MIX) return false;
    if (!std::isfinite(gamma) || !(gamma > 0.0f)) return false;
    if (!std::isfinite(satBoost) || satBoost < 0.0f || satBoost > 1.0f)
        return false;
    if (!std::isfinite(dBias) || dBias < 0.0f || dBias > 0.5f)
        return false;
    if (!std::isfinite(cont) || cont <= 0.0f || cont > 3.0f)
        return false;
    if (!std::isfinite(warm) || warm < -1.0f || warm > 1.0f)
        return false;
    if (!std::isfinite(diffStr) || diffStr < 0.0f || diffStr > 1.0f)
        return false;
    if (cfg.colorMetric != METRIC_RGB && cfg.colorMetric != METRIC_REDMEAN)
        return false;

    const bool hasCustomPalette = cfg.customPaletteRgb ||
                                  cfg.customPaletteCode ||
                                  cfg.customPaletteCount;
    if (hasCustomPalette && pal != PAL_E6 && pal != PAL_BWRY) return false;

    const size_t workingBytes = dither_working_memory_bytes(width, pal, method);
    if (!DitherContextAccess::prepare(context, gamma, workingBytes)) return false;
    const uint8_t* gammaLut = DitherContextAccess::gammaLut(context);
    int32_t* errorBuffer = DitherContextAccess::errorBuffer(context);
    const uint8_t xorMask = (pal == PAL_BW && cfg.invert) ? 1u : 0u;
    OutputWriter writer(output, width, height, packed4bpp,
                        paddingNibble, xorMask);

    // ----- BW ---------------------------------------------------------------
    if (pal == PAL_BW) {
        bool ok = true;
        if (method == DITHER_NONE)            bw_none(rgb888, width, height, gammaLut, writer);
        else if (method == DITHER_BAYER8)     bw_bayer(rgb888, width, height, gammaLut, writer);
        else if (method == DITHER_PALETTE_MIX) return false;
        else ok = bw_diffuse(rgb888, width, height, gammaLut, method, serp,
                             errorBuffer, writer);
        return ok;
    }

    // ----- Gray4 / Gray16 --------------------------------------------------
    if (pal == PAL_GRAY4 || pal == PAL_GRAY16) {
        const int levels = (pal == PAL_GRAY4) ? 4 : 16;
        if (method == DITHER_NONE) {
            for (int p = 0, i = 0; p < width * height; ++p, i += 3) {
                int g = applyGamma(luma(rgb888[i], rgb888[i + 1], rgb888[i + 2]), gammaLut);
                writer.setLinear(static_cast<size_t>(p), (levels == 4)
                    ? static_cast<uint8_t>(nearestGray4(g))
                    : static_cast<uint8_t>(nearestGray16(g)));
            }
        } else if (method == DITHER_BAYER8) {
            gray_bayer(levels, rgb888, width, height, gammaLut, writer);
        } else if (method == DITHER_PALETTE_MIX) {
            return false;
        } else {
            if (!gray_diffuse(rgb888, width, height, gammaLut, method, serp,
                              levels, errorBuffer, writer)) return false;
        }
        return true;
    }

    // ----- E6 / BWRY (color palettes) --------------------------------------
    if (pal == PAL_E6 || pal == PAL_BWRY) {
        PaletteView view;
        if (!makePaletteView(cfg, view)) return false;
        const ColorMetric metric = cfg.colorMetric;
        bool ok = true;
        if (method == DITHER_NONE)
            color_none(view, rgb888, width, height, gammaLut, satBoost, dBias, cont, warm, metric, writer);
        else if (method == DITHER_BAYER8)
            color_bayer(view, rgb888, width, height, gammaLut, satBoost, dBias, cont, warm, metric, writer);
        else if (method == DITHER_PALETTE_MIX)
            color_palette_mix(view, rgb888, width, height, gammaLut, satBoost, dBias, cont, warm, writer);
        else
            ok = color_diffuse(view, rgb888, width, height, method, serp,
                               gammaLut, satBoost, dBias, cont, warm, metric,
                               cfg.legacyClamp, diffStr, errorBuffer, writer);
        return ok;
    }

    return false;  // unknown palette
}

}  // anonymous namespace

// ===========================================================================
// Public API: index and direct-4bpp output
// ===========================================================================

bool dither_image_ex(const uint8_t* rgb888, int width, int height,
                     const DitherConfig& cfg, uint8_t* out_index) {
    DitherContext context;
    return ditherImageImpl(rgb888, width, height, cfg, context, out_index,
                           false, 0);
}

bool dither_image_ex(const uint8_t* rgb888, int width, int height,
                     const DitherConfig& cfg, DitherContext& context,
                     uint8_t* out_index) {
    return ditherImageImpl(rgb888, width, height, cfg, context, out_index,
                           false, 0);
}

bool dither_image_4bpp(const uint8_t* rgb888, int width, int height,
                       const DitherConfig& cfg, uint8_t* out_packed,
                       uint8_t padding_nibble) {
    DitherContext context;
    return ditherImageImpl(rgb888, width, height, cfg, context, out_packed,
                           true, padding_nibble);
}

bool dither_image_4bpp(const uint8_t* rgb888, int width, int height,
                       const DitherConfig& cfg, DitherContext& context,
                       uint8_t* out_packed, uint8_t padding_nibble) {
    return ditherImageImpl(rgb888, width, height, cfg, context, out_packed,
                           true, padding_nibble);
}

// ===========================================================================
// Packing helpers
// ===========================================================================

void pack_1bpp_msb(const uint8_t* bw_index, uint8_t* out_bits,
                   int width, int height, bool bit_for_black) {
    if (!bw_index || !out_bits || width <= 0 || height <= 0) return;
    const size_t rowBytes = (static_cast<size_t>(width) + 7u) / 8u;
    for (int y = 0; y < height; ++y) {
        const uint8_t* row = bw_index + static_cast<size_t>(y) * width;
        uint8_t* dst = out_bits + static_cast<size_t>(y) * rowBytes;
        for (int x = 0; x < width; x += 8) {
            uint8_t byte = 0;
            for (int b = 0; b < 8; ++b) {
                const int xi = x + b;
                const bool isBlack = (xi < width) ? (row[xi] == 0) : false;
                const int bit = bit_for_black ? (isBlack ? 1 : 0) : (isBlack ? 0 : 1);
                byte |= static_cast<uint8_t>((bit & 1) << (7 - b));
            }
            dst[x / 8] = byte;
        }
    }
}

void pack_4bpp(const uint8_t* index, uint8_t* out_packed,
               int width, int height, uint8_t padding_nibble) {
    if (!index || !out_packed || width <= 0 || height <= 0) return;
    padding_nibble &= 0x0F;
    const size_t packedStride = (static_cast<size_t>(width) + 1) / 2;
    for (int y = 0; y < height; ++y) {
        const uint8_t* src = index + static_cast<size_t>(y) * width;
        uint8_t* dst = out_packed + static_cast<size_t>(y) * packedStride;
        for (int x = 0; x < width; x += 2) {
            const uint8_t a = src[x] & 0xF;
            const uint8_t b = (x + 1 < width) ? (src[x + 1] & 0xF)
                                               : padding_nibble;
            dst[x >> 1] = static_cast<uint8_t>((a << 4) | b);
        }
    }
}

void pack_4bpp_in_place(uint8_t* index, int width, int height,
                        uint8_t padding_nibble) {
    if (!index || width <= 0 || height <= 0) return;
    padding_nibble &= 0x0F;
    const size_t packedStride = (static_cast<size_t>(width) + 1) / 2;
    for (int y = 0; y < height; ++y) {
        const uint8_t* src = index + static_cast<size_t>(y) * width;
        uint8_t* dst = index + static_cast<size_t>(y) * packedStride;
        for (int x = 0; x < width; x += 2) {
            const uint8_t a = src[x] & 0x0F;
            const uint8_t b = (x + 1 < width) ? (src[x + 1] & 0x0F)
                                               : padding_nibble;
            dst[x >> 1] = static_cast<uint8_t>((a << 4) | b);
        }
    }
}

}  // namespace SeeedDither
