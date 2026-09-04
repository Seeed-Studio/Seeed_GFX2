/*
 * reTerminal E1002 -- SD card image to 6-color e-paper: DITHER COMPARISON.
 *
 * Shows the SAME resized image side-by-side, separated by a BLACK vertical
 * line. Each side dithers the complete 399x480 source image independently:
 *   LEFT  pane (0..398)   = NEW Seeed_GFX2 library dither (dither_image_ex)
 *   RIGHT pane (401..799) = Legacy Seeed_GFX dither (LegacyDither::dither_image)
 *
 * The only difference between the two is the error-buffer clamping:
 *   NEW  library: clampBuf [-255, 510], int32_t, no serpentine (serpentine=false)
 *   LEGACY copy:  clamp_u8  [0, 255],  int16_t, always left-to-right
 *
 * All other parameters (FS method, gamma=1.0, palette=E6) are identical.
 * This compares the algorithms fairly: it does not dither an 800px-wide image
 * and crop a different half for each algorithm.
 *
 * Hardware: reTerminal E1002 (XIAO ESP32-S3 + 7.5" ED2208 6-color e-paper).
 *
 *           Function     ESP32-S3 GPIO
 *           SPI SCK      7   (shared with SD)
 *           SPI MOSI     9   (shared with SD)
 *           EPD CS       10
 *           EPD DC       11
 *           EPD RST      12
 *           EPD BUSY     13
 *           SD  CS       14
 *           SD  EN       16
 *           SD  DET      15
 *           SD  MISO     8   (routed by the library E1002 board config)
 *
 * Logging goes out on UART1 (GPIO43 TX, GPIO44 RX).
 */

#include <SPI.h>
#include <FS.h>
#include <SD.h>
#include <Seeed_GFX.h>
#include <dither/Dither.h>
#include "bus/Bus_SPI.h"

#include "image_loader.h"
#include "legacy_dither.h"

Seeed_GFX display(Seeed_Product::reTerminal_E1002);
static constexpr int EPD_WIDTH  = 800;
static constexpr int EPD_HEIGHT = 480;
static constexpr int DIVIDER_WIDTH = 2;
static constexpr int PANE_WIDTH = (EPD_WIDTH - DIVIDER_WIDTH) / 2;

// ----- pins ------------------------------------------------------------------
static constexpr int    PIN_SD_SCK   = 7;
static constexpr int    PIN_SD_MISO  = 8;
static constexpr int    PIN_SD_MOSI  = 9;
static constexpr int    PIN_SD_CS    = 14;
static constexpr int    PIN_SD_EN    = 16;
static constexpr int    PIN_SD_DET   = 15;
static constexpr int    PIN_DBG_RX   = 44;
static constexpr int    PIN_DBG_TX   = 43;
#define LOG       Serial1
#define TAG       "[dither-cmp]"

// ----- user configuration ----------------------------------------------------

// Path to the image on the SD card (must start with '/').
static const char* IMAGE_PATH = "/img/demo.jpg";

// =========================================================================
//  NEW library parameters (Seeed_GFX2) — TUNABLE
//  Adjust these to find the best settings for different image styles.
// =========================================================================

// Dither algorithm: FS | ATKINSON | BURKES | SIERRA3 | BAYER8 | NONE | PALETTE_MIX
static SeeedDither::DitherMethod NEW_METHOD = SeeedDither::DITHER_FS;

// Gamma: <1.0 darkens, >1.0 brightens.  1.0 = linear.
static float NEW_GAMMA = 1.0f;

// Serpentine (zigzag) scanning: reduces directional streaks.
static bool NEW_SERPENTINE = false;

// Clamp strategy: false = wide [-255,510] (default, better for photos/magenta);
//                  true = narrow [0,255] (legacy behavior, cleaner pure-color boundaries).
static bool NEW_LEGACY_CLAMP = false;

// Saturation boost [0, 1]: compensates for reduced palette.  0 = none.
static float NEW_SAT_BOOST = 0.0f;

// Darkness bias [0, 0.5]: darken before dithering to compensate for bright ePaper.
static float NEW_DARKNESS_BIAS = 0.0f;

// Contrast: 1.0 = no change.  >1 increases, <1 reduces.
static float NEW_CONTRAST = 1.0f;

// Error diffusion strength [0, 1]: 0 = nearest only, 1 = full diffusion.
static float NEW_DIFF_STRENGTH = 1.0f;

// Warmth [-1, 1]: positive = warmer (more red), negative = cooler (more blue).
static float NEW_WARMTH = 0.0f;

// Color distance metric: METRIC_RGB (default) | METRIC_REDMEAN (deprecated).
static SeeedDither::ColorMetric NEW_COLOR_METRIC = SeeedDither::METRIC_RGB;

