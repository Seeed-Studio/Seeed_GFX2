#ifndef SEEED_GFX_DITHER_KERNELS_H
#define SEEED_GFX_DITHER_KERNELS_H

// Error-diffusion kernel definitions for dithering algorithms.
//
// Each kernel is an array of KernelTap entries describing how quantization
// error is distributed to neighboring pixels.  The `shift` field is the
// base-2 logarithm of the denominator (i.e. denominator = 1 << shift).
// All included kernels use power-of-two denominators, so the inner loop can
// replace division by a right shift.
//
// Kernels included:
//   1. Floyd-Steinberg   — 4 taps, denominator 16.  Industry default.
//   2. Atkinson           — 6 taps, denominator  8.  Only 75% of error spread.
//   3. Burkes             — 7 taps, denominator 32.  Stucki minus bottom row.
//   4. Full Sierra / Sierra-3  — 10 taps, denominator 32.  Smooth middle ground.
//
// All kernels assume left-to-right scanning.  When serpentine scanning is
// enabled the caller must mirror the `dx` component on odd rows.

#include <stddef.h>
#include <stdint.h>

namespace SeeedDither {

struct KernelTap {
    int8_t dx;
    int8_t dy;
    int8_t num;    // numerator
    int8_t shift;  // denominator = 1 << shift (all kernels use power-of-two denominators)
};

// ---------------------------------------------------------------------------
// 1. Floyd-Steinberg  (4 taps / 16)
//
//              X   7/16
//   3/16  5/16  1/16
// ---------------------------------------------------------------------------
inline const KernelTap* kFloydSteinberg(size_t& count) {
    static const KernelTap k[] = {
        { 1, 0, 7, 4},
        {-1, 1, 3, 4},
        { 0, 1, 5, 4},
        { 1, 1, 1, 4},
    };
    count = sizeof(k) / sizeof(k[0]);
    return k;
}

// ---------------------------------------------------------------------------
// 2. Atkinson  (6 taps / 8)
//
//              X   1/8  1/8
//        1/8  1/8  1/8
//              1/8
//
// Spreads only 6/8 = 75% of the error.  Result is lighter / higher-contrast.
// ---------------------------------------------------------------------------
inline const KernelTap* kAtkinson(size_t& count) {
    static const KernelTap k[] = {
        { 1, 0, 1, 3}, { 2, 0, 1, 3},
        {-1, 1, 1, 3}, { 0, 1, 1, 3}, { 1, 1, 1, 3},
        { 0, 2, 1, 3},
    };
    count = sizeof(k) / sizeof(k[0]);
    return k;
}

// ---------------------------------------------------------------------------
// 3. Burkes  (7 taps / 32)
//
//              X   8/32  4/32
//   2/32  4/32  8/32  4/32  2/32
//
// Stucki with the bottom row removed.  Faster while still producing
// smooth gradients.  Default algorithm in epaper-dithering (OpenDisplay).
// ---------------------------------------------------------------------------
inline const KernelTap* kBurkes(size_t& count) {
    static const KernelTap k[] = {
        { 1, 0, 8, 5}, { 2, 0, 4, 5},
        {-2, 1, 2, 5}, {-1, 1, 4, 5}, { 0, 1, 8, 5}, { 1, 1, 4, 5}, { 2, 1, 2, 5},
    };
    count = sizeof(k) / sizeof(k[0]);
    return k;
}

// ---------------------------------------------------------------------------
// 4. Full Sierra / Sierra-3  (10 taps / 32)
//
//              X   5/32  3/32
//   2/32  4/32  5/32  4/32  2/32
//        2/32  3/32  2/32
//
// Jarvis-Judice-Ninke with fewer non-zero coefficients — smooth without the
// heavy 12-tap footprint.
// ---------------------------------------------------------------------------
inline const KernelTap* kSierra3(size_t& count) {
    static const KernelTap k[] = {
        { 1, 0, 5, 5}, { 2, 0, 3, 5},
        {-2, 1, 2, 5}, {-1, 1, 4, 5}, { 0, 1, 5, 5}, { 1, 1, 4, 5}, { 2, 1, 2, 5},
        {-1, 2, 2, 5}, { 0, 2, 3, 5}, { 1, 2, 2, 5},
    };
    count = sizeof(k) / sizeof(k[0]);
    return k;
}

}  // namespace SeeedDither

#endif  // SEEED_GFX_DITHER_KERNELS_H
