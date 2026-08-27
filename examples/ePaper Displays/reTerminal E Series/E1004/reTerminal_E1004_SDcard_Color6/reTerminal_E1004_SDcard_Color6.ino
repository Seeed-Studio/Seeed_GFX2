/*
 * reTerminal E1004 -- SD card image to 6-color e-paper (1200x1600, T133A01).
 *
 * Pipeline mirrors online_img2bitmap_.html with screen=e6:
 *   SD JPG/BMP/PNG -> RGB888 -> E6 dither (full DitherConfig parameter set)
 *               -> packed 4bpp -> pushImage4BPP into the T133A01 frame buffer.
 *
 * 6-color palette (matches the HTML tool):
 *   WHITE  GREEN  RED  YELLOW  BLUE  BLACK
 *
 * MEMORY BUDGET (1200x1600 panel + XIAO ESP32-S3 8 MB PSRAM):
 *   - 4bpp frame buffer:    1200*1600 / 2 = 960 KB
 *   - Source RGB888 decode: width*height*3 bytes  (panel size = 5.76 MB)
 *   - Direct 4bpp output:   ceil(width/2)*height  (panel size = 960 KB)
 *
 * Direct output avoids the former 1-byte-per-pixel quantization index. The unified
 * dither module keeps only 2 or 3 error rows (FS at panel width: about 28.8 KB),
 * and reports allocation failure instead of silently changing algorithms.
 * Large decoder allocations can still exhaust PSRAM, so use a scaled source or a
 * preprocessed panel-format image when necessary.
 *
 * Hardware: reTerminal E1004 (XIAO ESP32-S3 + 13.3" T133A01 6-color e-paper).
 *
 *           Function     ESP32-S3 GPIO
 *           SPI SCK      7   (shared with SD)
 *           SPI MISO     8   (shared with SD)
 *           SPI MOSI     9   (shared with SD)
 *           EPD CS       10
 *           EPD CS1      2
 *           EPD DC       11
 *           EPD BUSY     13
 *           EPD RST      38
 *           EPD ENABLE   12
 *           SD  CS       14
 *           SD  EN       16
 *           SD  DET      15
 *
 * Logging goes out on UART1 (GPIO43 TX, GPIO44 RX) -- the on-board USB-to-UART
 * bridge of the reTerminal carrier board is wired there. The default Arduino
 * `Serial` (USB CDC) only works when "USB CDC On Boot" is enabled, which is
 * unreliable for diagnostics on reTerminal.
 */

#include <SPI.h>
#include <FS.h>
#include <SD.h>
#include <Seeed_GFX.h>
#include "bus/Bus_SPI.h"

#include <dither/Dither.h>
#include "image_loader.h"

Seeed_GFX display(Seeed_Product::RETERMINAL_E1004);
static DitherContext ditherContext;
static constexpr int EPD_WIDTH  = 1200;
static constexpr int EPD_HEIGHT = 1600;

// ----- pins ------------------------------------------------------------------
// SD pins are identical across all reTerminal E10xx carrier boards, except
// SD_EN: E1001/E1002/E1004 use GPIO16, E1003 uses GPIO39.
static constexpr int    PIN_SD_SCK   = 7;   // shared with e-paper SPI bus
static constexpr int    PIN_SD_MISO  = 8;
static constexpr int    PIN_SD_MOSI  = 9;   // shared with e-paper SPI bus
static constexpr int    PIN_SD_CS    = 14;
static constexpr int    PIN_SD_EN    = 16;
static constexpr int    PIN_SD_DET   = 15;
static constexpr int    PIN_DBG_RX   = 44;
static constexpr int    PIN_DBG_TX   = 43;
#define LOG       Serial1
#define TAG       "[e1004]"

// USER CONFIGURATION -- edit the constants below. Each option lists all the
// values you can pick from right above it.

// ----- image source ----------------------------------------------------------
// Path to the image on the SD card (must start with '/').
// Supported formats:
//   .jpg / .jpeg -- baseline JFIF (8-bit, YCbCr or grayscale)
//   .bmp         -- 24-bit BGR uncompressed, or 4-bit indexed (BI_RGB)
//   .png         -- common PNG color types (decoded via bundled pngle;
//                    RGBA images are alpha-composited over white).
// The actual format is sniffed from magic bytes -- a misleading extension is
// auto-corrected and a warning is printed.
static const char* IMAGE_PATH = "/img/demo.jpg";

