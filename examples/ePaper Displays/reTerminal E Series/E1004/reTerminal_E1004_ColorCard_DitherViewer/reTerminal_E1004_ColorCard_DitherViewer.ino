/*
 * reTerminal E1004 -- Full-screen color-card dither viewer.
 *
 * Purpose: cycle through the 7 kept dither algorithms on the SAME reference
 * color card, one at a time.  The 800x480 card is rendered in the center of
 * the 1200x1600 panel.  This lets a user SEE each algorithm's character
 * (color bias, texture, smoothness) on the larger panel.
 *
 *   1. NONE        -- nearest-color (no dither)
 *   2. BAYER8      -- 8x8 ordered Bayer
 *   3. FS          -- Floyd-Steinberg error diffusion
 *   4. ATKINSON    -- Atkinson error diffusion
 *   5. BURKES      -- Burkes error diffusion
 *   6. SIERRA3     -- Full Sierra / Sierra-3 error diffusion
 *   7. PALETTE_MIX  -- Two-color palette-mix ordered dither
 *
 * The sketch automatically advances every 8 seconds.  Watch the panel and
 * read the serial legend to map what you see to an algorithm name.
 *
 * Hardware: reTerminal E1004 (XIAO ESP32-S3 + 13.3" T133A01 6-color ePaper).
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

Seeed_GFX display(Seeed_Product::RETERMINAL_E1004);
static DitherContext ditherContext;

// Panel dimensions (portrait).
static constexpr int EPD_WIDTH  = 1200;
static constexpr int EPD_HEIGHT = 1600;

// Color card region -- 800x480 centered on the 1200x1600 panel.
static constexpr int CARD_W = COLORCARD_E6_CUSTOM_WIDTH;   // 800
static constexpr int CARD_H = COLORCARD_E6_CUSTOM_HEIGHT;  // 480
static constexpr int CARD_X = (EPD_WIDTH  - CARD_W) / 2;   // 200
static constexpr int CARD_Y = (EPD_HEIGHT - CARD_H) / 2;   // 560

static constexpr int PIN_DBG_RX = 44;
static constexpr int PIN_DBG_TX = 43;

#define LOG Serial1
#define TAG "[e1004-viewer]"

// ----- Algorithm table (must precede functions for Arduino prototype) -------

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

static DitherConfig makeConfig(DitherMethod m) {
    DitherConfig cfg;
    cfg.method          = m;
    cfg.palette         = PAL_E6;
    cfg.gamma           = 1.1f;
    cfg.serpentine      = true;
    cfg.legacyClamp     = false; // false=wide[-255,510](default) better for photos; true=narrow[0,255] cleaner pure-color boundaries
    cfg.saturationBoost = 0.2f;
    cfg.darknessBias    = 0.0f;
    cfg.colorMetric     = METRIC_RGB;
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
    LOG.printf(TAG " output bytes=%lu checksum=%08lX "
                   "W=%lu G=%lu R=%lu Y=%lu B=%lu K=%lu\n",
               (unsigned long)bytes, (unsigned long)hash,
               (unsigned long)counts[0x0], (unsigned long)counts[0x2],
               (unsigned long)counts[0x6], (unsigned long)counts[0xB],
               (unsigned long)counts[0xD], (unsigned long)counts[0xF]);
}

// Fill the area outside the card with white.
static void clearBackground() {
    if (CARD_Y > 0)
        display.fillRect(0, 0, EPD_WIDTH, CARD_Y, TFT_WHITE);
    const int bottomY = CARD_Y + CARD_H;
    if (bottomY < EPD_HEIGHT)
        display.fillRect(0, bottomY, EPD_WIDTH, EPD_HEIGHT - bottomY,
                         TFT_WHITE);
    if (CARD_X > 0)
        display.fillRect(0, CARD_Y, CARD_X, CARD_H, TFT_WHITE);
    const int rightX = CARD_X + CARD_W;
    if (rightX < EPD_WIDTH)
        display.fillRect(rightX, CARD_Y, EPD_WIDTH - rightX, CARD_H,
                         TFT_WHITE);
}

// Dither the full card once and push it to the panel center.
static bool show_method(int index, const MethodInfo& info, uint8_t* packedBuf) {
    const DitherConfig cfg = makeConfig(info.method);

    const uint32_t t0 = millis();
    if (!dither_image_4bpp(colorcard_e6_custom_data, CARD_W, CARD_H, cfg,
                           ditherContext, packedBuf)) {
        LOG.printf(TAG " dither_image_4bpp failed for %s\n", info.name);
        return false;
    }
    const uint32_t dt = millis() - t0;

    // Clear background, then push the card to the center.
    clearBackground();
    if (!display.pushImage4BPP(CARD_X, CARD_Y, CARD_W, CARD_H, packedBuf)) {
        LOG.println(TAG " pushImage4BPP failed");
        return false;
    }

    // Header above the card.
    char title[96];
    snprintf(title, sizeof(title), "[%d/%d] %s",
             index + 1, METHOD_COUNT, info.name);
    display.fillRect(0, 40, EPD_WIDTH, 50, TFT_WHITE);
    display.setTextDatum(TC_DATUM);
    display.setTextColor(TFT_BLACK);
    display.drawString(title, EPD_WIDTH / 2, 48, 4);

    // Config details below the card.
    display.drawString("gamma=1.10  sat=0.20  serpentine",
                       EPD_WIDTH / 2, CARD_Y + CARD_H + 16, 2);
    display.drawString(info.bestFor,
                       EPD_WIDTH / 2, CARD_Y + CARD_H + 50, 2);

    const GfxResult refreshResult = display.refresh();

    LOG.printf(TAG " %-11s dither %lu ms\n", info.name, (unsigned long)dt);
    LOG.printf(TAG " %-11s work=%lu B context=%lu B\n", info.name,
               (unsigned long)dither_working_memory_bytes(
                   CARD_W, PAL_E6, info.method),
               (unsigned long)ditherContext.workingMemoryCapacity());
    log_output_stats(packedBuf, CARD_W, CARD_H);
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
    LOG.println("  reTerminal E1004 -- Color-Card Dither Viewer");
    LOG.println("  Panel: 1200x1600 (portrait)");
    LOG.println("  Card:  800x480 centered at (200,560)");
    LOG.println("==============================================");
    LOG.printf("[sys] chip      : ESP32-S3 @ %lu MHz\n",
               (unsigned long)ESP.getCpuFreqMHz());
    LOG.printf("[sys] PSRAM size: %lu kB\n",
               (unsigned long)(ESP.getPsramSize() / 1024));
    if (ESP.getPsramSize() == 0)
        LOG.println("[sys] !!! PSRAM is 0 kB -- enable Tools > PSRAM > OPI PSRAM !!!");
    LOG.printf("[sys] panel     : %d x %d\n", EPD_WIDTH, EPD_HEIGHT);
    LOG.printf("[sys] card      : %d x %d at (%d,%d)\n",
               CARD_W, CARD_H, CARD_X, CARD_Y);
    LOG.println("[sys] tuning     : visual baseline; palette RGB is not measured");
    LOG.println(TAG " methods:");
    for (int i = 0; i < METHOD_COUNT; ++i) {
        LOG.printf("  %d. %-11s -- %s\n", i + 1, METHODS[i].name,
                   METHODS[i].desc);
    }
    LOG.flush();

    LOG.println(TAG " display.begin() ...");
    if (!display.begin()) {
        LOG.printf(TAG " display.begin failed: %s\n",
                   display.lastResult().message);
        return;
    }
    log_mem("after display.begin");

    const size_t packedBytes = ((size_t)CARD_W + 1u) / 2u * CARD_H;

    uint8_t* packedBuf = (uint8_t*)ps_malloc(packedBytes);
    if (!packedBuf) packedBuf = (uint8_t*)malloc(packedBytes);
    if (!packedBuf) {
        LOG.println(TAG " OOM packed output -- aborting");
        return;
    }
    LOG.printf(TAG " direct 4bpp output=%lu B\n", (unsigned long)packedBytes);
    log_mem("after output alloc");

    for (int i = 0; i < METHOD_COUNT; ++i) {
        LOG.printf(TAG " showing method %d/%d: %s\n",
                   i + 1, METHOD_COUNT, METHODS[i].name);
        if (!show_method(i, METHODS[i], packedBuf)) {
            LOG.println(TAG " show_method failed -- stopping");
            break;
        }
        log_mem("after refresh");

        if (AUTO_CYCLE && i != METHOD_COUNT - 1) {
            LOG.printf(TAG " holding %lu ms before next...\n",
                       (unsigned long)HOLD_MS);
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
    delay(1000);
}
