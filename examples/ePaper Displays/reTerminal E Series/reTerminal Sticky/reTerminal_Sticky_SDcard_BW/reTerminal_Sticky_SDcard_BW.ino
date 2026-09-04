/*
 * reTerminal Sticky -- SD card image to monochrome e-paper (800x480, 1bpp).
 * Hardware: reTerminal Sticky (XIAO ESP32-S3 + 3.97" 800x480 monochrome
 * e-paper). Production mixes SSD1677 and SSD2677 controllers, one per
 * unit, identical glass and wiring:
 *
 *           Function     ESP32-S3 GPIO
 *           SPI SCK      13  (shared with SD)
 *           SPI MOSI     14  (shared with SD)
 *           SPI MISO     12  (shared with SD; panel read-back line)
 *           EPD CS       15
 *           EPD DC       16
 *           EPD RST      17
 *           EPD BUSY     18
 *           EPD EN       47  (panel power enable, active high)
 *           SD  CS       8
 *
 * The Sticky microSD slot has no SD_EN / SD_DET wiring -- do not drive
 * GPIO15/16 here, those are the display CS/DC pins.
 *
 * Controller selection: this sketch first takes the catalog path
 * (Seeed_Product::reTerminal_Sticky), which runs the library's two-stage
 * auto-detect: the firmware-style read probe (reset -> 0x70 -> read one
 * byte -> 0x07 = SSD2677), and, when the read-back line does not answer
 * (0x00/0xFF), a BUSY-polarity probe after reset (ready HIGH = SSD2677,
 * ready LOW = SSD1677). If the chosen driver still fails to init, the
 * sketch retries with the direct SSD2677 configuration as belt-and-braces
 * (bypasses detection entirely; a wrong first pick costs one BUSY timeout
 * of about 30 s).
 *
 * Diagnostic side effect: the SD slot shares MISO=GPIO12 with the panel
 * SDO. If ProbeResult/ProbePhase2 read all 0x00 from the panel but this
 * sketch loads card data fine, the ESP32 -> GPIO12 read path is proven
 * good and the panel SDO is simply not wired on that unit.
 *
 * Serial: USB CDC. Board: Tools > Board > ESP32 Arduino > XIAO_ESP32S3,
 * PSRAM: OPI PSRAM (the RGB888 decode needs more than internal SRAM).
 */

#include <SPI.h>
#include <FS.h>
#include <SD.h>
#include <Seeed_GFX.h>
#include "bus/Bus_SPI.h"

// Direct-begin fallback for SSD2677 units where the catalog probe fails.
#include "board/boards/reTerminal_ePaper_Boards.h"
#include "panel/configs/Seeed_Panel_Configs.h"
#include "driver/epaper/Driver_SSD2677.h"
#include "panel/Panel_EPaper.h"

#include <dither/Dither.h>
#include "image_loader.h"

Seeed_GFX display(Seeed_Product::reTerminal_Sticky);
static constexpr int EPD_WIDTH  = 800;
static constexpr int EPD_HEIGHT = 480;

// ----- pins ------------------------------------------------------------------
// Sticky microSD: CS=GPIO8 on the bus shared with the panel (SCK=13,
// MOSI=14, MISO=12). No SD_EN / SD_DET pins exist on this board.
static constexpr int PIN_SD_CS = 8;
#define LOG Serial
#define TAG "[sticky-bw]"

// USER CONFIGURATION -- edit the constants below.

// ----- image source ----------------------------------------------------------
// Path to the image on the SD card (must start with '/').
// Supported formats:
//   .jpg / .jpeg -- baseline JFIF (8-bit, YCbCr or grayscale)
//   .bmp         -- 24-bit BGR uncompressed, or 4-bit indexed (BI_RGB)
//   .png         -- common PNG color types (decoded via bundled pngle;
//                    RGBA images are alpha-composited over white)
// The actual format is sniffed from magic bytes -- a misleading extension is
// auto-corrected and a warning is printed. If the path is missing, the first
// supported image in /img or the card root is used instead.
static const char* IMAGE_PATH = "/img/demo.jpg";

// ----- dither ----------------------------------------------------------------
// Dithering algorithm (same options as the E6 dither library):
//   DITHER_NONE     -- nearest-color, no dithering (fastest, blocky)
//   DITHER_BAYER8   -- 8x8 ordered Bayer (no error buffer; safest on big panels)
//   DITHER_FS       -- Floyd-Steinberg (best quality/speed balance, default)
//   DITHER_ATKINSON -- Atkinson (high contrast, classic Mac look)
//   DITHER_BURKES   -- Burkes (smooth, two-row error buffer)
//   DITHER_SIERRA3  -- Full Sierra (smoothest, three-row error buffer)
static const DitherMethod DITHER_METHOD = DITHER_FS;

