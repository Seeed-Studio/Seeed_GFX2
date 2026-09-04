// Legacy dither implementation — exact copy of the original Seeed_GFX dither.cpp
// (Floyd-Steinberg with [0,255] int16_t clamping, no serpentine).
//
// Wrapped in LegacyDither namespace to avoid conflicts with Seeed_GFX2's
// SeeedDither module.

#include "legacy_dither.h"

#include <cmath>
#include <cstring>

namespace LegacyDither {

// ===========================================================================
// Internal helpers  (same as old dither.cpp anonymous namespace)
// ===========================================================================

namespace {

struct Rgb { uint8_t r, g, b; };

// Gray16: 0..15, step 17.
static Rgb make_gray(uint8_t v) { return {v, v, v}; }

// E6 mapping (white, green, red, yellow, blue, black) and the raw e-paper
// codes used by the HTML tool and the ePaper firmware.
static const Rgb kE6Rgb[6] = {
    {255, 255, 255},  // 0: WHITE  (code 0x0)
    { 29, 185,  84},  // 1: GREEN  (code 0x2)
    {229,  57,  53},  // 2: RED    (code 0x6)
    {255, 216,   0},  // 3: YELLOW (code 0xB)
    {  0,  76, 255},  // 4: BLUE   (code 0xD)
    {  0,   0,   0},  // 5: BLACK  (code 0xF)
};
static const uint8_t kE6Code[6] = {0x0, 0x2, 0x6, 0xB, 0xD, 0xF};

// ----- Bayer 8x8 -----------------------------------------------------------

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

// ----- Error diffusion kernels ---------------------------------------------

struct KernelTap { int dx, dy, num, den; };

static const KernelTap kFS[] = {
    { 1, 0, 7, 16},
    {-1, 1, 3, 16},
    { 0, 1, 5, 16},
    { 1, 1, 1, 16},
};
static const KernelTap kJarvis[] = {
    { 1, 0, 7, 48}, { 2, 0, 5, 48},
    {-2, 1, 3, 48}, {-1, 1, 5, 48}, { 0, 1, 7, 48}, { 1, 1, 5, 48}, { 2, 1, 3, 48},
    {-2, 2, 1, 48}, {-1, 2, 3, 48}, { 0, 2, 5, 48}, { 1, 2, 3, 48}, { 2, 2, 1, 48},
};
static const KernelTap kAtkinson[] = {
    { 1, 0, 1, 8}, { 2, 0, 1, 8},
    {-1, 1, 1, 8}, { 0, 1, 1, 8}, { 1, 1, 1, 8},
    { 0, 2, 1, 8},
};

static const KernelTap* pick_kernel(DitherMethod m, size_t& nout) {
  switch (m) {
    case DITHER_JARVIS:   nout = sizeof(kJarvis)   / sizeof(KernelTap); return kJarvis;
    case DITHER_ATKINSON: nout = sizeof(kAtkinson) / sizeof(KernelTap); return kAtkinson;
    default:              nout = sizeof(kFS)       / sizeof(KernelTap); return kFS;
  }
}

// ----- Math helpers ---------------------------------------------------------

static inline int clamp_u8(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }

static inline int luma(int r, int g, int b) {
  // 0.2126 R + 0.7152 G + 0.0722 B, fixed-point with Q15 weights.
  // 0.2126 ≈ 6966 / 32768, 0.7152 ≈ 23436 / 32768, 0.0722 ≈ 2366 / 32768
  const int w = 6966 * r + 23436 * g + 2366 * b;
  return w >> 15;
}

static inline int apply_gamma(int gray, float g) {
  if (g <= 0.0f || (g > 0.999f && g < 1.001f)) return gray;
  const float x = gray / 255.0f;
  const float y = powf(x, 1.0f / g);
  return clamp_u8(static_cast<int>(y * 255.0f + 0.5f));
}

// Squared distance (no weighting).
static inline int rgb_dist2(int r, int g, int b, const Rgb& p) {
  const int dr = r - p.r, dg = g - p.g, db = b - p.b;
  return dr * dr + dg * dg + db * db;
}

static inline int nearest_e6(int r, int g, int b) {
  int best = 0;
  int bestD = rgb_dist2(r, g, b, kE6Rgb[0]);
  for (int i = 1; i < 6; ++i) {
    const int d = rgb_dist2(r, g, b, kE6Rgb[i]);
    if (d < bestD) { bestD = d; best = i; }
  }
  return best;
}

// Gray16 nearest: simply round-divide by 17.
static inline int nearest_gray16(int gray) {
  int q = (gray + 8) / 17;
  if (q < 0) q = 0;
  if (q > 15) q = 15;
  return q;
}

// Gray4 nearest: round to the nearest of [0, 85, 170, 255].
static inline int nearest_gray4(int gray) {
  int q = (gray + 42) / 85;
  if (q < 0) q = 0;
  if (q > 3) q = 3;
  return q;
}

// ----- BW -------------------------------------------------------------------

static void bw_none(const uint8_t* rgb, int W, int H, float gamma, uint8_t* out) {
  for (int i = 0, p = 0; p < W * H; ++p, i += 3) {
    int g = apply_gamma(luma(rgb[i], rgb[i + 1], rgb[i + 2]), gamma);
    out[p] = (g < 128) ? 0 : 1;
  }
}

static void bw_bayer(const uint8_t* rgb, int W, int H, float gamma, uint8_t* out) {
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      const int i = (y * W + x) * 3;
      const int g = apply_gamma(luma(rgb[i], rgb[i + 1], rgb[i + 2]), gamma);
      const int t = (kBayer8[(y & 7) * 8 + (x & 7)] * 4) + 2;
      out[y * W + x] = (g < t) ? 0 : 1;
    }
  }
}

