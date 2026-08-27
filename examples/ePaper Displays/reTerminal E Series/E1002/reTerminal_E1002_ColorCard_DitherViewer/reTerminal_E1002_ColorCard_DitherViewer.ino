/*
 * reTerminal E1002 -- Full-screen color-card dither viewer.
 *
 * Purpose: cycle through the 7 kept dither algorithms on the SAME reference
 * color card, one at a time, full-screen.  This lets a user SEE each
 * algorithm's character (color bias, texture, smoothness) without the
 * downscaling of a grid view.
 *
 * The selected method still applies to the whole image.  This viewer helps
 * users choose the global method that best preserves the colors important to
 * their content; it does not switch algorithms independently for each color.
 *
 * The sketch automatically advances every 8 seconds.  Watch the panel and
 * read the serial legend to map what you see to an algorithm name.
 *
 *   1. NONE        -- nearest-color (no dither)
 *   2. BAYER8      -- 8x8 ordered Bayer
 *   3. FS          -- Floyd-Steinberg error diffusion
 *   4. ATKINSON    -- Atkinson error diffusion
 *   5. BURKES      -- Burkes error diffusion
 *   6. SIERRA3     -- Full Sierra / Sierra-3 error diffusion
 *   7. PALETTE_MIX  -- Two-color palette-mix ordered dither
 *
 * Hardware: reTerminal E1002 (XIAO ESP32-S3 + 7.5" ED2208 6-color e-paper).
 *           Logging on UART1 (GPIO43 TX, GPIO44 RX), 115200 baud.
 *
 * Image:  colorcard_e6_custom.h (synthesized 800x480 E6 reference card).
 */

#include <SPI.h>
#include <Seeed_GFX.h>
#include <dither/Dither.h>

#include "colorcard_e6_custom.h"

// ===== USER CONFIGURATION ===================================================
// Time to display each algorithm before advancing (ms).
static constexpr uint32_t HOLD_MS = 8000;

// If true, advance to the next algorithm automatically every HOLD_MS.
// If false, stay on the first method and print how to change it.
static constexpr bool AUTO_CYCLE = true;

// =============================================================================

Seeed_GFX display(Seeed_Product::RETERMINAL_E1002);
static DitherContext ditherContext;
static constexpr int EPD_WIDTH  = 800;
static constexpr int EPD_HEIGHT = 480;

static constexpr int PIN_DBG_RX = 44;
static constexpr int PIN_DBG_TX = 43;
#define LOG Serial1
#define TAG "[e1002-viewer]"

// ----- Algorithm table -------------------------------------------------------

struct MethodInfo {
    const char*    name;
    DitherMethod   method;
    const char*    desc;
    const char*    bestFor;
};

static const MethodInfo METHODS[] = {
    {
        "NONE",
        DITHER_NONE,
        "Nearest-color quantization (no dithering)",
        "No error diffusion; fastest"
    },
    {
        "BAYER8",
        DITHER_BAYER8,
        "8x8 ordered Bayer dithering",
        "Fixed 8x8 threshold matrix; deterministic"
    },
    {
        "FS",
        DITHER_FS,
        "Floyd-Steinberg error diffusion",
        "4-tap kernel; narrow/fast ED"
    },
    {
        "ATKINSON",
        DITHER_ATKINSON,
        "Atkinson error diffusion (75% error spread)",
        "6-tap kernel; partial error spread"
    },
    {
        "BURKES",
        DITHER_BURKES,
        "Burkes error diffusion (Stucki minus bottom row)",
        "7-tap kernel; 2-row buffer"
    },
    {
        "SIERRA3",
        DITHER_SIERRA3,
        "Full Sierra / Sierra-3 error diffusion",
        "10-tap kernel; 3-row buffer"
    },
    {
        "PALETTE_MIX",
        DITHER_PALETTE_MIX,
        "Two-color palette-mix ordered dither",
        "Two-color spatial mixing; O(palette^2)"
    },
};
static constexpr int METHOD_COUNT = sizeof(METHODS) / sizeof(METHODS[0]);