// ----- dither ----------------------------------------------------------------
// The full DitherConfig parameter set. Every initial value is the library
// default from src/dither/Dither.h, EXCEPT the two marked "E1004 baseline"
// below -- deliberate picks for the 13.3" panel kept from the previous
// hardcoded version, so the rendered output is unchanged by this upgrade.
// Edit and re-flash to tune.
// (reTerminal_E1004_SDcard_Color6_DitherCompare offers the same parameter set
// with live serial tuning: g/s/d/t/e/w/p/l/m/k.)

// Dithering algorithm. Same options as online_img2bitmap_.html. Pick one:
//   DITHER_NONE     -- nearest-color, no dithering (fastest, blocky)
//   DITHER_BAYER8   -- 8x8 ordered Bayer (no error buffer; safest on big panels)
//   DITHER_FS       -- Floyd-Steinberg (best quality/speed balance)
//   DITHER_ATKINSON -- Atkinson (high contrast, classic Mac look)
//   DITHER_BURKES   -- Burkes (smooth, two-row error buffer)
//   DITHER_SIERRA3  -- Full Sierra (smoothest, three-row error buffer)
//   DITHER_PALETTE_MIX -- two-color mixing for out-of-gamut colors
// BAYER8 is the E1004 default for predictable speed on the large panel.
// Direct 4bpp output also makes diffusion practical for more source sizes,
// subject to decoder PSRAM usage.
static const DitherMethod DITHER_METHOD = DITHER_BAYER8;

// Brightness gamma. x' = pow(x, 1/gamma): >1.0 brightens, <1.0 darkens.
// 1.0 = neutral.
static const float DITHER_GAMMA = 1.0f;

// Serpentine (snake) scan: reduces directional streaks in error diffusion.
// E1004 baseline: true (library default is false).
static const bool DITHER_SERPENTINE = true;

// Error-buffer clamp: true = narrow [0,255] clamp (library default, cleaner
// pure-color boundaries); false = wide [-255,510] (often better for photos).
// E1004 baseline: false (wide clamp, photo-friendly on the large panel).
static const bool DITHER_LEGACY_CLAMP = false;

// Saturation boost [0, 1]: compensates for the reduced palette. 0 = off.
static const float DITHER_SAT_BOOST = 0.0f;

// Darkness bias [0, 0.5]: darkens before dithering; compensates bright panels.
// 0 = off.
static const float DITHER_DARKNESS_BIAS = 0.0f;

// Contrast: 1.0 = no change; >1 increases, <1 reduces.
static const float DITHER_CONTRAST = 1.0f;

// Error diffusion strength [0, 1]: 0 = nearest-color only, 1 = full diffusion.
static const float DITHER_DIFF_STRENGTH = 1.0f;

// Warmth [-1, 1]: positive = warmer (more red), negative = cooler (more blue).
static const float DITHER_WARMTH = 0.0f;

// Color distance metric. Pick one:
//   METRIC_RGB     -- plain RGB distance (default)
//   METRIC_REDMEAN -- perceptual redmean; deprecated for E6 (collapses cyan
//                     toward green), kept for experiments only
static const ColorMetric DITHER_COLOR_METRIC = METRIC_RGB;

// ----- calibrated palette (optional) ------------------------------------------
// The library's built-in E6 palette (src/dither/Palettes.h) is already
// calibrated. Only if you measured YOUR panel with a colorimeter, fill in the
// RGB values below and set USE_CALIBRATED_PALETTE to 1 to override it.
#define USE_CALIBRATED_PALETTE 0
static const Rgb kCalibratedE6_Rgb[6] = {
  {255, 255, 255},   // WHITE  (code 0x0) -- replace with measured
  { 29, 185,  84},   // GREEN  (code 0x2) -- replace with measured
  {229,  57,  53},   // RED    (code 0x6) -- replace with measured
  {255, 216,   0},   // YELLOW (code 0xB) -- replace with measured
  {  0,  76, 255},   // BLUE   (code 0xD) -- replace with measured
  {  0,   0,   0},   // BLACK  (code 0xF) -- replace with measured
};
static const uint8_t kCalibratedE6_Code[6] = {0x0, 0x2, 0x6, 0xB, 0xD, 0xF};