// ----- Custom palette calibration -------------------------------------------
// Toggle with 'k'.  Edit individual entries with k0..k5 R G B.
// When enabled, the dithering algorithm targets these RGB values instead of the
// default E6 reference.  This lets you calibrate to your panel's actual pigments.
//   Index:  0=White(0x0)  1=Green(0x2)  2=Red(0x6)  3=Yellow(0xB)  4=Blue(0xD)  5=Black(0xF)
static bool USE_CUSTOM_PALETTE = false;
// Default sRGB reference values for E6 Spectra pigments.
// These are THEORETICAL — calibrate with your phone and
// replace with measured values to match YOUR panel's actual colors.
static SeeedDither::Rgb CUSTOM_PALETTE[6] = {
    {255, 255, 255},  // 0: White
    { 29, 185,  84},  // 1: Green
    {229,  57,  53},  // 2: Red
    {255, 216,   0},  // 3: Yellow
    {  0,  76, 255},  // 4: Blue
    {  0,   0,   0},  // 5: Black
};
static const uint8_t CUSTOM_CODE[6] = {0x0, 0x2, 0x6, 0xB, 0xD, 0xF};
static const char* PAL_NAMES[6] = {"White","Green","Red","Yellow","Blue","Black"};

// =========================================================================
//  LEGACY library parameters (original Seeed_GFX) — FIXED baseline
//  These match the old library's exact behavior for fair comparison.
// =========================================================================

// Dither algorithm: FS | JARVIS | ATKINSON (same as old library).
static const LegacyDither::DitherMethod LEGACY_METHOD = LegacyDither::DITHER_FS;

// Gamma: matches old library default.
static const float LEGACY_GAMMA = 1.0f;

// Invert: BW only, ignored for E6.
static const bool LEGACY_INVERT = false;

// ----- global image storage (kept for interactive mode switching) ------------
static RgbImage g_originalImage = {nullptr, 0, 0};
static bool     g_hasImage      = false;
static char     g_currentMode   = 'c';   // 'a'=full NEW, 'b'=full LEGACY, 'c'=split
static char     g_pendingCmd   = 0;     // buffered command char for chained input
static bool     g_paramChanged = false; // batch flag: defer refresh until all cmds processed

// ----- helpers ---------------------------------------------------------------
static bool refresh_current_view() {
  switch (g_currentMode) {
    case 'a': return show_full_new(&g_originalImage);
    case 'b': return show_full_legacy(&g_originalImage);
    default:  return show_comparison(&g_originalImage);
  }
}
static bool copy_rgb_image(const RgbImage* src, RgbImage* dst) {
  const size_t bytes = (size_t)src->width * src->height * 3;
  dst->pixels = (uint8_t*)ps_malloc(bytes);
  if (!dst->pixels) dst->pixels = (uint8_t*)malloc(bytes);
  if (!dst->pixels) return false;
  memcpy(dst->pixels, src->pixels, bytes);
  dst->width  = src->width;
  dst->height = src->height;
  return true;
}

// ----- diagnostics -----------------------------------------------------------
static void log_mem(const char* tag) {
  LOG.printf("[mem] %-22s heap=%lu kB  PSRAM free=%lu/%lu kB\n", tag,
             (unsigned long)(ESP.getFreeHeap() / 1024),
             (unsigned long)(ESP.getFreePsram() / 1024),
             (unsigned long)(ESP.getPsramSize() / 1024));
  LOG.flush();
}

static void print_params() {
  LOG.println();
  LOG.println("--- Current NEW library parameters ---");
  LOG.printf("  method      = %s\n", SeeedDither::dither_method_name(NEW_METHOD));
  LOG.printf("  gamma       = %.2f  (g<value>)\n", NEW_GAMMA);
  LOG.printf("  serpentine  = %d     (p toggles)\n", NEW_SERPENTINE);
  LOG.printf("  legacyClamp = %d     (l toggles)\n", NEW_LEGACY_CLAMP);
  LOG.printf("  satBoost    = %.2f  (s<value>)\n", NEW_SAT_BOOST);
  LOG.printf("  darkness    = %.2f  (d<value>)\n", NEW_DARKNESS_BIAS);
  LOG.printf("  contrast    = %.2f  (t<value>)\n", NEW_CONTRAST);
  LOG.printf("  diffStrength= %.2f  (e<value>)\n", NEW_DIFF_STRENGTH);
  LOG.printf("  warmth      = %.2f  (w<value>)\n", NEW_WARMTH);
  LOG.printf("  colorMetric = %s     (m0..m6 cycles)\n",
             NEW_COLOR_METRIC == SeeedDither::METRIC_RGB ? "RGB" : "REDMEAN");
  LOG.printf("  customPal   = %s     (k toggles)\n", USE_CUSTOM_PALETTE ? "ON" : "OFF");
  if (USE_CUSTOM_PALETTE) {
    for (int i = 0; i < 6; ++i)
      LOG.printf("    %-6s = RGB(%3d,%3d,%3d)  (k%d to edit)\n",
                 PAL_NAMES[i], CUSTOM_PALETTE[i].r, CUSTOM_PALETTE[i].g, CUSTOM_PALETTE[i].b, i);
  }
  LOG.println("  ? = show this  |  a/b/c = display modes");
  LOG.println("----------------------------------------");
}