// ----- Helpers ---------------------------------------------------------------

// ----- Dither parameters (full DitherConfig set) -----------------------------
// Every initial value is the library default from src/dither/Dither.h.
// Edit and re-flash to tune.

static const float DITHER_GAMMA         = 1.0f;  // x'=pow(x,1/gamma): >1 brightens
static const bool  DITHER_SERPENTINE    = false; // snake scan for error diffusion
static const bool  DITHER_LEGACY_CLAMP  = true;  // true=narrow[0,255](default); false=wide[-255,510]
static const float DITHER_SAT_BOOST     = 0.0f;  // [0,1] saturation boost; 0 = off
static const float DITHER_DARKNESS_BIAS = 0.0f;  // [0,0.5] pre-darken; 0 = off
static const float DITHER_CONTRAST      = 1.0f;  // 1.0 = no change
static const float DITHER_DIFF_STRENGTH = 1.0f;  // 1 = full diffusion, 0 = nearest only
static const float DITHER_WARMTH        = 0.0f;  // [-1,1]: +warmer / -cooler
static const ColorMetric DITHER_COLOR_METRIC = METRIC_RGB; // METRIC_REDMEAN deprecated for E6

// ----- Calibrated palette (optional) ------------------------------------------
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

static DitherConfig makeConfig(DitherMethod m) {
    DitherConfig cfg;
    cfg.method                 = m;
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
    return cfg;
}

static void log_mem(const char* tag) {
    LOG.printf("[mem] %-22s heap=%lu kB  PSRAM free=%lu/%lu kB\n", tag,
               (unsigned long)(ESP.getFreeHeap() / 1024),
               (unsigned long)(ESP.getFreePsram() / 1024),
               (unsigned long)(ESP.getPsramSize() / 1024));
    LOG.flush();
}

static void log_output_stats(const uint8_t* packed, int width, int height) {
    size_t counts[16] = {};
    uint32_t hash = 2166136261u;
    const size_t stride = ((size_t)width + 1u) / 2u;
    const size_t bytes = stride * height;
    for (size_t i = 0; i < bytes; ++i) {
        hash ^= packed[i];
        hash *= 16777619u;
    }
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const uint8_t byte = packed[(size_t)y * stride + (x >> 1)];
            const uint8_t code = (x & 1) ? (byte & 0x0F) : (byte >> 4);
            ++counts[code];
        }
    }
    LOG.printf(TAG " output bytes=%lu checksum=%08lX W=%lu G=%lu R=%lu Y=%lu B=%lu K=%lu\n",
               (unsigned long)bytes, (unsigned long)hash,
               (unsigned long)counts[0x0], (unsigned long)counts[0x2],
               (unsigned long)counts[0x6], (unsigned long)counts[0xB],
               (unsigned long)counts[0xD], (unsigned long)counts[0xF]);
}

// Dither the full card once and push it to the panel.
static bool show_method(const MethodInfo& info, uint8_t* packedBuf) {
    const int W = COLORCARD_E6_CUSTOM_WIDTH;
    const int H = COLORCARD_E6_CUSTOM_HEIGHT;

    const DitherConfig cfg = makeConfig(info.method);

    const uint32_t t0 = millis();
    if (!dither_image_4bpp(colorcard_e6_custom_data, W, H, cfg,
                           ditherContext, packedBuf)) {
        LOG.printf(TAG " dither_image_4bpp failed for %s\n", info.name);
        return false;
    }
    const uint32_t dt = millis() - t0;

    if (!display.pushImage4BPP(0, 0, W, H, packedBuf)) {
        LOG.println(TAG " pushImage4BPP failed");
        return false;
    }

    const GfxResult refreshResult = display.refresh();

    LOG.printf(TAG " %-11s dither %lu ms\n", info.name, (unsigned long)dt);
    LOG.printf(TAG " %-11s work=%lu B context=%lu B\n", info.name,
               (unsigned long)dither_working_memory_bytes(W, PAL_E6, info.method),
               (unsigned long)ditherContext.workingMemoryCapacity());
    log_output_stats(packedBuf, W, H);
    LOG.printf(TAG " %-11s best for: %s\n", info.name, info.bestFor);
    LOG.flush();

    return static_cast<bool>(refreshResult);
}

