#ifndef SEEED_GFX_DITHER_PALETTES_H
#define SEEED_GFX_DITHER_PALETTES_H

// Palette definitions for ePaper / e-ink dithering.
//
// Each palette specifies:
//   - The RGB888 reference color for each entry (used for nearest-color
//     matching and error computation).
//   - The 4-bit output code that the ePaper driver expects.
//   - The number of entries.
//
// IMPORTANT:  The RGB values here are the *theoretical* sRGB reference.
// For best results on real hardware, calibrate by measuring the actual
// pigment color with a colorimeter and override the palette at runtime via
// `DitherConfig::customPaletteRgb`, `customPaletteCode`, and
// `customPaletteCount`.
//
// Palettes:
//   PAL_BW     — 1 bpp  monochrome (black / white)
//   PAL_GRAY4  — 2 bpp  4-level grayscale
//   PAL_GRAY16 — 4 bpp  16-level grayscale
//   PAL_E6     — 4 bpp  6-color Spectra (W, G, R, Y, B, BK)
//   PAL_BWRY   — 4 bpp  4-color (Black, White, Red, Yellow)

#include <stdint.h>

namespace SeeedDither {

// ----- Shared RGB triplet ---------------------------------------------------

struct Rgb {
    uint8_t r, g, b;
};

// ----- Palette enumeration --------------------------------------------------

enum DitherPalette {
    PAL_BW = 0,      // 1bpp: 0=black, 1=white
    PAL_GRAY4,       // 2bpp: 0..3 (0=black, 3=white), step 85
    PAL_GRAY16,      // 4bpp: 0..15 (0=black, 15=white), step 17
    PAL_E6,          // 4bpp: 6-color Spectra (W, G, R, Y, B, BK)
    PAL_BWRY,        // 4bpp: 4-color (BK, W, R, Y)
};

// ----- BW ------------------------------------------------------------------

static constexpr int BW_COUNT = 2;
static const Rgb kBW_Rgb[BW_COUNT] = {
    {  0,   0,   0},   // 0: black
    {255, 255, 255},   // 1: white
};
static const uint8_t kBW_Code[BW_COUNT] = {0, 1};

// ----- Gray4 ---------------------------------------------------------------
// 4 levels: 0, 85, 170, 255

static constexpr int GRAY4_COUNT = 4;

inline Rgb makeGray(uint8_t v) { return {v, v, v}; }

static const uint8_t kGray4_Code[GRAY4_COUNT] = {0, 1, 2, 3};

inline const Rgb* gray4Rgb() {
    static Rgb table[GRAY4_COUNT];
    static bool built = false;
    if (!built) {
        for (int i = 0; i < GRAY4_COUNT; ++i)
            table[i] = makeGray(static_cast<uint8_t>(i * 85));
        built = true;
    }
    return table;
}

// ----- Gray16 --------------------------------------------------------------
// 16 levels: 0, 17, 34, ..., 255

static constexpr int GRAY16_COUNT = 16;

static const uint8_t kGray16_Code[GRAY16_COUNT] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
};

inline const Rgb* gray16Rgb() {
    static Rgb table[GRAY16_COUNT];
    static bool built = false;
    if (!built) {
        for (int i = 0; i < GRAY16_COUNT; ++i)
            table[i] = makeGray(static_cast<uint8_t>(i * 17));
        built = true;
    }
    return table;
}

// ----- E6 (Spectra 6-color) ------------------------------------------------
// White, Green, Red, Yellow, Blue, Black
// Codes match the EPaper firmware raw 4-bit values.

static constexpr int E6_COUNT = 6;

static const Rgb kE6_Rgb[E6_COUNT] = {
    {255, 255, 255},   // 0: WHITE  (code 0x0)
    { 29, 185,  84},   // 1: GREEN  (code 0x2)
    {229,  57,  53},   // 2: RED    (code 0x6)
    {255, 216,   0},   // 3: YELLOW (code 0xB)
    {  0,  76, 255},   // 4: BLUE   (code 0xD)
    {  0,   0,   0},   // 5: BLACK  (code 0xF)
};

static const uint8_t kE6_Code[E6_COUNT] = {0x0, 0x2, 0x6, 0xB, 0xD, 0xF};

// ----- BWRY (4-color: Black, White, Red, Yellow) ---------------------------
// Codes match Panel_EPaper BWRY_PALETTE_4BIT.

static constexpr int BWRY_COUNT = 4;

static const Rgb kBWRY_Rgb[BWRY_COUNT] = {
    {  0,   0,   0},   // 0: BLACK  (code 0xF)
    {255, 255, 255},   // 1: WHITE  (code 0x0)
    {229,  57,  53},   // 2: RED    (code 0x6)
    {255, 216,   0},   // 3: YELLOW (code 0xB)
};