static void parse_param_cmd(char cmd) {
  switch (cmd) {
    case '?': print_params(); break;
    case 'p':
    case 'P': NEW_SERPENTINE = !NEW_SERPENTINE;
              LOG.printf(TAG " serpentine = %d\n", NEW_SERPENTINE);
              g_paramChanged = true;
              break;
    case 'l':
    case 'L': NEW_LEGACY_CLAMP = !NEW_LEGACY_CLAMP;
              LOG.printf(TAG " legacyClamp = %d\n", NEW_LEGACY_CLAMP);
              g_paramChanged = true;
              break;
    default: break;
  }
}

// Parse a float value command like "g1.2", "s0.3", "d0.1"
// Returns true if consumed.
// Supports chained input: "w0.2 t0.6" — the next command letter is buffered
// in g_pendingCmd so loop() can dispatch it immediately.
static bool parse_float_cmd(char prefix, float* target, const char* name) {
  // Read remaining digits and dot from serial buffer
  char buf[8] = {0};
  int i = 0;
  while (LOG.available() > 0 && i < 7) {
    char c = LOG.read();
    if (c == '\n' || c == '\r') break;
    if (c == ' ' || c < ' ') continue;
    // If this is a letter, it's the start of the next command — buffer it
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
      g_pendingCmd = c;
      break;
    }
    buf[i++] = c;
  }
  if (i == 0) {
    LOG.printf(TAG " %s = ? (need value, e.g. %c1.2)\n", name, prefix);
    return false;
  }
  float val = atof(buf);
  *target = val;
  LOG.printf(TAG " %s = %.2f\n", name, val);
  g_paramChanged = true;
  return true;
}

// Parse method switch: m0..m6
static bool parse_method_cmd() {
  char buf[4] = {0};
  int i = 0;
  while (LOG.available() > 0 && i < 3) {
    char c = LOG.read();
    if (c == '\n' || c == '\r') break;
    if (c == ' ' || c < ' ') continue;
    // If this is a letter, it's the start of the next command — buffer it
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
      g_pendingCmd = c;
      break;
    }
    if (c < '0' || c > '9') continue;
    buf[i++] = c;
  }
  if (i == 0) return false;
  int m = atoi(buf);
  switch (m) {
    case 0: NEW_METHOD = SeeedDither::DITHER_FS;        break;
    case 1: NEW_METHOD = SeeedDither::DITHER_ATKINSON;  break;
    case 2: NEW_METHOD = SeeedDither::DITHER_BURKES;    break;
    case 3: NEW_METHOD = SeeedDither::DITHER_SIERRA3;   break;
    case 4: NEW_METHOD = SeeedDither::DITHER_BAYER8;    break;
    case 5: NEW_METHOD = SeeedDither::DITHER_NONE;      break;
    case 6: NEW_METHOD = SeeedDither::DITHER_PALETTE_MIX; break;
    default: LOG.printf(TAG " unknown method %d (0..6)\n", m); return false;
  }
  LOG.printf(TAG " method = %s\n", SeeedDither::dither_method_name(NEW_METHOD));
  g_paramChanged = true;
  return true;
}

// Parse palette command: 'k' toggles, 'k0'..'k5' followed by R G B sets an entry.
//   k           → toggle custom palette on/off
//   k0 R G B    → set White  to RGB(R,G,B)
//   k1 R G B    → set Green  to RGB(R,G,B)
//   k2 R G B    → set Red    to RGB(R,G,B)
//   k3 R G B    → set Yellow to RGB(R,G,B)
//   k4 R G B    → set Blue   to RGB(R,G,B)
//   k5 R G B    → set Black  to RGB(R,G,B)
static bool parse_palette_cmd() {
  // Read the sub-command character
  int idx = -1;
  if (LOG.available() > 0) {
    char c = LOG.read();
    if (c == '\n' || c == '\r' || c == ' ') {
      // 'k' alone → toggle
      USE_CUSTOM_PALETTE = !USE_CUSTOM_PALETTE;
      LOG.printf(TAG " custom palette = %s\n", USE_CUSTOM_PALETTE ? "ON" : "OFF");
      if (USE_CUSTOM_PALETTE) {
        for (int i = 0; i < 6; ++i)
          LOG.printf(TAG "   %-6s = RGB(%3d,%3d,%3d)\n",
                     PAL_NAMES[i], CUSTOM_PALETTE[i].r, CUSTOM_PALETTE[i].g, CUSTOM_PALETTE[i].b);
      }
      g_paramChanged = true;
      return true;
    }
    if (c >= '0' && c <= '5') {
      idx = c - '0';
    } else {
      LOG.printf(TAG " palette: 0-5 expected, got '%c'\n", c);
      return false;
    }
  } else {
    // No sub-char → toggle
    USE_CUSTOM_PALETTE = !USE_CUSTOM_PALETTE;
    LOG.printf(TAG " custom palette = %s\n", USE_CUSTOM_PALETTE ? "ON" : "OFF");
    if (USE_CUSTOM_PALETTE) {
      for (int i = 0; i < 6; ++i)
        LOG.printf(TAG "   %-6s = RGB(%3d,%3d,%3d)\n",
                   PAL_NAMES[i], CUSTOM_PALETTE[i].r, CUSTOM_PALETTE[i].g, CUSTOM_PALETTE[i].b);
    }
    g_paramChanged = true;
    return true;
  }

  // Read three space-separated integers: R G B
  int vals[3] = {-1, -1, -1};
  for (int v = 0; v < 3; ++v) {
    char buf[8] = {0};
    int i = 0;
    while (LOG.available() > 0 && i < 7) {
      char c = LOG.read();
      if (c == '\n' || c == '\r') break;
      if (c == ' ' || c < ' ') {
        if (i > 0) break;  // space after number
        continue;           // leading space
      }
      if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
        g_pendingCmd = c;   // next command
        break;
      }
      if (c < '0' || c > '9') continue;
      buf[i++] = c;
    }
    if (i > 0) vals[v] = atoi(buf);
  }
  if (vals[0] < 0 || vals[1] < 0 || vals[2] < 0) {
    LOG.printf(TAG " palette: need R G B (e.g. k%d 35 178 80)\n", idx);
    return false;
  }
  CUSTOM_PALETTE[idx].r = (uint8_t)constrain(vals[0], 0, 255);
  CUSTOM_PALETTE[idx].g = (uint8_t)constrain(vals[1], 0, 255);
  CUSTOM_PALETTE[idx].b = (uint8_t)constrain(vals[2], 0, 255);
  USE_CUSTOM_PALETTE = true;  // auto-enable when editing
  LOG.printf(TAG " %s = RGB(%3d,%3d,%3d)\n",
             PAL_NAMES[idx], CUSTOM_PALETTE[idx].r, CUSTOM_PALETTE[idx].g, CUSTOM_PALETTE[idx].b);
  g_paramChanged = true;
  return true;
}