// =============================================================================

void setup() {
    LOG.begin(115200, SERIAL_8N1, PIN_DBG_RX, PIN_DBG_TX);
    delay(2500);
    LOG.println();
    LOG.println("==============================================");
    LOG.println("  reTerminal E1002 -- Color-Card Dither Viewer");
    LOG.println("==============================================");
    LOG.printf("[sys] chip      : ESP32-S3 @ %lu MHz\n", (unsigned long)ESP.getCpuFreqMHz());
    LOG.printf("[sys] PSRAM size: %lu kB\n", (unsigned long)(ESP.getPsramSize() / 1024));
    if (ESP.getPsramSize() == 0)
        LOG.println("[sys] !!! PSRAM is 0 kB -- enable Tools > PSRAM > OPI PSRAM !!!");
    LOG.printf("[sys] panel     : %d x %d\n", EPD_WIDTH, EPD_HEIGHT);
    LOG.printf("[sys] image     : %d x %d (embedded)\n", COLORCARD_E6_CUSTOM_WIDTH, COLORCARD_E6_CUSTOM_HEIGHT);
    LOG.println("[sys] tuning     : library defaults; edit DITHER_* constants to tune");
    LOG.println(TAG " methods:");
    for (int i = 0; i < METHOD_COUNT; ++i) {
        LOG.printf("  %d. %-11s -- %s\n", i + 1, METHODS[i].name, METHODS[i].desc);
    }
    LOG.flush();

    LOG.println(TAG " display.begin() ...");
    if (!display.begin()) {
        LOG.printf(TAG " display.begin failed: %s\n", display.lastResult().message);
        return;
    }
    log_mem("after display.begin");

    const int W = EMBEDDED_IMAGE_WIDTH;
    const int H = EMBEDDED_IMAGE_HEIGHT;
    const size_t packedBytes = ((size_t)W + 1u) / 2u * H;

    // Direct packed output: half the storage of a byte-per-pixel index buffer.
    uint8_t* packedBuf = (uint8_t*)ps_malloc(packedBytes);
    if (!packedBuf) packedBuf = (uint8_t*)malloc(packedBytes);
    if (!packedBuf) {
        LOG.println(TAG " OOM packed output -- aborting");
        return;
    }
    LOG.printf(TAG " direct 4bpp output=%lu B (saved=%lu B vs index)\n",
               (unsigned long)packedBytes,
               (unsigned long)((size_t)W * H - packedBytes));
    log_mem("after output alloc");

    // Render each method once and optionally hold.
    for (int i = 0; i < METHOD_COUNT; ++i) {
        LOG.printf(TAG " showing method %d/%d: %s\n", i + 1, METHOD_COUNT, METHODS[i].name);
        if (!show_method(METHODS[i], packedBuf)) {
            LOG.println(TAG " show_method failed -- stopping");
            break;
        }
        log_mem("after refresh");

        if (AUTO_CYCLE && i != METHOD_COUNT - 1) {
            LOG.printf(TAG " holding %lu ms before next...\n", (unsigned long)HOLD_MS);
            LOG.flush();
            delay(HOLD_MS);
        }
    }

    LOG.println(TAG " done. Cycle complete.");
    LOG.println("==============================================");
    LOG.flush();

    free(packedBuf);
}

void loop() {
    // After the cycle, nothing more to do.  Press RESET to restart.
    delay(1000);
}
