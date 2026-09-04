// Legacy dither implementation matching the original Seeed_GFX local dither.cpp.
//
// This is the EXACT same algorithm as the old library's example-level dither.cpp,
// with Floyd-Steinberg error diffusion using [0,255] clamping on both read and
// write (int16_t buffer).  Wrapped in LegacyDither namespace so it coexists with
// the new Seeed_GFX2 library's SeeedDither module.
//
// Differences from SeeedDither:
//   - No serpentine scanning (always left-to-right)
//   - Error buffer clamped to [0,255] (not [-255,510])
//   - int16_t error buffer (not int32_t)
//   - Fewer dither methods (no BURKES, SIERRA3, PALETTE_MIX)
//   - No gamma LUT caching or saturation boost

#pragma once

#include <Arduino.h>

namespace LegacyDither {

enum DitherMethod {
  DITHER_NONE = 0,      // 直接最近色，无抖动
  DITHER_BAYER8,        // 有序 Bayer 8x8
  DITHER_FS,            // Floyd-Steinberg
  DITHER_JARVIS,        // Jarvis-Judice-Ninke
  DITHER_ATKINSON,      // Atkinson
};

enum DitherPalette {
  PAL_BW = 0,           // 1bpp: 0=black, 1=white
  PAL_GRAY4,            // 2bpp: 0..3 (0=black, 1=dark, 2=light, 3=white), luminance step +85
  PAL_GRAY16,           // 4bpp: 0..15 (0=black, 15=white), each step +17 in luminance
  PAL_E6,               // 4bpp E-Ink: 0x0=W, 0x2=G, 0x6=R, 0xB=Y, 0xD=B, 0xF=BK (raw codes)
};

// Returns true on success.  On PSRAM allocation failure for the error-diffusion
// buffer the function transparently falls back to undithered nearest-color
// quantization and still returns true.  out_index must be width * height bytes.
//
// BW mode honors invert (flip 0/1) and gamma.
// Gray4 / Gray16 / E6 modes ignore invert.
bool dither_image(const uint8_t* rgb888, int width, int height,
                  DitherPalette palette, DitherMethod method,
                  float gamma, bool invert,
                  uint8_t* out_index);

}  // namespace LegacyDither