static void list_sd_root(int max_entries = 32) {
  File root = SD.open("/");
  if (!root || !root.isDirectory()) {
    LOG.println("[sd] cannot open '/' on the card");
    if (root) root.close();
    return;
  }
  LOG.println("[sd] contents of '/' :");
  int n = 0;
  File entry = root.openNextFile();
  while (entry) {
    if (entry.isDirectory()) LOG.printf("   <DIR>  %s\n", entry.name());
    else                     LOG.printf("   %7lu B  %s\n",
                                        (unsigned long)entry.size(), entry.name());
    entry.close();
    if (++n >= max_entries) { LOG.printf("   ... (truncated at %d entries)\n", max_entries); break; }
    entry = root.openNextFile();
  }
  if (n == 0) LOG.println("   (empty)");
  root.close();
  LOG.flush();
}

static bool is_supported_image_name(const String& name) {
  const int dot = name.lastIndexOf('.');
  if (dot < 0) return false;
  String ext = name.substring(dot);
  ext.toLowerCase();
  return ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".png";
}

static String first_image_in(const char* directory) {
  File dir = SD.open(directory);
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    return String();
  }
  String result;
  File entry = dir.openNextFile();
  while (entry) {
    if (!entry.isDirectory()) {
      String name(entry.name());
      if (is_supported_image_name(name)) {
        if (name.startsWith("/")) result = name;
        else if (name.indexOf('/') >= 0) result = String("/") + name;
        else {
          result = String(directory);
          if (!result.endsWith("/")) result += '/';
          result += name;
        }
        entry.close();
        break;
      }
    }
    entry.close();
    entry = dir.openNextFile();
  }
  dir.close();
  return result;
}

static String choose_image_path() {
  if (SD.exists(IMAGE_PATH)) return String(IMAGE_PATH);
  String fallback = first_image_in("/img");
  if (fallback.length() == 0) fallback = first_image_in("/");
  if (fallback.length()) {
    LOG.printf("[sd] '%s' not found; using first supported image '%s'\n",
               IMAGE_PATH, fallback.c_str());
  }
  return fallback;
}