// ----- layout: position -----------------------------------------------------
// Where on the panel the image snaps to. Pick one of these 9 anchors in a
// 3x3 grid:
//   ANCHOR_TOP_LEFT       ANCHOR_TOP_CENTER       ANCHOR_TOP_RIGHT
//   ANCHOR_MIDDLE_LEFT    ANCHOR_CENTER           ANCHOR_MIDDLE_RIGHT
//   ANCHOR_BOTTOM_LEFT    ANCHOR_BOTTOM_CENTER    ANCHOR_BOTTOM_RIGHT
enum DisplayAnchor {
  ANCHOR_TOP_LEFT,      ANCHOR_TOP_CENTER,      ANCHOR_TOP_RIGHT,
  ANCHOR_MIDDLE_LEFT,   ANCHOR_CENTER,          ANCHOR_MIDDLE_RIGHT,
  ANCHOR_BOTTOM_LEFT,   ANCHOR_BOTTOM_CENTER,   ANCHOR_BOTTOM_RIGHT,
};
static const DisplayAnchor DISPLAY_ANCHOR = ANCHOR_CENTER;

// ----- layout: size ---------------------------------------------------------
// How the image is sized relative to the panel. Pick one:
//   FIT_ORIGINAL -- keep source size as-is (no resize; check PSRAM budget)
//   FIT_CONTAIN  -- shrink to fit fully inside the panel; NEVER upscales
//                   (behaves like FIT_ORIGINAL if source is already smaller)
//   FIT_SCALE    -- multiply source size by DISPLAY_SCALE below
enum DisplayFit { FIT_ORIGINAL, FIT_CONTAIN, FIT_SCALE };
static const DisplayFit DISPLAY_FIT = FIT_ORIGINAL;

// Scale factor -- only used when DISPLAY_FIT == FIT_SCALE. Examples:
//   0.25 -- quarter size       0.5  -- half size
//   1.0  -- original            2.0  -- 2x zoom (>1.0 may OOM on this panel)
static const float DISPLAY_SCALE = 1.0f;