// Error-diffusion BW with int16 working buffer (PSRAM). Falls back to nearest
// if alloc fails.
static bool bw_diffuse(const uint8_t* rgb, int W, int H, float gamma,
                        DitherMethod method, uint8_t* out) {
  const size_t n = static_cast<size_t>(W) * static_cast<size_t>(H);
  int16_t* buf = static_cast<int16_t*>(ps_malloc(n * sizeof(int16_t)));
  if (!buf) buf = static_cast<int16_t*>(malloc(n * sizeof(int16_t)));
  if (!buf) {
    Serial1.printf("[legacy-dither] WARN: error-diffusion buffer %lu kB alloc "
                   "FAILED -> falling back to DITHER_NONE.\n",
                   (unsigned long)(n * sizeof(int16_t) / 1024));
    bw_none(rgb, W, H, gamma, out);
    return false;
  }
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      const int i = (y * W + x) * 3;
      buf[y * W + x] = static_cast<int16_t>(
          apply_gamma(luma(rgb[i], rgb[i + 1], rgb[i + 2]), gamma));
    }
  }
  size_t kn = 0;
  const KernelTap* K = pick_kernel(method, kn);
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      const int idx = y * W + x;
      const int old = buf[idx];
      const int nv = (old < 128) ? 0 : 255;
      out[idx] = (nv == 0) ? 0 : 1;
      const int err = old - nv;
      for (size_t k = 0; k < kn; ++k) {
        const int nx = x + K[k].dx;
        const int ny = y + K[k].dy;
        if (nx < 0 || nx >= W || ny < 0 || ny >= H) continue;
        const int ni = ny * W + nx;
        const int v = buf[ni] + err * K[k].num / K[k].den;
        buf[ni] = static_cast<int16_t>(clamp_u8(v));
      }
    }
  }
  free(buf);
  return true;
}

// ----- Gray16 ---------------------------------------------------------------

static void gray16_none(const uint8_t* rgb, int W, int H, float gamma, uint8_t* out) {
  for (int i = 0, p = 0; p < W * H; ++p, i += 3) {
    const int g = apply_gamma(luma(rgb[i], rgb[i + 1], rgb[i + 2]), gamma);
    out[p] = static_cast<uint8_t>(nearest_gray16(g));
  }
}

static void gray16_bayer(const uint8_t* rgb, int W, int H, float gamma, uint8_t* out) {
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      const int i = (y * W + x) * 3;
      int g = apply_gamma(luma(rgb[i], rgb[i + 1], rgb[i + 2]), gamma);
      const int mod = kBayer8[(y & 7) * 8 + (x & 7)] - 32;
      g = clamp_u8(g + mod);
      out[y * W + x] = static_cast<uint8_t>(nearest_gray16(g));
    }
  }
}

static bool gray16_diffuse(const uint8_t* rgb, int W, int H, float gamma,
                            DitherMethod method, uint8_t* out) {
  const size_t n = static_cast<size_t>(W) * static_cast<size_t>(H);
  int16_t* buf = static_cast<int16_t*>(ps_malloc(n * sizeof(int16_t)));
  if (!buf) buf = static_cast<int16_t*>(malloc(n * sizeof(int16_t)));
  if (!buf) {
    Serial1.printf("[legacy-dither] WARN: error-diffusion buffer %lu kB alloc "
                   "FAILED -> falling back to DITHER_NONE.\n",
                   (unsigned long)(n * sizeof(int16_t) / 1024));
    gray16_none(rgb, W, H, gamma, out);
    return false;
  }
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      const int i = (y * W + x) * 3;
      buf[y * W + x] = static_cast<int16_t>(
          apply_gamma(luma(rgb[i], rgb[i + 1], rgb[i + 2]), gamma));
    }
  }
  size_t kn = 0;
  const KernelTap* K = pick_kernel(method, kn);
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      const int idx = y * W + x;
      const int old = clamp_u8(buf[idx]);
      const int q = nearest_gray16(old);
      out[idx] = static_cast<uint8_t>(q);
      const int qv = q * 17;
      const int err = old - qv;
      for (size_t k = 0; k < kn; ++k) {
        const int nx = x + K[k].dx;
        const int ny = y + K[k].dy;
        if (nx < 0 || nx >= W || ny < 0 || ny >= H) continue;
        const int ni = ny * W + nx;
        const int v = buf[ni] + err * K[k].num / K[k].den;
        buf[ni] = static_cast<int16_t>(clamp_u8(v));
      }
    }
  }
  free(buf);
  return true;
}