// ----- show one full-screen comparison frame ---------------------------------
static bool show_comparison(const RgbImage* src) {
  // Copy the source image so we can resize it without affecting the original.
  RgbImage img;
  if (!copy_rgb_image(src, &img)) {
    LOG.println(TAG " OOM copying image for comparison");
    return false;
  }

  // Each pane shows the SAME image content, processed independently by the two
  // algorithms.  Resize the source to a single pane, then dither it twice.
  const int paneW = PANE_WIDTH;   // 399
  const int paneH = EPD_HEIGHT;   // 480
  const int fullW = EPD_WIDTH;    // 800
  const int fullH = EPD_HEIGHT;   // 480
  LOG.printf("[layout] src=%dx%d -> pane %dx%d  (combined %dx%d)\n",
             img.width, img.height, paneW, paneH, fullW, fullH);

  if (paneW != img.width || paneH != img.height) {
    const size_t need_kb = (size_t)paneW * paneH * 3 / 1024;
    LOG.printf("[layout] resizing %dx%d -> %dx%d  (needs +%lu kB temporarily)\n",
               img.width, img.height, paneW, paneH, (unsigned long)need_kb);
    log_mem("before resize");
    if (!resize_image(&img, paneW, paneH)) {
      LOG.println("[layout] resize failed (likely OOM) -- aborting");
      image_free(&img);
      return false;
    }
    log_mem("after resize");
  }

  const size_t paneNpx = (size_t)paneW * paneH;

  // Allocate TWO index buffers (1 byte per pixel, E6 index codes) for the pane.
  log_mem("before idx malloc");
  LOG.printf(TAG " allocating 2x index buf: %lu kB each\n",
             (unsigned long)(paneNpx / 1024));
  uint8_t* idx_new = (uint8_t*)ps_malloc(paneNpx);
  if (!idx_new) idx_new = (uint8_t*)malloc(paneNpx);
  uint8_t* idx_legacy = (uint8_t*)ps_malloc(paneNpx);
  if (!idx_legacy) idx_legacy = (uint8_t*)malloc(paneNpx);
  if (!idx_new || !idx_legacy) {
    LOG.println(TAG " OOM idx -- aborting");
    free(idx_new); free(idx_legacy);
    return false;
  }

  // ---- NEW library dither (LEFT pane) ----
  LOG.printf(TAG " NEW library dither (%s, gamma=%.2f, serp=%d, legacyClamp=%d, sat=%.2f, dark=%.2f) ...\n",
             SeeedDither::dither_method_name(NEW_METHOD), NEW_GAMMA, NEW_SERPENTINE,
             NEW_LEGACY_CLAMP, NEW_SAT_BOOST, NEW_DARKNESS_BIAS);
  {
    SeeedDither::DitherConfig cfg;
    cfg.method          = NEW_METHOD;
    cfg.palette         = SeeedDither::PAL_E6;
    cfg.gamma           = NEW_GAMMA;
    cfg.serpentine      = NEW_SERPENTINE;
    cfg.legacyClamp     = NEW_LEGACY_CLAMP;
    cfg.saturationBoost = NEW_SAT_BOOST;
    cfg.darknessBias    = NEW_DARKNESS_BIAS;
    cfg.contrast        = NEW_CONTRAST;
    cfg.errorDiffusionStrength = NEW_DIFF_STRENGTH;
    cfg.warmth          = NEW_WARMTH;
    cfg.colorMetric     = NEW_COLOR_METRIC;
    if (USE_CUSTOM_PALETTE) {
      cfg.customPaletteRgb   = CUSTOM_PALETTE;
      cfg.customPaletteCode  = CUSTOM_CODE;
      cfg.customPaletteCount = 6;
    }

    const uint32_t t0 = millis();
    if (!SeeedDither::dither_image_ex(img.pixels, paneW, paneH, cfg, idx_new)) {
      LOG.println(TAG " NEW dither failed -- aborting");
      free(idx_new); free(idx_legacy);
      return false;
    }
    LOG.printf(TAG " NEW dither done in %lu ms\n", (unsigned long)(millis() - t0));
  }

  // ---- LEGACY dither (RIGHT pane) ----
  LOG.printf(TAG " LEGACY dither (FS, gamma=%.2f, [0,255] clamp, int16_t, no serpentine) ...\n",
             LEGACY_GAMMA);
  {
    const uint32_t t0 = millis();
    if (!LegacyDither::dither_image(img.pixels, paneW, paneH,
                                     LegacyDither::PAL_E6, LEGACY_METHOD,
                                     LEGACY_GAMMA, LEGACY_INVERT,
                                     idx_legacy)) {
      LOG.println(TAG " LEGACY dither failed -- aborting");
      free(idx_new); free(idx_legacy);
      return false;
    }
    LOG.printf(TAG " LEGACY dither done in %lu ms\n", (unsigned long)(millis() - t0));
  }

  // Free the working copy — original is kept for interactive mode switching.
  LOG.println(TAG " freeing RGB888 working copy");
  image_free(&img);
  log_mem("after RGB888 freed");

  // ---- Combine into a full-screen buffer: LEFT=NEW | DIVIDER | RIGHT=LEGACY ----
  const size_t fullNpx = (size_t)fullW * fullH;
  LOG.printf(TAG " combining:  LEFT %dpx = NEW  |  %dpx divider  |  RIGHT %dpx = LEGACY\n",
             paneW, DIVIDER_WIDTH, paneW);
  uint8_t* combined = (uint8_t*)ps_malloc(fullNpx);
  if (!combined) combined = (uint8_t*)malloc(fullNpx);
  if (!combined) {
    LOG.println(TAG " OOM combined buffer -- aborting");
    free(idx_new); free(idx_legacy);
    return false;
  }

  for (int y = 0; y < paneH; ++y) {
    const size_t srcOff = (size_t)y * paneW;
    const size_t dstOff = (size_t)y * fullW;
    // LEFT: copy the entire NEW-dithered row
    memcpy(combined + dstOff, idx_new + srcOff, paneW);
    // DIVIDER: 2px black (E6 code 0x0F = BLACK)
    combined[dstOff + paneW]     = 0x0F;
    combined[dstOff + paneW + 1] = 0x0F;
    // RIGHT: copy the entire LEGACY-dithered row
    memcpy(combined + dstOff + paneW + DIVIDER_WIDTH, idx_legacy + srcOff, paneW);
  }
  free(idx_new);
  free(idx_legacy);

  // Pack combined to 4bpp in-place (fullW=800 is even, no padding needed).
  SeeedDither::pack_4bpp_in_place(combined, fullW, fullH, 0x00);

  // Push the full split-screen panel.
  LOG.printf(TAG " pushImage4BPP %dx%d at (0,0)\n", fullW, fullH);
  if (!display.pushImage4BPP(0, 0, fullW, fullH, combined)) {
    LOG.println(TAG " pushImage4BPP rejected");
    free(combined);
    return false;
  }

  LOG.println(TAG " refreshing panel ...");
  LOG.flush();
  const uint32_t t1 = millis();
  const GfxResult refreshResult = display.refresh();
  LOG.printf(TAG " refresh done in %lu ms\n", (unsigned long)(millis() - t1));

  free(combined);

  if (!refreshResult) {
    LOG.printf(TAG " refresh failed: %s\n", refreshResult.message);
    return false;
  }

  LOG.println(TAG " === COMPARISON READY ===");
  LOG.println(TAG " LEFT  = NEW library  |  RIGHT = LEGACY dither");
  LOG.println(TAG " Serial commands: a=Full NEW  b=Full LEGACY  c=Split comparison");
  return true;
}