// ----- diagnostics helpers ----------------------------------------------------
static void log_mem(const char* tag) {
  LOG.printf("[mem] %-22s heap=%lu kB  PSRAM free=%lu/%lu kB\n", tag,
             (unsigned long)(ESP.getFreeHeap() / 1024),
             (unsigned long)(ESP.getFreePsram() / 1024),
             (unsigned long)(ESP.getPsramSize() / 1024));
  LOG.flush();
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

// ----- layout helpers ---------------------------------------------------------
static void compute_target_size(int src_w, int src_h, DisplayFit fit, float scale,
                                int panel_w, int panel_h,
                                int* out_w, int* out_h) {
  switch (fit) {
    case FIT_ORIGINAL:
      *out_w = src_w; *out_h = src_h;
      break;
    case FIT_CONTAIN: {
      double sx = (double)panel_w / src_w;
      double sy = (double)panel_h / src_h;
      double s  = (sx < sy) ? sx : sy;
      if (s > 1.0) s = 1.0;
      *out_w = (int)(src_w * s + 0.5);
      *out_h = (int)(src_h * s + 0.5);
      break;
    }
    case FIT_SCALE:
      *out_w = (int)(src_w * scale + 0.5);
      *out_h = (int)(src_h * scale + 0.5);
      break;
  }
  if (*out_w < 1) *out_w = 1;
  if (*out_h < 1) *out_h = 1;
}

static void compute_anchor_xy(int img_w, int img_h, DisplayAnchor a,
                              int panel_w, int panel_h,
                              int* out_x, int* out_y) {
  int x = 0, y = 0;
  switch (a) {
    case ANCHOR_TOP_LEFT:      x = 0;                      y = 0;                       break;
    case ANCHOR_TOP_CENTER:    x = (panel_w - img_w) / 2;  y = 0;                       break;
    case ANCHOR_TOP_RIGHT:     x = panel_w - img_w;        y = 0;                       break;
    case ANCHOR_MIDDLE_LEFT:   x = 0;                      y = (panel_h - img_h) / 2;   break;
    case ANCHOR_CENTER:        x = (panel_w - img_w) / 2;  y = (panel_h - img_h) / 2;   break;
    case ANCHOR_MIDDLE_RIGHT:  x = panel_w - img_w;        y = (panel_h - img_h) / 2;   break;
    case ANCHOR_BOTTOM_LEFT:   x = 0;                      y = panel_h - img_h;         break;
    case ANCHOR_BOTTOM_CENTER: x = (panel_w - img_w) / 2;  y = panel_h - img_h;         break;
    case ANCHOR_BOTTOM_RIGHT:  x = panel_w - img_w;        y = panel_h - img_h;         break;
  }
  *out_x = x; *out_y = y;
}

static const char* fit_name(DisplayFit f) {
  switch (f) {
    case FIT_ORIGINAL: return "ORIGINAL";
    case FIT_CONTAIN:  return "CONTAIN";
    case FIT_SCALE:    return "SCALE";
  }
  return "?";
}
static const char* anchor_name(DisplayAnchor a) {
  switch (a) {
    case ANCHOR_TOP_LEFT:      return "TOP_LEFT";
    case ANCHOR_TOP_CENTER:    return "TOP_CENTER";
    case ANCHOR_TOP_RIGHT:     return "TOP_RIGHT";
    case ANCHOR_MIDDLE_LEFT:   return "MIDDLE_LEFT";
    case ANCHOR_CENTER:        return "CENTER";
    case ANCHOR_MIDDLE_RIGHT:  return "MIDDLE_RIGHT";
    case ANCHOR_BOTTOM_LEFT:   return "BOTTOM_LEFT";
    case ANCHOR_BOTTOM_CENTER: return "BOTTOM_CENTER";
    case ANCHOR_BOTTOM_RIGHT:  return "BOTTOM_RIGHT";
  }
  return "?";
}

enum StreamBmpResult {
  STREAM_BMP_NOT_NEEDED = 0,
  STREAM_BMP_OK = 1,
  STREAM_BMP_FAILED = -1
};

static uint32_t read_le32(const uint8_t* p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
         ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static size_t sd_file_read_at(void* context, uint32_t offset,
                              void* destination, size_t length) {
  File* file = static_cast<File*>(context);
  if (!file || !*file || !file->seek(offset)) return 0;
  return file->read(static_cast<uint8_t*>(destination), length);
}

// A full-panel buffered RGB888 decode can still approach the PSRAM limit once
// display/output/scratch storage is included. For an unscaled BMP, use
// Seeed_GFX2's row decoder so only one source row is buffered. JPEG/PNG still
// use the dither pipeline below. The threshold remains intentionally
// conservative and includes memory reserved for decoder/display overhead.
static StreamBmpResult stream_large_bmp_if_needed(const char* path) {
  File file = SD.open(path, FILE_READ);
  if (!file) return STREAM_BMP_FAILED;

  uint8_t header[26];
  if (file.read(header, sizeof(header)) != sizeof(header) ||
      header[0] != 'B' || header[1] != 'M') {
    file.close();
    return STREAM_BMP_NOT_NEEDED;
  }

  const int32_t sourceW = (int32_t)read_le32(header + 18);
  const int32_t signedH = (int32_t)read_le32(header + 22);
  if (sourceW <= 0 || signedH == 0 || signedH == INT32_MIN) {
    LOG.println("[bmp] invalid dimensions");
    file.close();
    return STREAM_BMP_FAILED;
  }
  const int32_t sourceH = signedH < 0 ? -signedH : signedH;
  const uint64_t bufferedNeed = (uint64_t)sourceW * sourceH * 4ULL;
  const uint64_t freePsram = ESP.getFreePsram();
  const uint64_t reserve = 128ULL * 1024ULL;
  if (bufferedNeed + reserve <= freePsram) {
    file.close();
    return STREAM_BMP_NOT_NEEDED;
  }

  int targetW, targetH;
  compute_target_size(sourceW, sourceH, DISPLAY_FIT, DISPLAY_SCALE,
                      EPD_WIDTH, EPD_HEIGHT, &targetW, &targetH);
  if (targetW != sourceW || targetH != sourceH) {
    LOG.printf("[bmp] buffered resize needs about %llu kB, but streaming cannot scale\n",
               (unsigned long long)(bufferedNeed / 1024ULL));
    LOG.println("[bmp] pre-resize the BMP on a PC or select FIT_ORIGINAL");
    file.close();
    return STREAM_BMP_FAILED;
  }

  int x, y;
  compute_anchor_xy(sourceW, sourceH, DISPLAY_ANCHOR,
                    EPD_WIDTH, EPD_HEIGHT, &x, &y);
  LOG.printf("[bmp] large %ldx%ld image: streaming rows at (%d,%d)\n",
             (long)sourceW, (long)sourceH, x, y);
  UiCallbackSource source((uint32_t)file.size(), sd_file_read_at, &file);
  const UiStatus status = display.drawImage(x, y, source, UiImageFormat::BMP);
  file.close();
  if (!uiOk(status)) {
    LOG.printf("[bmp] stream decode failed (UiStatus=%u)\n", (unsigned)status);
    return STREAM_BMP_FAILED;
  }

  LOG.println(TAG " update() -- panel refresh starts");
  LOG.flush();
  const GfxResult refreshResult = display.refresh();
  if (!refreshResult) {
    LOG.printf(TAG " refresh failed: %s\n", refreshResult.message);
    return STREAM_BMP_FAILED;
  }
  return STREAM_BMP_OK;
}

// ----- main pipeline ----------------------------------------------------------
static bool show_image_on_panel(RgbImage* img) {
  // Force full-screen: always resize to panel dimensions.
  int W = EPD_WIDTH;
  int H = EPD_HEIGHT;
  int x = 0, y = 0;
  LOG.printf("[layout] src=%dx%d -> fullscreen %dx%d\n",
             img->width, img->height, W, H);

  // 2) Resize if needed. resize_image() allocates a new PSRAM buffer for the
  //    target and then frees the old one, so peak memory briefly holds both.
  if (W != img->width || H != img->height) {
    const size_t need_kb = (size_t)W * H * 3 / 1024;
    LOG.printf("[layout] resizing %dx%d -> %dx%d  (needs +%lu kB temporarily)\n",
               img->width, img->height, W, H, (unsigned long)need_kb);
    log_mem("before resize");
    if (!resize_image(img, W, H)) {
      LOG.println("[layout] resize failed (likely OOM) -- aborting");
      LOG.println("[layout]   try FIT_ORIGINAL, or a smaller DISPLAY_SCALE,");
      LOG.println("[layout]   or pre-shrink the image on the PC");
      return false;
    }
    log_mem("after resize");
  }

  const size_t indexBytes = (size_t)W * H;
  const size_t packedBytes = ((size_t)W + 1u) / 2u * H;

  // 3) Dither directly into the panel's packed Spectra-6 format.
  log_mem("before output malloc");
  LOG.printf(TAG " allocating packed output: %lu kB (saves %lu kB)\n",
             (unsigned long)(packedBytes / 1024),
             (unsigned long)((indexBytes - packedBytes) / 1024));
  uint8_t* packed = (uint8_t*)ps_malloc(packedBytes);
  if (!packed) packed = (uint8_t*)malloc(packedBytes);
  if (!packed) { LOG.println(TAG " OOM packed output -- aborting"); return false; }

  DitherConfig cfg;
  cfg.method                 = DITHER_METHOD;
  cfg.palette                = PAL_E6;
  cfg.gamma                  = DITHER_GAMMA;
  cfg.serpentine             = DITHER_SERPENTINE;
  cfg.legacyClamp            = DITHER_LEGACY_CLAMP;
  cfg.saturationBoost        = DITHER_SAT_BOOST;
  cfg.darknessBias           = DITHER_DARKNESS_BIAS;
  cfg.contrast               = DITHER_CONTRAST;
  cfg.errorDiffusionStrength = DITHER_DIFF_STRENGTH;
  cfg.warmth                 = DITHER_WARMTH;
  cfg.colorMetric            = DITHER_COLOR_METRIC;
#if USE_CALIBRATED_PALETTE
  cfg.customPaletteRgb   = kCalibratedE6_Rgb;
  cfg.customPaletteCode  = kCalibratedE6_Code;
  cfg.customPaletteCount = 6;
#endif

  LOG.printf(TAG " dithering E6: %s g=%.2f serp=%d clamp=%d s=%.2f d=%.2f t=%.2f e=%.2f w=%.2f%s\n",
             dither_method_name(DITHER_METHOD), DITHER_GAMMA,
             DITHER_SERPENTINE ? 1 : 0, DITHER_LEGACY_CLAMP ? 1 : 0,
             DITHER_SAT_BOOST, DITHER_DARKNESS_BIAS, DITHER_CONTRAST,
             DITHER_DIFF_STRENGTH, DITHER_WARMTH,
             USE_CALIBRATED_PALETTE ? " customPal" : "");
  const uint32_t t0 = millis();
  if (!dither_image_4bpp(img->pixels, W, H, cfg, ditherContext, packed)) {
    LOG.println(TAG " dither failed -- aborting");
    free(packed); return false;
  }
  LOG.printf(TAG " dither done in %lu ms\n", (unsigned long)(millis() - t0));
  LOG.printf(TAG " dither work=%lu B context=%lu B output=%lu B\n",
             (unsigned long)dither_working_memory_bytes(W, PAL_E6,
                                                        DITHER_METHOD),
             (unsigned long)ditherContext.workingMemoryCapacity(),
             (unsigned long)packedBytes);

  // 4) Free the RGB888 source; the dither output is already packed 4bpp.
  LOG.println(TAG " freeing RGB888 source");
  image_free(img);
  log_mem("after RGB888 freed");

  // 5) Preserve the raw Spectra 6 palette nibbles; this is not RGB565.
  LOG.printf(TAG " pushImage4BPP %dx%d at (%d,%d) -> framebuffer\n",
             W, H, x, y);
  if (!display.pushImage4BPP(x, y, W, H, packed)) {
    LOG.println(TAG " packed 4bpp image rejected");
    free(packed);
    return false;
  }
  LOG.println(TAG " update() -- panel refresh starts");
  LOG.flush();
  const uint32_t t1 = millis();
  const GfxResult refreshResult = display.refresh();
  LOG.printf(TAG " update done in %lu ms\n", (unsigned long)(millis() - t1));
  free(packed);
  if (!refreshResult) {
    LOG.printf(TAG " refresh failed: %s\n", refreshResult.message);
    return false;
  }
  return true;
}

void setup() {
  LOG.begin(115200, SERIAL_8N1, PIN_DBG_RX, PIN_DBG_TX);
  delay(2500);
  LOG.println();
  LOG.println("==============================================");
  LOG.println("  reTerminal E1004 -- SD Bitmap (6-color)");
  LOG.println("==============================================");
  LOG.printf("[sys] chip      : ESP32-S3 @ %lu MHz\n", (unsigned long)ESP.getCpuFreqMHz());
  LOG.printf("[sys] PSRAM size: %lu kB\n", (unsigned long)(ESP.getPsramSize() / 1024));
  if (ESP.getPsramSize() == 0) {
    LOG.println("[sys] !!! PSRAM is 0 kB -- enable Tools > PSRAM > OPI PSRAM in the IDE !!!");
  }
  LOG.printf("[sys] panel     : %d x %d (EPD_WIDTH x EPD_HEIGHT)\n", EPD_WIDTH, EPD_HEIGHT);
  LOG.printf("[sys] image     : '%s'\n", IMAGE_PATH);
  LOG.println("[sys] tuning     : full DitherConfig constants (E1004 baseline: BAYER8, serpentine, wide clamp)");
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
  // Product mode already configures the 4bpp Spectra 6 buffer to white.
  log_mem("after display.begin");

  Bus_SPI& displayBus =
      static_cast<Bus_SPI&>(display.panel().driver().bus());
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
    LOG.println(TAG "   - paths and file names are case-sensitive");
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

  const StreamBmpResult streamed = stream_large_bmp_if_needed(imagePath.c_str());
  if (streamed == STREAM_BMP_OK) {
    LOG.println(TAG " large BMP streamed and displayed OK");
    return;
  }
  if (streamed == STREAM_BMP_FAILED) {
    LOG.println(TAG " large BMP streaming failed -- aborting");
    return;
  }

  log_mem("before image load");
  RgbImage img;
  if (!load_image_from_sd(imagePath.c_str(), /*target_w=*/0, /*target_h=*/0, &img)) {
    LOG.println(TAG " load failed -- aborting");
    LOG.println(TAG "   common issues:");
    LOG.println(TAG "    - source resolution too large (decodes to > 8 MB RGB888)");
    LOG.println(TAG "    - JPEG: non-baseline (progressive / 4:1:1 / CMYK not supported)");
    LOG.println(TAG "    - PNG : source must fit RGB888 memory before layout scaling");
    return;
  }
  LOG.printf(TAG " image decoded: %dx%d  (%lu kB in PSRAM)\n",
             img.width, img.height,
             (unsigned long)((size_t)img.width * img.height * 3 / 1024));
  log_mem("after image decoded");

  if (show_image_on_panel(&img)) LOG.println(TAG " frame pushed OK");
  else                           LOG.println(TAG " show_image_on_panel failed");
  image_free(&img);

  LOG.println(TAG " done. Panel is asleep after refresh.");
  LOG.println("==============================================");
}

void loop() {
  // The e-paper holds the image without power. Press RESET to re-display.
  delay(1000);
}