// Brightness gamma. 1.0 = neutral, >1.0 darkens, <1.0 brightens. Typical 0.8 - 1.6.
static const float DITHER_GAMMA = 1.0f;

// Swap black and white in the output (false = normal).
static const bool DITHER_INVERT = false;

// ----- diagnostics helpers ----------------------------------------------------
static void log_mem(const char* tag) {
  LOG.printf("[mem] %-22s heap=%lu kB  PSRAM free=%lu/%lu kB\n", tag,
             (unsigned long)(ESP.getFreeHeap() / 1024),
             (unsigned long)(ESP.getFreePsram() / 1024),
             (unsigned long)(ESP.getPsramSize() / 1024));
  LOG.flush();
}

// Post-begin failures are also drawn onto the panel so the unit does not
// look "dead" without a serial monitor attached. e-paper keeps the message
// without power -- press RESET to retry.
static void fail_on_panel(const char* title, const char* hint1,
                          const char* hint2 = nullptr) {
  display.fillScreen(TFT_WHITE);
  display.setTextColor(TFT_BLACK);
  display.setTextDatum(TL_DATUM);
  display.setTextSize(4);
  display.drawString(title, 24, 56);
  display.setTextSize(2);
  int y = 140;
  if (hint1) {
    display.drawString(hint1, 24, y);
    y += 48;
  }
  if (hint2) {
    display.drawString(hint2, 24, y);
    y += 48;
  }
  display.setTextSize(1);
  display.drawString("Serial Monitor @ 115200 has the full log.", 24, y + 24);
  display.refresh();
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

// ----- main pipeline ----------------------------------------------------------
static bool show_image_on_panel(RgbImage* img) {
  // The panel and the pipeline target are both 800x480: force fullscreen.
  int W = EPD_WIDTH;
  int H = EPD_HEIGHT;
  int x = 0, y = 0;
  LOG.printf("[layout] src=%dx%d -> fullscreen %dx%d\n",
             img->width, img->height, W, H);

  if (W != img->width || H != img->height) {
    const size_t need_kb = (size_t)W * H * 3 / 1024;
    LOG.printf("[layout] resizing %dx%d -> %dx%d  (needs +%lu kB temporarily)\n",
               img->width, img->height, W, H, (unsigned long)need_kb);
    log_mem("before resize");
    if (!resize_image(img, W, H)) {
      LOG.println("[layout] resize failed (likely OOM) -- aborting");
      LOG.println("[layout]   pre-shrink the image on the PC to 800x480");
      return false;
    }
    log_mem("after resize");
  }

  const size_t npx = (size_t)W * H;

  log_mem("before idx malloc");
  LOG.printf(TAG " allocating index buf: %lu kB\n", (unsigned long)(npx / 1024));
  uint8_t* idx = (uint8_t*)ps_malloc(npx);
  if (!idx) idx = (uint8_t*)malloc(npx);
  if (!idx) { LOG.println(TAG " OOM idx -- aborting"); return false; }

  LOG.printf(TAG " dithering BW with %s, gamma=%.2f ...\n",
             dither_method_name(DITHER_METHOD), DITHER_GAMMA);
  const uint32_t t0 = millis();
  if (!dither_image(img->pixels, W, H, PAL_BW, DITHER_METHOD,
                    DITHER_GAMMA, DITHER_INVERT, idx)) {
    LOG.println(TAG " dither failed -- aborting");
    free(idx); return false;
  }
  LOG.printf(TAG " dither done in %lu ms\n", (unsigned long)(millis() - t0));

  LOG.println(TAG " freeing RGB888 source");
  image_free(img);
  log_mem("after RGB888 freed");

  const size_t bm_bytes = ((size_t)W + 7) / 8 * (size_t)H;
  uint8_t* bm = (uint8_t*)ps_malloc(bm_bytes);
  if (!bm) bm = (uint8_t*)malloc(bm_bytes);
  if (!bm) { free(idx); LOG.println(TAG " OOM bm -- aborting"); return false; }
  // Pack black-as-bit=1 so we can call drawBitmap(fg=BLACK, bg=WHITE).
  pack_1bpp_msb(idx, bm, W, H, /*bit_for_black=*/true);
  free(idx);

  LOG.printf(TAG " drawBitmap %dx%d at (%d,%d) -> framebuffer\n", W, H, x, y);
  display.drawBitmap(x, y, bm, W, H, TFT_BLACK, TFT_WHITE);
  LOG.println(TAG " update() -- panel refresh starts");
  LOG.flush();
  const uint32_t t1 = millis();
  const GfxResult refreshResult = display.refresh();
  LOG.printf(TAG " update done in %lu ms\n", (unsigned long)(millis() - t1));
  free(bm);
  if (!refreshResult) {
    LOG.printf(TAG " refresh failed: %s\n", refreshResult.message);
    return false;
  }
  return true;
}

void setup() {
  LOG.begin(115200);
  delay(2000);  // let USB CDC enumerate so the early prints are not lost
  LOG.println();
  LOG.println("==============================================");
  LOG.println("  reTerminal Sticky -- SD Image (BW 1bpp)");
  LOG.println("==============================================");
  LOG.printf("[sys] chip      : ESP32-S3 @ %lu MHz\n", (unsigned long)ESP.getCpuFreqMHz());
  LOG.printf("[sys] PSRAM size: %lu kB\n", (unsigned long)(ESP.getPsramSize() / 1024));
  if (ESP.getPsramSize() == 0) {
    LOG.println("[sys] !!! PSRAM is 0 kB -- enable Tools > PSRAM > OPI PSRAM in the IDE !!!");
  }
  LOG.printf("[sys] panel     : %d x %d (EPD_WIDTH x EPD_HEIGHT)\n", EPD_WIDTH, EPD_HEIGHT);
  LOG.printf("[sys] image     : '%s'\n", IMAGE_PATH);
  LOG.flush();

  LOG.println(TAG " display.begin() (catalog path, auto-detect probe) ...");
  bool ok = display.begin();
  if (!ok) {
    LOG.printf(TAG " auto path failed: %s\n", display.lastResult().message);
    LOG.println(TAG " retrying with the direct SSD2677 configuration ...");
    LOG.println(TAG " (both auto-detect stages failed; if the ~30 s BUSY");
    LOG.println(TAG "  timeout appeared above, the wrong driver was tried first)");
    LOG.flush();
    ok = display.begin<Board_reTerminal_Sticky, Config_reTerminal_Sticky_SSD2677>();
    if (!ok) {
      LOG.printf(TAG " direct SSD2677 begin failed: %s\n",
                 display.lastResult().message);
      return;
    }
    LOG.println(TAG " direct SSD2677 begin OK");
  } else {
    LOG.printf(TAG " begin OK, driver: %s\n", display.driverPtr()->name());
  }
  display.fillScreen(TFT_WHITE);
  log_mem("after display.begin");

  // The library board config starts the display and SD slot on the same
  // SPI host with MISO=GPIO12 already routed. Do not end()/begin() that
  // bus: doing so invalidates the display driver's SPI ownership.
  Bus_SPI& displayBus =
      static_cast<Bus_SPI&>(display.panel().driver().bus());
  SPIClass* spi = displayBus.spiInstance();
  if (!spi) {
    LOG.println(TAG " shared SPI instance unavailable -- aborting");
    fail_on_panel("SPI ERROR", "shared SPI instance unavailable");
    return;
  }

  LOG.println(TAG " SD.begin (shares the SPI bus with the panel) ...");
  if (!SD.begin(PIN_SD_CS, *spi)) {
    LOG.println(TAG " SD.begin FAILED -- aborting");
    LOG.println(TAG "   - Is the card inserted and formatted as FAT/FAT32?");
    LOG.println(TAG "   - CS is GPIO8; SCK/MOSI/MISO are shared with the panel");
    fail_on_panel("SD CARD FAILED",
                  "insert a FAT32 card, then press RESET",
                  "cards >32 GB are usually exFAT and will NOT mount");
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
    fail_on_panel("NO IMAGE FOUND",
                  "copy a JPG/BMP/PNG to /img or card root",
                  "paths and file names are case-sensitive");
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
  RgbImage img;
  if (!load_image_from_sd(imagePath.c_str(), /*target_w=*/0, /*target_h=*/0, &img)) {
    LOG.println(TAG " load failed -- aborting");
    LOG.println(TAG "   common issues:");
    LOG.println(TAG "    - source resolution too large (decodes to > 8 MB RGB888)");
    LOG.println(TAG "    - JPEG: non-baseline (progressive / 4:1:1 / CMYK not supported)");
    LOG.println(TAG "    - PNG : source must fit RGB888 memory before layout scaling");
    fail_on_panel("IMAGE DECODE FAILED",
                  "use a baseline JPG / 24-bit BMP / common PNG",
                  "pre-shrink large images to about 800x480 on the PC");
    return;
  }
  LOG.printf(TAG " image decoded: %dx%d  (%lu kB in PSRAM)\n",
             img.width, img.height,
             (unsigned long)((size_t)img.width * img.height * 3 / 1024));
  log_mem("after image decoded");

  if (show_image_on_panel(&img)) LOG.println(TAG " frame pushed OK");
  else {
    LOG.println(TAG " show_image_on_panel failed");
    fail_on_panel("PANEL DRAW FAILED", "see Serial Monitor for the failing stage");
  }
  image_free(&img);

  LOG.println(TAG " done. Panel is asleep after refresh.");
  LOG.println("==============================================");
}

void loop() {
  // The e-paper holds the image without power. Press RESET to re-display.
  delay(1000);
}
