#ifndef SEEED_GFX_DITHER_H
#define SEEED_GFX_DITHER_H

// Unified image dithering module for Seeed ePaper displays.
//
// Input:  24-bit RGB888 pixel buffer (row-major, no stride).
// Output: 1 byte per pixel — value is the palette index / code that the
//         ePaper driver expects (see Palettes.h for details).
//
// Algorithms:
//   DITHER_NONE      — Nearest-color quantization (no dithering)
//   DITHER_BAYER8    — 8×8 ordered Bayer dithering (zero extra RAM)
//   DITHER_FS        — Floyd-Steinberg error diffusion
//   DITHER_ATKINSON  — Atkinson error diffusion (75% error spread, crisp look)
//   DITHER_BURKES    — Burkes error diffusion (Stucki minus bottom row, fast+smooth)
//   DITHER_SIERRA3   — Full Sierra / Sierra-3 error diffusion
//   DITHER_PALETTE_MIX— Two-color palette-mix ordered dithering for irregular palettes
//
// Palettes:
//   PAL_BW     — 1bpp monochrome
//   PAL_GRAY4  — 4-level grayscale (2bpp)
//   PAL_GRAY16 — 16-level grayscale (4bpp)
//   PAL_E6     — 6-color Spectra (Black, White, Red, Yellow, Blue, Green)
//   PAL_BWRY   — 4-color (Black, White, Red, Yellow)
//
// Basic API:
//   dither_image(rgb888, w, h, palette, method, gamma, invert, out_index);
//
// Extended API with config struct:
//   DitherConfig cfg;
//   cfg.method = DITHER_FS;
//   cfg.palette = PAL_E6;
//   cfg.gamma = 1.1f;
//   cfg.serpentine = true;
//   dither_image_ex(rgb888, w, h, cfg, out_index);

#include <stdint.h>
#include <stddef.h>
#include "Palettes.h"

namespace SeeedDither {

// ----- Dither method enumeration -------------------------------------------

enum DitherMethod {
    DITHER_NONE = 0,     // Nearest-color, no dithering
    DITHER_BAYER8,       // 8×8 ordered Bayer
    DITHER_FS,           // Floyd-Steinberg
    DITHER_ATKINSON,     // Atkinson
    DITHER_BURKES,       // Burkes
    DITHER_SIERRA3,      // Full Sierra / Sierra-3
    DITHER_PALETTE_MIX,  // Two-color ordered palette mixing
    // Backward-compatible name.  The old implementation was not the published
    // Yliluoma algorithm; keep the numeric/API alias without making that claim.
    DITHER_YLILUOMA = DITHER_PALETTE_MIX,
};

// ----- Extended config ------------------------------------------------------

struct DitherConfig {
    DitherMethod  method          = DITHER_FS;
    DitherPalette palette         = PAL_E6;
    float         gamma           = 1.0f;       // x' = pow(x, 1/gamma): <1 darkens, >1 brightens
    bool          invert          = false;      // BW only: flip 0/1
    bool          serpentine      = false;       // snake-scan for error diffusion
    bool          legacyClamp     = true;      // true = narrow [0,255] clamp (legacy behavior); false = wide [-255,510] (default, better for photos)
    float         errorDiffusionStrength = 1.0f; // 0 = nearest-color only (no diffusion), 1 = full diffusion (default)
    float         saturationBoost = 0.0f;       // range [0,1]: 0 = none, 0.3 = moderate
    float         darknessBias    = 0.0f;       // range [0,0.5]: darken before dither; compensates for bright ePaper
    float         contrast        = 1.0f;       // 1.0 = no change; >1 increases contrast, <1 reduces
    float         warmth          = 0.0f;       // [-1, 1]: positive = warmer (more red), negative = cooler (more blue)
    // Distance metric for nearest-color quantization (color palettes only).
    // Default is METRIC_RGB: measured on the E6 color card, METRIC_REDMEAN
    // collapses out-of-gamut green+blue mixes (cyan -> 98% green) because its
    // 4x green weight fights the RGB error feedback of diffusion, and it even
    // speckles the pure blue primary with green.  METRIC_REDMEAN remains
    // selectable for experiments but is deprecated for E6/BWRY palettes.
    // Ignored by DITHER_PALETTE_MIX, which must stay METRIC_RGB to match its
    // RGB-linear mix geometry.  If mixes still look wrong on your panel,
    // calibrate customPaletteRgb instead.
    ColorMetric   colorMetric     = METRIC_RGB;
    uint32_t      randomSeed      = 0x12345678u; // DEPRECATED: legacy / reserved (no algorithm currently uses it); will be removed in v2.2

    // Optional calibrated palette for PAL_E6 / PAL_BWRY.  Supply all three
    // fields together.  Codes are the 4-bit values expected by the panel.
    const Rgb*    customPaletteRgb   = nullptr;
    const uint8_t* customPaletteCode = nullptr;
    uint8_t       customPaletteCount = 0;
};

// Reusable scratch storage for repeated dithering operations.  Keeping one
// context avoids rebuilding the gamma LUT and repeatedly allocating/freeing
// error-diffusion rows.  A context is not thread-safe and must not be copied.
class DitherContext {
public:
    DitherContext();
    ~DitherContext();

    DitherContext(const DitherContext&) = delete;
    DitherContext& operator=(const DitherContext&) = delete;