// ----- full-screen: NEW library only -----------------------------------------
static bool show_full_new(const RgbImage* src) {
  RgbImage img;
  if (!copy_rgb_image(src, &img)) {
    LOG.println(TAG " OOM copying image for full NEW");
    return false;
  }

  LOG.printf("[layout] src=%dx%d -> fullscreen %dx%d (NEW)\n",
             img.width, img.height, EPD_WIDTH, EPD_HEIGHT);
  // Image is already 800x480 from the stored copy — no resize needed.

  const size_t npx = (size_t)EPD_WIDTH * EPD_HEIGHT;
  uint8_t* idx = (uint8_t*)ps_malloc(npx);
  if (!idx) idx = (uint8_t*)malloc(npx);
  if (!idx) { image_free(&img); return false; }

  LOG.printf(TAG " NEW library full-screen dither (%s, gamma=%.2f, serp=%d, legacyClamp=%d, sat=%.2f, dark=%.2f) ...\n",
             SeeedDither::dither_method_name(NEW_METHOD), NEW_GAMMA, NEW_SERPENTINE,
             NEW_LEGACY_CLAMP, NEW_SAT_BOOST, NEW_DARKNESS_BIAS);
  {
    SeeedDither::DitherConfig cfg;
    cfg.method          = NEW_METHOD;
    cfg.palette         = SeeedDither::PAL_E6;
    cfg.gamma           = NEW_GAMMA;
    cfg.serpentine      = NEW_SERPENTINE;
    cfg.legacyClamp     = NEW_LEGACY_CLAMP;
    cfg.saturationBoost = NEW_SAT_BOOST;
    cfg.darknessBias    = NEW_DARKNESS_BIAS;
    cfg.contrast        = NEW_CONTRAST;
    cfg.errorDiffusionStrength = NEW_DIFF_STRENGTH;
    cfg.warmth          = NEW_WARMTH;
    cfg.colorMetric     = NEW_COLOR_METRIC;
    if (USE_CUSTOM_PALETTE) {
      cfg.customPaletteRgb   = CUSTOM_PALETTE;
      cfg.customPaletteCode  = CUSTOM_CODE;
      cfg.customPaletteCount = 6;
    }

    const uint32_t t0 = millis();
    if (!SeeedDither::dither_image_ex(img.pixels, EPD_WIDTH, EPD_HEIGHT, cfg, idx)) {
      LOG.println(TAG " NEW dither failed");
      free(idx); image_free(&img);
      return false;
    }
    LOG.printf(TAG " NEW dither done in %lu ms\n", (unsigned long)(millis() - t0));
  }
  image_free(&img);

  SeeedDither::pack_4bpp_in_place(idx, EPD_WIDTH, EPD_HEIGHT, 0x00);

  LOG.printf(TAG " pushImage4BPP %dx%d (FULL NEW)\n", EPD_WIDTH, EPD_HEIGHT);
  if (!display.pushImage4BPP(0, 0, EPD_WIDTH, EPD_HEIGHT, idx)) {
    LOG.println(TAG " pushImage4BPP rejected");
    free(idx);
    return false;
  }

  LOG.println(TAG " refreshing panel ...");
  const uint32_t t1 = millis();
  const GfxResult refreshResult = display.refresh();
  LOG.printf(TAG " refresh done in %lu ms\n", (unsigned long)(millis() - t1));
  free(idx);

  if (!refreshResult) {
    LOG.printf(TAG " refresh failed: %s\n", refreshResult.message);
    return false;
  }
  LOG.println(TAG " === FULL NEW (left-side library) ===");
  LOG.println(TAG " Serial commands: a=Full NEW  b=Full LEGACY  c=Split comparison");
  return true;
}