static const uint8_t kBWRY_Code[BWRY_COUNT] = {0xF, 0x0, 0x6, 0xB};

// ----- Lookup helpers -------------------------------------------------------

// Return the number of palette entries for a given palette.
inline int paletteCount(DitherPalette p) {
    switch (p) {
        case PAL_BW:     return BW_COUNT;
        case PAL_GRAY4:  return GRAY4_COUNT;
        case PAL_GRAY16: return GRAY16_COUNT;
        case PAL_E6:     return E6_COUNT;
        case PAL_BWRY:   return BWRY_COUNT;
    }
    return 0;
}

// Return the RGB table for a given palette.
inline const Rgb* paletteRgb(DitherPalette p) {
    switch (p) {
        case PAL_BW:     return kBW_Rgb;
        case PAL_GRAY4:  return gray4Rgb();
        case PAL_GRAY16: return gray16Rgb();
        case PAL_E6:     return kE6_Rgb;
        case PAL_BWRY:   return kBWRY_Rgb;
    }
    return nullptr;
}

// Return the output-code table for a given palette.
inline const uint8_t* paletteCode(DitherPalette p) {
    switch (p) {
        case PAL_BW:     return kBW_Code;
        case PAL_GRAY4:  return kGray4_Code;
        case PAL_GRAY16: return kGray16_Code;
        case PAL_E6:     return kE6_Code;
        case PAL_BWRY:   return kBWRY_Code;
    }
    return nullptr;
}

// ----- Color-distance helpers -----------------------------------------------

// Which distance metric nearest-color quantization uses.
//
//   METRIC_RGB     — plain squared RGB Euclidean.  Matches the original
//                    Seeed_GFX reference implementation and the HTML tool.
//                    Required for any code path whose mix geometry is
//                    RGB-linear (see color_palette_mix).
//   METRIC_REDMEAN — perceptually-weighted "redmean" approximation.
//                    Reduces the E6 palette's tendency to snap mid-tones
//                    onto the saturated green, at the cost of slight hue
//                    shifts in out-of-gamut regions (e.g. teal mixes).
enum ColorMetric {
    METRIC_RGB = 0,
    METRIC_REDMEAN = 1,
};

// Squared Euclidean distance in RGB space.  This is intentionally unweighted;
// calibrated perceptual matching can be supplied in a future quantizer.
inline int rgbDist2(int r, int g, int b, const Rgb& p) {
    const int dr = r - p.r;
    const int dg = g - p.g;
    const int db = b - p.b;
    return dr * dr + dg * dg + db * db;
}

// Perceptually-weighted "redmean" color distance (Compuphase formula).
//
// This is the widely-used, cheap perceptual metric described at
//   https://www.compuphase.com/cmetric.htm
// It weights the green channel most heavily (the eye is most sensitive to green)
// and adapts the red/blue weights to the local brightness via the mean red value.
// Compared with plain RGB Euclidean it greatly reduces the E6 palette's tendency to
// snap mid-tones onto the saturated green, while staying a handful of integer ops
// (no color-space conversion).  This is the industry-standard drop-in alternative
// to rgbDist2(); it is not a full CIE76/CIEDE2000.  Selected via ColorMetric.
inline int redmeanDist2(int r, int g, int b, const Rgb& p) {
    const int dr = r - p.r;
    const int dg = g - p.g;
    const int db = b - p.b;
    const int rmean = (r + p.r) / 2;
    // red/blue effective weights range over [2.0, 3.0); green is fixed at 4.
    return (((512 + rmean) * dr * dr) >> 8)
         + 4 * dg * dg
         + (((767 - rmean) * db * db) >> 8);
}

// Dispatch to the configured distance metric.
inline int paletteDist2(int r, int g, int b, const Rgb& p, ColorMetric metric) {
    return (metric == METRIC_REDMEAN) ? redmeanDist2(r, g, b, p)
                                      : rgbDist2(r, g, b, p);
}

// Find the nearest palette entry index for an RGB pixel.
inline int nearestPaletteIndex(int r, int g, int b, DitherPalette pal,
                               ColorMetric metric = METRIC_RGB) {
    const Rgb* rgb = paletteRgb(pal);
    const int count = paletteCount(pal);
    int best = 0;
    int bestD = paletteDist2(r, g, b, rgb[0], metric);
    for (int i = 1; i < count; ++i) {
        const int d = paletteDist2(r, g, b, rgb[i], metric);
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}

// BT.709 luminance using fixed-point Q15 weights.
inline int luma(int r, int g, int b) {
    const int w = 6966 * r + 23436 * g + 2366 * b;
    return w >> 15;
}

}  // namespace SeeedDither

#endif  // SEEED_GFX_DITHER_PALETTES_H