    // Release the reusable error buffer and invalidate the cached gamma LUT.
    void reset();

    // Bytes currently reserved for error diffusion.  The buffer grows on
    // demand and is retained until reset() or destruction.
    size_t workingMemoryCapacity() const;

private:
    friend struct DitherContextAccess;
    uint8_t gammaLut_[256];
    float gamma_;
    bool gammaValid_;
    int32_t* errorBuffer_;
    size_t errorCapacity_;
};

// ----- Public API -----------------------------------------------------------

// Basic API — compatible with the example-level dither_image() from v2.0.
// Returns true only when the requested method completed.  Allocation failure
// returns false; it never silently substitutes another algorithm.  This is
// important for benchmarks because a hidden fallback would invalidate results.
//
// `out_index` must point to a buffer of at least `width * height` bytes.
bool dither_image(const uint8_t* rgb888, int width, int height,
                  DitherPalette palette, DitherMethod method,
                  float gamma, bool invert,
                  uint8_t* out_index);

// Extended API — supports serpentine scanning and saturation boost.
// Returns true only when the requested method completed.
bool dither_image_ex(const uint8_t* rgb888, int width, int height,
                     const DitherConfig& config,
                     uint8_t* out_index);

// Context overload.  Output is byte-for-byte identical to dither_image_ex();
// only the LUT and error-row allocation are reused between calls.
bool dither_image_ex(const uint8_t* rgb888, int width, int height,
                     const DitherConfig& config, DitherContext& context,
                     uint8_t* out_index);

// Dither directly into packed 4bpp (two pixels per byte, left pixel in the
// high nibble).  `out_packed` needs ((width + 1) / 2) * height bytes.  For an
// odd width, the unused low nibble is filled with `padding_nibble`.
bool dither_image_4bpp(const uint8_t* rgb888, int width, int height,
                       const DitherConfig& config, uint8_t* out_packed,
                       uint8_t padding_nibble = 0);

// Direct-4bpp overload with reusable context.
bool dither_image_4bpp(const uint8_t* rgb888, int width, int height,
                       const DitherConfig& config, DitherContext& context,
                       uint8_t* out_packed, uint8_t padding_nibble = 0);

// Stable printable name for logs and example menus.  Returns "UNKNOWN" for
// values outside DitherMethod so sketches do not need enum-indexed arrays.
const char* dither_method_name(DitherMethod method);

// Error-diffusion working-buffer bytes for the requested combination.
// Returns 0 for algorithms without an error buffer or invalid dimensions.
// This excludes the RGB input, output index, decoder, and display framebuffer.
size_t dither_working_memory_bytes(int width, DitherPalette palette,
                                   DitherMethod method);

// ----- Packing helpers ------------------------------------------------------

// Pack a 1-byte-per-pixel BW index into 1bpp MSB-first format.
// `bit_for_black`: if true, black pixels get bit 1; otherwise bit 0.
void pack_1bpp_msb(const uint8_t* bw_index, uint8_t* out_bits,
                   int width, int height, bool bit_for_black);

// Pack a 1-byte-per-pixel 4bpp index into packed 4bpp format (2 pixels/byte,
// high nibble = left pixel).  For odd widths, use the palette's white code as
// `padding_nibble` (E6/BWRY=0, Gray4=3, Gray16=15).
void pack_4bpp(const uint8_t* index, uint8_t* out_packed,
               int width, int height, uint8_t padding_nibble = 0);

// In-place variant used by memory-constrained image examples.  Compression
// proceeds forwards safely because every packed destination byte is behind
// the unread 1-byte-per-pixel source data.
void pack_4bpp_in_place(uint8_t* index, int width, int height,
                        uint8_t padding_nibble);

}  // namespace SeeedDither

// ----- Backward compatibility -----------------------------------------------
// The old example-level code used a non-namespaced DitherMethod / DitherPalette
// and a free function `dither_image()`.  Re-export so existing sketches that
// switch from local dither.h to this library module compile without changes.

using SeeedDither::DitherMethod;
using SeeedDither::DitherPalette;
using SeeedDither::DitherConfig;
using SeeedDither::DitherContext;
using SeeedDither::ColorMetric;
using SeeedDither::Rgb;
using SeeedDither::dither_image;
using SeeedDither::dither_image_ex;
using SeeedDither::dither_image_4bpp;
using SeeedDither::dither_method_name;
using SeeedDither::dither_working_memory_bytes;
using SeeedDither::pack_1bpp_msb;
using SeeedDither::pack_4bpp;
using SeeedDither::pack_4bpp_in_place;

using SeeedDither::DITHER_NONE;
using SeeedDither::DITHER_BAYER8;
using SeeedDither::DITHER_FS;
using SeeedDither::DITHER_ATKINSON;
using SeeedDither::DITHER_BURKES;
using SeeedDither::DITHER_SIERRA3;
using SeeedDither::DITHER_PALETTE_MIX;
using SeeedDither::DITHER_YLILUOMA;

using SeeedDither::PAL_BW;
using SeeedDither::PAL_GRAY4;
using SeeedDither::PAL_GRAY16;
using SeeedDither::PAL_E6;
using SeeedDither::PAL_BWRY;

using SeeedDither::METRIC_RGB;
using SeeedDither::METRIC_REDMEAN;

#endif  // SEEED_GFX_DITHER_H