// ----- full-screen: LEGACY library only --------------------------------------
static bool show_full_legacy(const RgbImage* src) {
  RgbImage img;
  if (!copy_rgb_image(src, &img)) {
    LOG.println(TAG " OOM copying image for full LEGACY");
    return false;
  }

  LOG.printf("[layout] src=%dx%d -> fullscreen %dx%d (LEGACY)\n",
             img.width, img.height, EPD_WIDTH, EPD_HEIGHT);
  // Image is already 800x480 from the stored copy — no resize needed.

  const size_t npx = (size_t)EPD_WIDTH * EPD_HEIGHT;
  uint8_t* idx = (uint8_t*)ps_malloc(npx);
  if (!idx) idx = (uint8_t*)malloc(npx);
  if (!idx) { image_free(&img); return false; }

  LOG.printf(TAG " LEGACY full-screen dither (FS, gamma=%.2f, [0,255] clamp, int16_t) ...\n",
             LEGACY_GAMMA);
  {
    const uint32_t t0 = millis();
    if (!LegacyDither::dither_image(img.pixels, EPD_WIDTH, EPD_HEIGHT,
                                     LegacyDither::PAL_E6, LEGACY_METHOD,
                                     LEGACY_GAMMA, LEGACY_INVERT, idx)) {
      LOG.println(TAG " LEGACY dither failed");
      free(idx); image_free(&img);
      return false;
    }
    LOG.printf(TAG " LEGACY dither done in %lu ms\n", (unsigned long)(millis() - t0));
  }
  image_free(&img);

  SeeedDither::pack_4bpp_in_place(idx, EPD_WIDTH, EPD_HEIGHT, 0x00);

  LOG.printf(TAG " pushImage4BPP %dx%d (FULL LEGACY)\n", EPD_WIDTH, EPD_HEIGHT);
  if (!display.pushImage4BPP(0, 0, EPD_WIDTH, EPD_HEIGHT, idx)) {
    LOG.println(TAG " pushImage4BPP rejected");
    free(idx);
    return false;
  }

  LOG.println(TAG " refreshing panel ...");
  const uint32_t t1 = millis();
  const GfxResult refreshResult = display.refresh();
  LOG.printf(TAG " refresh done in %lu ms\n", (unsigned long)(millis() - t1));
  free(idx);

  if (!refreshResult) {
    LOG.printf(TAG " refresh failed: %s\n", refreshResult.message);
    return false;
  }
  LOG.println(TAG " === FULL LEGACY (original library) ===");
  LOG.println(TAG " Serial commands: a=Full NEW  b=Full LEGACY  c=Split comparison");
  return true;
}

