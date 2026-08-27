/*
 * reTerminal E1004 -- SD card image to 6-color e-paper: DITHER COMPARISON.
 *
 * Shows the SAME resized image twice on the portrait 1200x1600 panel,
 * separated by a BLACK horizontal line. Each pane dithers the complete
 * 1200x799 source image independently:
 *   TOP    pane (rows 0..798)    = NEW Seeed_GFX2 library dither (dither_image_4bpp)
 *   BOTTOM pane (rows 801..1599) = Legacy Seeed_GFX dither (LegacyDither::dither_image)
 *
 * Why a TOP/BOTTOM split (unlike the E1002 LEFT/RIGHT split)?  The E1004 panel
 * is portrait.  Two side-by-side panes would be 599x1600 each -- a 0.37:1
 * aspect that squashes typical photos beyond recognition.  Top/bottom panes
 * are 1200x799 (about 3:2), which fits landscape photos naturally.
 *
 * The main difference between the two dither engines is the error-buffer
 * clamping:
 *   NEW  library: clampBuf [-255, 510], int32_t, optional serpentine scan
 *   LEGACY copy:  clamp_u8  [0, 255],  int16_t, always left-to-right
 *
 * All other baseline parameters (FS method, gamma=1.0, palette=E6) are
 * identical, so the comparison is fair: both engines dither the exact same
 * pane-sized source, never a cropped half of a bigger dither.
 *
 * MEMORY BUDGET (1200x1600 panel + XIAO ESP32-S3 8 MB PSRAM):
 * The E1002 strategy (store the full-panel RGB image, copy it, resize the
 * copy, keep two index buffers, combine, then pack) would need ~12 MB here.
 * Instead this sketch:
 *   - stores the image pre-resized to ONE PANE (1200x799 RGB = 2.87 MB),
 *   - builds the combined frame DIRECTLY in packed 4bpp (960 KB),
 *   - dithers the two panes sequentially: the LEGACY pane uses one
 *     1-byte-per-pixel index buffer (936 KB) that is freed before the NEW
 *     pane is dithered straight to packed 4bpp (468 KB) via dither_image_4bpp.
 * Peak extra PSRAM is about 4.8 MB on top of the 960 KB display frame buffer.
 *
 * Display modes:
 *   c = split:        TOP = NEW, BOTTOM = LEGACY (default)
 *   a = full NEW:     one pane dithered by the NEW library, vertically centered
 *   b = full LEGACY:  one pane dithered by the LEGACY library, centered
 * Full modes show a single 1200x799 pane centered at y=400 with white bands
 * above and below -- upscaling the pane to 1600 rows would need a 5.76 MB RGB
 * buffer and is deliberately avoided.
 *
 * Hardware: reTerminal E1004 (XIAO ESP32-S3 + 13.3" T133A01 6-color e-paper).
 *
 *           Function     ESP32-S3 GPIO
 *           SPI SCK      7   (shared with SD)
 *           SPI MISO     8   (shared with SD)
 *           SPI MOSI     9   (shared with SD)
 *           EPD CS       10
 *           EPD CS1      2    (this panel needs two chip selects)
 *           EPD DC       11
 *           EPD BUSY     13
 *           EPD RST      38
 *           EPD ENABLE   12
 *           SD  CS       14
 *           SD  EN       16
 *           SD  DET      15
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

Seeed_GFX display(Seeed_Product::RETERMINAL_E1004);
static DitherContext ditherContext;

// ----- panel geometry (portrait) ----------------------------------------------
static constexpr int EPD_WIDTH  = 1200;
static constexpr int EPD_HEIGHT = 1600;
static constexpr int DIVIDER_HEIGHT = 2;
static constexpr int PANE_HEIGHT = (EPD_HEIGHT - DIVIDER_HEIGHT) / 2;  // 799
static constexpr int PANE_WIDTH  = EPD_WIDTH;                          // 1200

// Packed 4bpp geometry.  Pane width == panel width (even), so packed rows of
// a pane align byte-for-byte with packed rows of the full panel.
static constexpr size_t FULL_PACKED_ROW   = (EPD_WIDTH + 1) / 2;                  // 600
static constexpr size_t FULL_PACKED_BYTES = FULL_PACKED_ROW * EPD_HEIGHT;         // 960000
static constexpr size_t PANE_PACKED_BYTES = FULL_PACKED_ROW * PANE_HEIGHT;        // 479400
static constexpr size_t PANE_IDX_BYTES    = (size_t)PANE_WIDTH * PANE_HEIGHT;     // 958800

// Vertical position of the pane in the full-screen modes ('a' / 'b').
static constexpr int FULL_PANE_Y0 = (EPD_HEIGHT - PANE_HEIGHT) / 2;               // 400

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
#define TAG       "[e1004-cmp]"

// ----- user configuration ----------------------------------------------------

// Path to the image on the SD card (must start with '/').
static const char* IMAGE_PATH = "/img/demo.jpg";

// =========================================================================
//  NEW library parameters (Seeed_GFX2) — TUNABLE
//  Adjust these to find the best settings for different image styles.
// =========================================================================

// Dither algorithm: FS | ATKINSON | BURKES | SIERRA3 | BAYER8 | NONE | PALETTE_MIX
static DitherMethod NEW_METHOD = DITHER_FS;

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
static ColorMetric NEW_COLOR_METRIC = METRIC_RGB;

// ----- Custom palette calibration -------------------------------------------
// Toggle with 'k'.  Edit individual entries with k0..k5 R G B.
// When enabled, the dithering algorithm targets these RGB values instead of the
// default E6 reference.  This lets you calibrate to your panel's actual pigments.
//   Index:  0=White(0x0)  1=Green(0x2)  2=Red(0x6)  3=Yellow(0xB)  4=Blue(0xD)  5=Black(0xF)
static bool USE_CUSTOM_PALETTE = false;
// Default sRGB reference values for E6 Spectra pigments.
// These are THEORETICAL — calibrate with your phone (see PARAM_GUIDE.md) and
// replace with measured values to match YOUR panel's actual colors.
static Rgb CUSTOM_PALETTE[6] = {
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
// Stored pre-resized to ONE PANE (1200x799) -- see MEMORY BUDGET above.
static RgbImage g_originalImage = {nullptr, 0, 0};
static bool     g_hasImage      = false;
static char     g_currentMode   = 'c';   // 'a'=full NEW, 'b'=full LEGACY, 'c'=split
static char     g_pendingCmd    = 0;     // buffered command char for chained input
static bool     g_paramChanged  = false; // batch flag: defer refresh until all cmds processed

// ----- forward declarations ----------------------------------------------------
static bool show_comparison(const RgbImage* src);
static bool show_full_new(const RgbImage* src);
static bool show_full_legacy(const RgbImage* src);

// ----- helpers ---------------------------------------------------------------
static bool refresh_current_view() {
  switch (g_currentMode) {
    case 'a': return show_full_new(&g_originalImage);
    case 'b': return show_full_legacy(&g_originalImage);
    default:  return show_comparison(&g_originalImage);
  }
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
  LOG.printf("  method      = %s\n", dither_method_name(NEW_METHOD));
  LOG.printf("  gamma       = %.2f  (g<value>)\n", NEW_GAMMA);
  LOG.printf("  serpentine  = %d     (p toggles)\n", NEW_SERPENTINE);
  LOG.printf("  legacyClamp = %d     (l toggles)\n", NEW_LEGACY_CLAMP);
  LOG.printf("  satBoost    = %.2f  (s<value>)\n", NEW_SAT_BOOST);
  LOG.printf("  darkness    = %.2f  (d<value>)\n", NEW_DARKNESS_BIAS);
  LOG.printf("  contrast    = %.2f  (t<value>)\n", NEW_CONTRAST);
  LOG.printf("  diffStrength= %.2f  (e<value>)\n", NEW_DIFF_STRENGTH);
  LOG.printf("  warmth      = %.2f  (w<value>)\n", NEW_WARMTH);
  LOG.printf("  colorMetric = %s     (m0..m6 cycles)\n",
             NEW_COLOR_METRIC == METRIC_RGB ? "RGB" : "REDMEAN");
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
    case 0: NEW_METHOD = DITHER_FS;          break;
    case 1: NEW_METHOD = DITHER_ATKINSON;    break;
    case 2: NEW_METHOD = DITHER_BURKES;      break;
    case 3: NEW_METHOD = DITHER_SIERRA3;     break;
    case 4: NEW_METHOD = DITHER_BAYER8;      break;
    case 5: NEW_METHOD = DITHER_NONE;        break;
    case 6: NEW_METHOD = DITHER_PALETTE_MIX; break;
    default: LOG.printf(TAG " unknown method %d (0..6)\n", m); return false;
  }
  LOG.printf(TAG " method = %s\n", dither_method_name(NEW_METHOD));
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

// ----- NEW library config from the live-tunable globals ----------------------
static DitherConfig makeNewConfig() {
  DitherConfig cfg;
  cfg.method          = NEW_METHOD;
  cfg.palette         = PAL_E6;
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
  return cfg;
}

// ----- pane blitting into the packed full-panel buffer ------------------------

// Copy a packed-4bpp pane (panel width, so rows align) into the combined
// full-panel packed buffer at row dstRow.
static void blit_pane_packed(uint8_t* combined, const uint8_t* panePacked, int dstRow) {
  for (int y = 0; y < PANE_HEIGHT; ++y) {
    memcpy(combined + ((size_t)dstRow + y) * FULL_PACKED_ROW,
           panePacked + (size_t)y * FULL_PACKED_ROW, FULL_PACKED_ROW);
  }
}

// Pack a 1-byte-per-pixel legacy index pane into the combined full-panel
// packed buffer at row dstRow.  E6 codes are 4-bit, so two pixels per byte.
static void blit_pane_idx(uint8_t* combined, const uint8_t* idx, int dstRow) {
  for (int y = 0; y < PANE_HEIGHT; ++y) {
    const uint8_t* srcRow = idx + (size_t)y * PANE_WIDTH;
    uint8_t* dstRowP = combined + ((size_t)dstRow + y) * FULL_PACKED_ROW;
    for (size_t xb = 0; xb < FULL_PACKED_ROW; ++xb)
      dstRowP[xb] = (uint8_t)((srcRow[2 * xb] << 4) | (srcRow[2 * xb + 1] & 0x0F));
  }
}

// Allocate the packed full-panel buffer, pre-filled with WHITE (code 0x0).
static uint8_t* alloc_full_packed() {
  uint8_t* combined = (uint8_t*)ps_malloc(FULL_PACKED_BYTES);
  if (!combined) combined = (uint8_t*)malloc(FULL_PACKED_BYTES);
  if (combined) memset(combined, 0x00, FULL_PACKED_BYTES);
  return combined;
}

// Push the combined full-panel buffer and refresh.  Does NOT free the buffer.
static bool push_full_panel(uint8_t* combined, const char* what) {
  LOG.printf(TAG " pushImage4BPP %dx%d at (0,0) (%s)\n", EPD_WIDTH, EPD_HEIGHT, what);
  if (!display.pushImage4BPP(0, 0, EPD_WIDTH, EPD_HEIGHT, combined)) {
    LOG.println(TAG " pushImage4BPP rejected");
    return false;
  }

  LOG.println(TAG " refreshing panel ...");
  LOG.flush();
  const uint32_t t1 = millis();
  const GfxResult refreshResult = display.refresh();
  LOG.printf(TAG " refresh done in %lu ms\n", (unsigned long)(millis() - t1));

  if (!refreshResult) {
    LOG.printf(TAG " refresh failed: %s\n", refreshResult.message);
    return false;
  }
  return true;
}

// Dither the stored pane with the LEGACY engine into a 1-byte-per-pixel index
// buffer.  Takes and logs the legacy working buffer allocation.
static bool dither_legacy_pane(const RgbImage* src, uint8_t* idx) {
  LOG.printf(TAG " LEGACY dither (FS, gamma=%.2f, [0,255] clamp, int16_t, no serpentine) ...\n",
             LEGACY_GAMMA);
  const uint32_t t0 = millis();
  if (!LegacyDither::dither_image(src->pixels, PANE_WIDTH, PANE_HEIGHT,
                                  LegacyDither::PAL_E6, LEGACY_METHOD,
                                  LEGACY_GAMMA, LEGACY_INVERT, idx)) {
    LOG.println(TAG " LEGACY dither failed");
    return false;
  }
  LOG.printf(TAG " LEGACY dither done in %lu ms\n", (unsigned long)(millis() - t0));
  return true;
}

// Dither the stored pane with the NEW engine directly into packed 4bpp.
static bool dither_new_pane(const RgbImage* src, uint8_t* panePacked) {
  LOG.printf(TAG " NEW library dither (%s, gamma=%.2f, serp=%d, legacyClamp=%d, sat=%.2f, dark=%.2f) ...\n",
             dither_method_name(NEW_METHOD), NEW_GAMMA, NEW_SERPENTINE,
             NEW_LEGACY_CLAMP, NEW_SAT_BOOST, NEW_DARKNESS_BIAS);
  const DitherConfig cfg = makeNewConfig();
  const uint32_t t0 = millis();
  if (!dither_image_4bpp(src->pixels, PANE_WIDTH, PANE_HEIGHT, cfg,
                         ditherContext, panePacked)) {
    LOG.println(TAG " NEW dither failed");
    return false;
  }
  LOG.printf(TAG " NEW dither done in %lu ms\n", (unsigned long)(millis() - t0));
  return true;
}

// ----- show one split-screen comparison frame --------------------------------
static bool show_comparison(const RgbImage* src) {
  LOG.printf("[layout] pane %dx%d  TOP=NEW | %d-row divider | BOTTOM=LEGACY  (panel %dx%d)\n",
             PANE_WIDTH, PANE_HEIGHT, DIVIDER_HEIGHT, EPD_WIDTH, EPD_HEIGHT);

  uint8_t* combined = alloc_full_packed();
  if (!combined) {
    LOG.println(TAG " OOM combined packed buffer -- aborting");
    return false;
  }

  // ---- LEGACY dither (BOTTOM pane) first: it needs the larger buffer ----
  log_mem("before legacy idx malloc");
  uint8_t* idx = (uint8_t*)ps_malloc(PANE_IDX_BYTES);
  if (!idx) idx = (uint8_t*)malloc(PANE_IDX_BYTES);
  if (!idx) {
    LOG.println(TAG " OOM legacy index buffer -- aborting");
    free(combined);
    return false;
  }
  bool ok = dither_legacy_pane(src, idx);
  if (ok) {
    blit_pane_idx(combined, idx, PANE_HEIGHT + DIVIDER_HEIGHT);  // rows 801..1599
  }
  free(idx);
  log_mem("after legacy idx freed");
  if (!ok) { free(combined); return false; }

  // ---- NEW library dither (TOP pane), directly to packed 4bpp ----
  uint8_t* panePacked = (uint8_t*)ps_malloc(PANE_PACKED_BYTES);
  if (!panePacked) panePacked = (uint8_t*)malloc(PANE_PACKED_BYTES);
  if (!panePacked) {
    LOG.println(TAG " OOM NEW pane packed buffer -- aborting");
    free(combined);
    return false;
  }
  ok = dither_new_pane(src, panePacked);
  if (ok) {
    blit_pane_packed(combined, panePacked, 0);  // rows 0..798
  }
  free(panePacked);
  if (!ok) { free(combined); return false; }

  // ---- Divider: 2 rows of black (E6 code 0xF = packed byte 0xFF) ----
  memset(combined + (size_t)PANE_HEIGHT * FULL_PACKED_ROW, 0xFF,
         (size_t)DIVIDER_HEIGHT * FULL_PACKED_ROW);

  LOG.println(TAG " combining:  TOP = NEW  |  divider  |  BOTTOM = LEGACY");
  const bool pushed = push_full_panel(combined, "split NEW/LEGACY");
  free(combined);
  if (!pushed) return false;

  LOG.println(TAG " === COMPARISON READY ===");
  LOG.println(TAG " TOP = NEW library  |  BOTTOM = LEGACY dither");
  LOG.println(TAG " Serial commands: a=Full NEW  b=Full LEGACY  c=Split comparison");
  return true;
}

// ----- full-screen: NEW library only -----------------------------------------
static bool show_full_new(const RgbImage* src) {
  LOG.printf("[layout] pane %dx%d centered at y=%d  (panel %dx%d, NEW)\n",
             PANE_WIDTH, PANE_HEIGHT, FULL_PANE_Y0, EPD_WIDTH, EPD_HEIGHT);

  uint8_t* combined = alloc_full_packed();
  if (!combined) {
    LOG.println(TAG " OOM combined packed buffer -- aborting");
    return false;
  }

  uint8_t* panePacked = (uint8_t*)ps_malloc(PANE_PACKED_BYTES);
  if (!panePacked) panePacked = (uint8_t*)malloc(PANE_PACKED_BYTES);
  if (!panePacked) {
    LOG.println(TAG " OOM NEW pane packed buffer -- aborting");
    free(combined);
    return false;
  }
  const bool ok = dither_new_pane(src, panePacked);
  if (ok) {
    blit_pane_packed(combined, panePacked, FULL_PANE_Y0);
  }
  free(panePacked);
  if (!ok) { free(combined); return false; }

  const bool pushed = push_full_panel(combined, "FULL NEW");
  free(combined);
  if (!pushed) return false;

  LOG.println(TAG " === FULL NEW (vertically centered pane) ===");
  LOG.println(TAG " Serial commands: a=Full NEW  b=Full LEGACY  c=Split comparison");
  return true;
}

// ----- full-screen: LEGACY library only --------------------------------------
static bool show_full_legacy(const RgbImage* src) {
  LOG.printf("[layout] pane %dx%d centered at y=%d  (panel %dx%d, LEGACY)\n",
             PANE_WIDTH, PANE_HEIGHT, FULL_PANE_Y0, EPD_WIDTH, EPD_HEIGHT);

  uint8_t* combined = alloc_full_packed();
  if (!combined) {
    LOG.println(TAG " OOM combined packed buffer -- aborting");
    return false;
  }

  uint8_t* idx = (uint8_t*)ps_malloc(PANE_IDX_BYTES);
  if (!idx) idx = (uint8_t*)malloc(PANE_IDX_BYTES);
  if (!idx) {
    LOG.println(TAG " OOM legacy index buffer -- aborting");
    free(combined);
    return false;
  }
  const bool ok = dither_legacy_pane(src, idx);
  if (ok) {
    blit_pane_idx(combined, idx, FULL_PANE_Y0);
  }
  free(idx);
  if (!ok) { free(combined); return false; }

  const bool pushed = push_full_panel(combined, "FULL LEGACY");
  free(combined);
  if (!pushed) return false;

  LOG.println(TAG " === FULL LEGACY (vertically centered pane) ===");
  LOG.println(TAG " Serial commands: a=Full NEW  b=Full LEGACY  c=Split comparison");
  return true;
}

void setup() {
  LOG.begin(115200, SERIAL_8N1, PIN_DBG_RX, PIN_DBG_TX);
  delay(2500);
  LOG.println();
  LOG.println("==============================================");
  LOG.println("  reTerminal E1004 -- Dither Comparison");
  LOG.println("  TOP = NEW library  |  BOTTOM = LEGACY");
  LOG.println("==============================================");
  LOG.printf("[sys] chip      : ESP32-S3 @ %lu MHz\n", (unsigned long)ESP.getCpuFreqMHz());
  LOG.printf("[sys] PSRAM size: %lu kB\n", (unsigned long)(ESP.getPsramSize() / 1024));
  if (ESP.getPsramSize() == 0) {
    LOG.println("[sys] !!! PSRAM is 0 kB -- enable Tools > PSRAM > OPI PSRAM in the IDE !!!");
  }
  LOG.printf("[sys] panel     : %d x %d (portrait)\n", EPD_WIDTH, EPD_HEIGHT);
  LOG.printf("[sys] pane      : %d x %d  (split rows 0..%d / %d..%d)\n",
             PANE_WIDTH, PANE_HEIGHT, PANE_HEIGHT - 1,
             PANE_HEIGHT + DIVIDER_HEIGHT, EPD_HEIGHT - 1);
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

  LOG.println(TAG " SD.begin (shares HSPI with EPaper) ...");
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

  // Immediately resize the stored image to ONE PANE.  Storing a full-panel
  // RGB copy (1200x1600x3 = 5.76 MB) would not leave enough PSRAM for the
  // comparison buffers; a pane-sized copy (1200x799x3 = 2.87 MB) does.
  if (g_originalImage.width != PANE_WIDTH || g_originalImage.height != PANE_HEIGHT) {
    LOG.printf(TAG " resizing stored image %dx%d -> %dx%d (pane size)\n",
               g_originalImage.width, g_originalImage.height, PANE_WIDTH, PANE_HEIGHT);
    if (!resize_image(&g_originalImage, PANE_WIDTH, PANE_HEIGHT)) {
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
  LOG.println(TAG "   a = Full-screen NEW library (centered pane)");
  LOG.println(TAG "   b = Full-screen LEGACY library (centered pane)");
  LOG.println(TAG "   c = Split comparison TOP/BOTTOM (default)");
  LOG.println(TAG "   ? = Print all parameters");
  LOG.println(TAG " ========================================");
  if (show_comparison(&g_originalImage)) LOG.println(TAG " comparison displayed OK");
  else                                   LOG.println(TAG " comparison failed");

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