// ----- Gray4 ----------------------------------------------------------------

static void gray4_none(const uint8_t* rgb, int W, int H, float gamma, uint8_t* out) {
  for (int i = 0, p = 0; p < W * H; ++p, i += 3) {
    const int g = apply_gamma(luma(rgb[i], rgb[i + 1], rgb[i + 2]), gamma);
    out[p] = static_cast<uint8_t>(nearest_gray4(g));
  }
}

static void gray4_bayer(const uint8_t* rgb, int W, int H, float gamma, uint8_t* out) {
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      const int i = (y * W + x) * 3;
      int g = apply_gamma(luma(rgb[i], rgb[i + 1], rgb[i + 2]), gamma);
      const int mod = kBayer8[(y & 7) * 8 + (x & 7)] - 32;
      g = clamp_u8(g + mod);
      out[y * W + x] = static_cast<uint8_t>(nearest_gray4(g));
    }
  }
}

static bool gray4_diffuse(const uint8_t* rgb, int W, int H, float gamma,
                           DitherMethod method, uint8_t* out) {
  const size_t n = static_cast<size_t>(W) * static_cast<size_t>(H);
  int16_t* buf = static_cast<int16_t*>(ps_malloc(n * sizeof(int16_t)));
  if (!buf) buf = static_cast<int16_t*>(malloc(n * sizeof(int16_t)));
  if (!buf) {
    Serial1.printf("[legacy-dither] WARN: error-diffusion buffer %lu kB alloc "
                   "FAILED -> falling back to DITHER_NONE.\n",
                   (unsigned long)(n * sizeof(int16_t) / 1024));
    gray4_none(rgb, W, H, gamma, out);
    return false;
  }
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      const int i = (y * W + x) * 3;
      buf[y * W + x] = static_cast<int16_t>(
          apply_gamma(luma(rgb[i], rgb[i + 1], rgb[i + 2]), gamma));
    }
  }
  size_t kn = 0;
  const KernelTap* K = pick_kernel(method, kn);
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      const int idx = y * W + x;
      const int old = clamp_u8(buf[idx]);
      const int q = nearest_gray4(old);
      out[idx] = static_cast<uint8_t>(q);
      const int qv = q * 85;
      const int err = old - qv;
      for (size_t k = 0; k < kn; ++k) {
        const int nx = x + K[k].dx;
        const int ny = y + K[k].dy;
        if (nx < 0 || nx >= W || ny < 0 || ny >= H) continue;
        const int ni = ny * W + nx;
        const int v = buf[ni] + err * K[k].num / K[k].den;
        buf[ni] = static_cast<int16_t>(clamp_u8(v));
      }
    }
  }
  free(buf);
  return true;
}

// ----- E6 -------------------------------------------------------------------

static void e6_none(const uint8_t* rgb, int W, int H, uint8_t* out) {
  for (int i = 0, p = 0; p < W * H; ++p, i += 3) {
    const int q = nearest_e6(rgb[i], rgb[i + 1], rgb[i + 2]);
    out[p] = kE6Code[q];
  }
}

static void e6_bayer(const uint8_t* rgb, int W, int H, uint8_t* out) {
  const int spread = 64;
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      const int i = (y * W + x) * 3;
      const int mod = ((kBayer8[(y & 7) * 8 + (x & 7)] * spread) >> 6) - (spread >> 1);
      const int r = clamp_u8(rgb[i + 0] + mod);
      const int g = clamp_u8(rgb[i + 1] + mod);
      const int b = clamp_u8(rgb[i + 2] + mod);
      out[y * W + x] = kE6Code[nearest_e6(r, g, b)];
    }
  }
}