void setup() {
  LOG.begin(115200, SERIAL_8N1, PIN_DBG_RX, PIN_DBG_TX);
  delay(2500);
  LOG.println();
  LOG.println("==============================================");
  LOG.println("  reTerminal E1002 -- Dither Comparison");
  LOG.println("  LEFT = NEW library  |  RIGHT = LEGACY");
  LOG.println("==============================================");
  LOG.printf("[sys] chip      : ESP32-S3 @ %lu MHz\n", (unsigned long)ESP.getCpuFreqMHz());
  LOG.printf("[sys] PSRAM size: %lu kB\n", (unsigned long)(ESP.getPsramSize() / 1024));
  if (ESP.getPsramSize() == 0) {
    LOG.println("[sys] !!! PSRAM is 0 kB -- enable Tools > PSRAM > OPI PSRAM in the IDE !!!");
  }
  LOG.printf("[sys] panel     : %d x %d\n", EPD_WIDTH, EPD_HEIGHT);
  LOG.printf("[sys] image     : '%s'\n", IMAGE_PATH);
  LOG.flush();

  pinMode(PIN_SD_EN, OUTPUT);
  digitalWrite(PIN_SD_EN, HIGH);
  pinMode(PIN_SD_DET, INPUT_PULLUP);
  delay(50);
  const int sd_det = digitalRead(PIN_SD_DET);
  LOG.printf("[sd] SD_DET (GPIO%d) reads %s -- %s\n", PIN_SD_DET,
             sd_det ? "HIGH" : "LOW",
             sd_det ? "no card detected (or DET pin floating)" : "card present");
  LOG.flush();

  LOG.println(TAG " display.begin() ...");
  if (!display.begin()) {
    LOG.printf(TAG " display.begin failed: %s\n", display.lastResult().message);
    return;
  }
  log_mem("after display.begin");

  Bus_SPI& displayBus = static_cast<Bus_SPI&>(display.panel().driver().bus());
  SPIClass* spi = displayBus.spiInstance();
  if (!spi) {
    LOG.println(TAG " shared SPI instance unavailable -- aborting");
    return;
  }

  LOG.println(TAG " SD.begin (shares HSPI with ePaper) ...");
  if (!SD.begin(PIN_SD_CS, *spi)) {
    LOG.println(TAG " SD.begin FAILED -- aborting");
    LOG.println(TAG "   - Is the card inserted and formatted as FAT/FAT32?");
    LOG.printf (TAG "   - Is SD_EN (GPIO%d) actually wired to power the slot?\n", PIN_SD_EN);
    return;
  }
  LOG.printf("[sd] mounted; card size = %llu MB\n",
             (unsigned long long)(SD.cardSize() / (1024ULL * 1024ULL)));
  list_sd_root();

  const String imagePath = choose_image_path();
  if (imagePath.length() == 0) {
    LOG.printf(TAG " no supported image found (configured path: '%s')\n", IMAGE_PATH);
    LOG.println(TAG "   - copy a JPG, BMP, or PNG to '/img' or the card root");
    return;
  }
  {
    File probe = SD.open(imagePath.c_str(), FILE_READ);
    if (probe) {
      LOG.printf(TAG " image found: %s (%lu bytes)\n",
                 imagePath.c_str(), (unsigned long)probe.size());
      probe.close();
    }
  }

  log_mem("before image load");
  if (!load_image_from_sd(imagePath.c_str(), /*target_w=*/0, /*target_h=*/0, &g_originalImage)) {
    LOG.println(TAG " load failed -- aborting");
    return;
  }
  g_hasImage = true;
  LOG.printf(TAG " image decoded: %dx%d  (%lu kB in PSRAM)\n",
             g_originalImage.width, g_originalImage.height,
             (unsigned long)((size_t)g_originalImage.width * g_originalImage.height * 3 / 1024));
  log_mem("after image decoded");

  // Immediately resize to panel size to keep memory footprint low.
  // The original 1280×853 (~3.3 MB) is replaced by 800×480 (~1.1 MB).
  if (g_originalImage.width != EPD_WIDTH || g_originalImage.height != EPD_HEIGHT) {
    LOG.printf(TAG " resizing stored image %dx%d -> %dx%d\n",
               g_originalImage.width, g_originalImage.height, EPD_WIDTH, EPD_HEIGHT);
    if (!resize_image(&g_originalImage, EPD_WIDTH, EPD_HEIGHT)) {
      LOG.println(TAG " initial resize failed (OOM) -- aborting");
      image_free(&g_originalImage);
      g_hasImage = false;
      return;
    }
    log_mem("after initial resize");
  }

  // Default: split-screen comparison.
  LOG.println(TAG " ========================================");
  LOG.println(TAG " Serial commands:");
  LOG.println(TAG "   a = Full-screen NEW library");
  LOG.println(TAG "   b = Full-screen LEGACY library");
  LOG.println(TAG "   c = Split comparison (default)");
  LOG.println(TAG "   ? = Print all parameters");
  LOG.println(TAG " ========================================");
  if (show_comparison(&g_originalImage)) LOG.println(TAG " comparison displayed OK");
  else                                    LOG.println(TAG " comparison failed");

  LOG.println(TAG " Ready.  Type a, b, or c in the serial monitor.");
  LOG.println("==============================================");
}

void loop() {
  // Process ALL queued commands in a tight loop, then refresh once at the end.
  // This prevents screen flicker when you paste "m3 g1.0 s0.10 ..." all at once.
  g_paramChanged = false;

  while (true) {
    char cmd = g_pendingCmd;
    if (cmd) {
      g_pendingCmd = 0;
    } else if (LOG.available() > 0) {
      cmd = LOG.read();
    } else {
      break;  // No more commands — done batching
    }

    switch (cmd) {
      case 'a': case 'A':
        g_currentMode = 'a';
        LOG.println();
        LOG.println(TAG " === switching to FULL NEW ===");
        show_full_new(&g_originalImage);
        break;
      case 'b': case 'B':
        g_currentMode = 'b';
        LOG.println();
        LOG.println(TAG " === switching to FULL LEGACY ===");
        show_full_legacy(&g_originalImage);
        break;
      case 'c': case 'C':
        g_currentMode = 'c';
        LOG.println();
        LOG.println(TAG " === switching to SPLIT COMPARISON ===");
        show_comparison(&g_originalImage);
        break;
      case 'g': case 'G':
        parse_float_cmd('g', &NEW_GAMMA, "gamma");
        break;
      case 's': case 'S':
        parse_float_cmd('s', &NEW_SAT_BOOST, "satBoost");
        break;
      case 'd': case 'D':
        parse_float_cmd('d', &NEW_DARKNESS_BIAS, "darknessBias");
        break;
      case 't': case 'T':
        parse_float_cmd('t', &NEW_CONTRAST, "contrast");
        break;
      case 'e': case 'E':
        parse_float_cmd('e', &NEW_DIFF_STRENGTH, "diffStrength");
        break;
      case 'w': case 'W':
        parse_float_cmd('w', &NEW_WARMTH, "warmth");
        break;
      case 'm': case 'M':
        parse_method_cmd();
        break;
      case 'k': case 'K':
        parse_palette_cmd();
        break;
      case 'p': case 'P':
      case 'l': case 'L':
      case '?':
        parse_param_cmd(cmd);
        break;
      default:
        break;
    }
  }

  // Single refresh after all parameter changes have been batched
  if (g_paramChanged) {
    LOG.printf(TAG " refreshing display...\n");
    LOG.flush();
    refresh_current_view();
  }

  delay(100);
}