static bool e6_diffuse(const uint8_t* rgb, int W, int H,
                        DitherMethod method, uint8_t* out) {
  const size_t n = static_cast<size_t>(W) * static_cast<size_t>(H);
  // 3 * int16 per pixel for RGB error diffusion.
  int16_t* buf = static_cast<int16_t*>(ps_malloc(n * 3 * sizeof(int16_t)));
  if (!buf) buf = static_cast<int16_t*>(malloc(n * 3 * sizeof(int16_t)));
  if (!buf) {
    Serial1.printf("[legacy-dither] WARN: E6 error-diffusion buffer %lu kB alloc "
                   "FAILED -> falling back to DITHER_NONE.\n",
                   (unsigned long)(n * 3 * sizeof(int16_t) / 1024));
    e6_none(rgb, W, H, out);
    return false;
  }
  for (size_t p = 0, i = 0; p < n; ++p, i += 3) {
    buf[i + 0] = rgb[i + 0];
    buf[i + 1] = rgb[i + 1];
    buf[i + 2] = rgb[i + 2];
  }
  size_t kn = 0;
  const KernelTap* K = pick_kernel(method, kn);
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      const int idx = y * W + x;
      const int o = idx * 3;
      const int r = clamp_u8(buf[o + 0]);
      const int g = clamp_u8(buf[o + 1]);
      const int b = clamp_u8(buf[o + 2]);
      const int q = nearest_e6(r, g, b);
      out[idx] = kE6Code[q];
      const int er = r - kE6Rgb[q].r;
      const int eg = g - kE6Rgb[q].g;
      const int eb = b - kE6Rgb[q].b;
      for (size_t k = 0; k < kn; ++k) {
        const int nx = x + K[k].dx;
        const int ny = y + K[k].dy;
        if (nx < 0 || nx >= W || ny < 0 || ny >= H) continue;
        const int no = (ny * W + nx) * 3;
        buf[no + 0] = static_cast<int16_t>(clamp_u8(buf[no + 0] + er * K[k].num / K[k].den));
        buf[no + 1] = static_cast<int16_t>(clamp_u8(buf[no + 1] + eg * K[k].num / K[k].den));
        buf[no + 2] = static_cast<int16_t>(clamp_u8(buf[no + 2] + eb * K[k].num / K[k].den));
      }
    }
  }
  free(buf);
  return true;
}

}  // anonymous namespace

// ===========================================================================
// Public API
// ===========================================================================

bool dither_image(const uint8_t* rgb888, int width, int height,
                  DitherPalette palette, DitherMethod method,
                  float gamma, bool invert,
                  uint8_t* out_index) {
  if (!rgb888 || !out_index || width <= 0 || height <= 0) return false;

  switch (palette) {
    case PAL_BW: {
      if (method == DITHER_NONE)        bw_none (rgb888, width, height, gamma, out_index);
      else if (method == DITHER_BAYER8) bw_bayer(rgb888, width, height, gamma, out_index);
      else                              bw_diffuse(rgb888, width, height, gamma, method, out_index);
      if (invert) {
        for (size_t p = 0, n = static_cast<size_t>(width) * height; p < n; ++p)
          out_index[p] ^= 1;
      }
      return true;
    }
    case PAL_GRAY4: {
      if (method == DITHER_NONE)        gray4_none (rgb888, width, height, gamma, out_index);
      else if (method == DITHER_BAYER8) gray4_bayer(rgb888, width, height, gamma, out_index);
      else                              gray4_diffuse(rgb888, width, height, gamma, method, out_index);
      return true;
    }
    case PAL_GRAY16: {
      if (method == DITHER_NONE)        gray16_none (rgb888, width, height, gamma, out_index);
      else if (method == DITHER_BAYER8) gray16_bayer(rgb888, width, height, gamma, out_index);
      else                              gray16_diffuse(rgb888, width, height, gamma, method, out_index);
      return true;
    }
    case PAL_E6: {
      // Gamma is supported for E6 by pre-adjusting each channel.
      if (gamma < 0.999f || gamma > 1.001f) {
        const size_t n = static_cast<size_t>(width) * height;
        uint8_t* tmp = static_cast<uint8_t*>(ps_malloc(n * 3));
        if (!tmp) tmp = static_cast<uint8_t*>(malloc(n * 3));
        if (tmp) {
          for (size_t i = 0; i < n * 3; ++i) tmp[i] = apply_gamma(rgb888[i], gamma);
          rgb888 = tmp;
          if (method == DITHER_NONE)        e6_none (rgb888, width, height, out_index);
          else if (method == DITHER_BAYER8) e6_bayer(rgb888, width, height, out_index);
          else                              e6_diffuse(rgb888, width, height, method, out_index);
          free(tmp);
          return true;
        }
        // alloc failed -> fall through with original buffer
      }
      if (method == DITHER_NONE)        e6_none (rgb888, width, height, out_index);
      else if (method == DITHER_BAYER8) e6_bayer(rgb888, width, height, out_index);
      else                              e6_diffuse(rgb888, width, height, method, out_index);
      return true;
    }
  }
  return false;
}

}  // namespace LegacyDither