/*
 * reTerminal E1004 -- interactive E6 color-card dither selector.
 *
 * Workflows: native color reference, seven dither methods, A/B candidates,
 * copyable DitherConfig output, full-screen native-color calibration,
 * and SD card photo mode for real-image parameter tuning.
 *
 * Color card mode:  embedded 800x480 test card centered on the 1200x1600
 *                   panel -- best for algorithm comparison.
 * Photo mode:       SD card image (resized to 800x480) -- best for parameter
 *                   tuning because real photos show visible changes.
 * Calibration:      full 1200x1600 screen for colorimeter measurement.
 *
 * Hardware: reTerminal E1004 (XIAO ESP32-S3 + 13.3" T133A01 6-color ePaper)
 * Logging: UART1, GPIO43 TX / GPIO44 RX, 115200 baud
 */

#include <SPI.h>
#include <FS.h>
#include <SD.h>
#include <Seeed_GFX.h>
#include <dither/Dither.h>
#include "bus/Bus_SPI.h"
#include "colorcard_e6_custom.h"
#include "image_loader.h"

Seeed_GFX display(Seeed_Product::reTerminal_E1004);

// Panel dimensions (portrait).
static constexpr int EPD_WIDTH  = 1200;
static constexpr int EPD_HEIGHT = 1600;

// Color card region -- 800x480 centered on the 1200x1600 panel.
static constexpr int CARD_W  = COLORCARD_E6_CUSTOM_WIDTH;   // 800
static constexpr int CARD_H  = COLORCARD_E6_CUSTOM_HEIGHT;  // 480
static constexpr int CARD_X  = (EPD_WIDTH  - CARD_W) / 2;   // 200
static constexpr int CARD_Y  = (EPD_HEIGHT - CARD_H) / 2;   // 560

// Packed 4bpp output buffer sized for the card only.
static constexpr size_t CARD_PACKED_BYTES =
    ((size_t)CARD_W + 1u) / 2u * CARD_H;  // 192000

static constexpr int PIN_DBG_RX = 44;
static constexpr int PIN_DBG_TX = 43;

// SD card pins (shared SPI with ePaper).
static constexpr int PIN_SD_CS   = 14;
static constexpr int PIN_SD_DET  = 15;
static constexpr int PIN_SD_EN   = 16;

// Path to the photo image on the SD card.
static const char* SD_IMAGE_PATH = "/img/demo.jpg";

#define LOG Serial1
#define TAG "[e1004-selector]"

// Struct must precede all function definitions so the Arduino auto-prototype
// generator can resolve types in forward declarations.
struct MethodInfo {
    const char* name;
    const char* enumName;
    DitherMethod method;
    const char* useCase;
};

static const MethodInfo METHODS[] = {
    {"NONE", "DITHER_NONE", DITHER_NONE,
     "flat graphics and a no-dither baseline"},
    {"BAYER8", "DITHER_BAYER8", DITHER_BAYER8,
     "stable patterns, charts and UI graphics"},
    {"FS", "DITHER_FS", DITHER_FS,
     "general photos and gradients"},
    {"ATKINSON", "DITHER_ATKINSON", DITHER_ATKINSON,
     "crisper, lighter photographic texture"},
    {"BURKES", "DITHER_BURKES", DITHER_BURKES,
     "smooth error diffusion with moderate cost"},
    {"SIERRA3", "DITHER_SIERRA3", DITHER_SIERRA3,
     "smooth gradients when processing time is acceptable"},
    {"PALETTE_MIX", "DITHER_PALETTE_MIX", DITHER_PALETTE_MIX,
     "flat colors that benefit from two-color spatial mixing"},
};

static constexpr int METHOD_COUNT =
    (int)(sizeof(METHODS) / sizeof(METHODS[0]));

static const uint8_t NATIVE_CODES[6] = {0x0, 0x2, 0x6, 0xB, 0xD, 0xF};
static const char* const NATIVE_NAMES[6] = {
    "WHITE", "GREEN", "RED", "YELLOW", "BLUE", "BLACK"
};

static DitherContext ditherContext;
static uint8_t* packedBuffer = nullptr;
static int currentPage = 0;         // 0=reference, 1..7=METHODS[0..6]
static int candidateA = -1;
static int candidateB = -1;
static int calibrationColor = 0;
static bool calibrationMode = false;

// --- Live-tunable parameters: the FULL DitherConfig set (serial commands) ---
// Initial values = library defaults from src/dither/Dither.h.
// Float params step by +/-0.05 (letter / uppercase letter); bool/enum params
// toggle on a keypress.  'd' resets everything to the library defaults.
static float g_gamma        = 1.00f;  // g/G   [0.30, 2.50]
static float g_satBoost     = 0.00f;  // s/S   [0.00, 1.00]
static float g_darknessBias = 0.00f;  // k/K   [0.00, 0.50]
static float g_contrast     = 1.00f;  // u/U   [0.05, 3.00] (library-valid (0,3])
static float g_diffStrength = 1.00f;  // e/E   [0.00, 1.00] 0=nearest only
static float g_warmth       = 0.00f;  // w/W   [-1.00, 1.00] +warmer/-cooler
static bool  g_serpentine   = false;  // y/Y   toggle snake scan
static bool  g_legacyClamp  = true;   // l/L   toggle narrow[0,255]/wide[-255,510]
static ColorMetric g_colorMetric = METRIC_RGB;  // z/Z toggle RGB/REDMEAN

// --- Per-algorithm ratings (1-5, 0=not yet rated) ---
static int g_ratings[7] = {0, 0, 0, 0, 0, 0, 0};
static bool waitingForRating = false;

// --- SD card photo mode ---
static RgbImage sdPhoto = {nullptr, 0, 0};
static bool photoLoaded = false;
static bool usePhotoMode = false;    // false=color card, true=SD photo

// --- Calibrated palette (optional) ---
// After measuring your panel's native colors with a colorimeter in calibration
// mode ('c' command), fill in the measured RGB values below and change
// USE_CALIBRATED_PALETTE to 1.  This gives the dither engine a more accurate
// picture of what each native code actually looks like on YOUR panel.
#define USE_CALIBRATED_PALETTE 0
static const Rgb kCalibratedE6_Rgb[6] = {
    {255, 255, 255},   // WHITE  (code 0x0) -- replace with measured
    { 40, 170,  50},   // GREEN  (code 0x2) -- replace with measured
    {210,  30,  40},   // RED    (code 0x6) -- replace with measured
    {250, 220,  20},   // YELLOW (code 0xB) -- replace with measured
    { 30,  80, 190},   // BLUE   (code 0xD) -- replace with measured
    { 15,  15,  15},   // BLACK  (code 0xF) -- replace with measured
};
static const uint8_t kCalibratedE6_Code[6] = {0x0, 0x2, 0x6, 0xB, 0xD, 0xF};

static DitherConfig makeConfig(DitherMethod method) {
    DitherConfig cfg;
    cfg.method                 = method;
    cfg.palette                = PAL_E6;
    cfg.gamma                  = g_gamma;
    cfg.invert                 = false;
    cfg.serpentine             = g_serpentine;
    cfg.legacyClamp            = g_legacyClamp;
    cfg.saturationBoost        = g_satBoost;
    cfg.darknessBias           = g_darknessBias;
    cfg.contrast               = g_contrast;
    cfg.errorDiffusionStrength = g_diffStrength;
    cfg.warmth                 = g_warmth;
    cfg.colorMetric            = g_colorMetric;
#if USE_CALIBRATED_PALETTE
    cfg.customPaletteRgb   = kCalibratedE6_Rgb;
    cfg.customPaletteCode  = kCalibratedE6_Code;
    cfg.customPaletteCount = 6;
#endif
    return cfg;
}

// ---------------------------------------------------------------------------

static void logMemory(const char* stage) {
    LOG.printf(TAG " %-20s heap=%lu kB, PSRAM=%lu/%lu kB\n",
               stage,
               (unsigned long)(ESP.getFreeHeap() / 1024u),
               (unsigned long)(ESP.getFreePsram() / 1024u),
               (unsigned long)(ESP.getPsramSize() / 1024u));
}

static void printHelp() {
    LOG.println();
    LOG.println("================ Dither Selector ================");
    LOG.println("  0       native E6 six-color reference page");
    LOG.println("  1..7    render one dither method");
    LOG.println("  n / p   next / previous page");
    LOG.println();
    LOG.println("  --- Image source ---");
    LOG.println("  i       toggle color card / SD photo");
    LOG.println();
    LOG.println("  --- Parameter tuning (re-dithers current page) ---");
    LOG.println("  g / G   gamma +0.05 / -0.05");
    LOG.println("  s / S   saturationBoost +0.05 / -0.05");
    LOG.println("  k / K   darknessBias +0.05 / -0.05");
    LOG.println("  u / U   contrast +0.05 / -0.05");
    LOG.println("  e / E   errorDiffusionStrength +0.05 / -0.05");
    LOG.println("  w / W   warmth +0.05 / -0.05");
    LOG.println("  y       toggle serpentine scan");
    LOG.println("  l       toggle legacyClamp (narrow [0,255] / wide [-255,510])");
    LOG.println("  z       toggle colorMetric (RGB / REDMEAN)");
    LOG.println("  d       reset all parameters to library defaults");
    LOG.println();
    LOG.println("  --- Scoring ---");
    LOG.println("  r       rate current algorithm (prompts 1-5)");
    LOG.println("  m       print rating summary table");
    LOG.println();
    LOG.println("  --- Candidates & config ---");
    LOG.println("  f       mark current algorithm as candidate A/B");
    LOG.println("  a / b   show candidate A / candidate B");
    LOG.println("  t       toggle between candidates A and B");
    LOG.println("  x       clear both candidates");
    LOG.println("  o       output DitherConfig with current parameters");
    LOG.println();
    LOG.println("  --- Calibration ---");
    LOG.println("  c       enter full-screen native-color calibration");
    LOG.println("  v       leave calibration and return to selector");
    LOG.println("  h       print this help");
    LOG.println();
    LOG.println("Calibration mode: 1..6 selects a native color; n/p changes it.");
    LOG.println("The physical result is the evaluation target; no universal score");
    LOG.println("can decide the best method for every type of content.");
    LOG.println("===================================================");
    LOG.flush();
}

static void printMethodList() {
    LOG.println(TAG " available methods:");
    for (int i = 0; i < METHOD_COUNT; ++i) {
        LOG.printf("  %d. %-11s - %s\n", i + 1, METHODS[i].name,
                   METHODS[i].useCase);
    }
}

// Fill the background area (outside the card) with white.
// Draws four rectangles above, below, left, and right of the card region.
static void clearBackground() {
    if (CARD_Y > 0) {
        display.fillRect(0, 0, EPD_WIDTH, CARD_Y, TFT_WHITE);
    }
    const int bottomY = CARD_Y + CARD_H;
    if (bottomY < EPD_HEIGHT) {
        display.fillRect(0, bottomY, EPD_WIDTH, EPD_HEIGHT - bottomY,
                         TFT_WHITE);
    }
    if (CARD_X > 0) {
        display.fillRect(0, CARD_Y, CARD_X, CARD_H, TFT_WHITE);
    }
    const int rightX = CARD_X + CARD_W;
    if (rightX < EPD_WIDTH) {
        display.fillRect(rightX, CARD_Y, EPD_WIDTH - rightX, CARD_H,
                         TFT_WHITE);
    }
}

// Fill the entire framebuffer with white (for calibration mode).
static void fillScreenWhite() {
    display.fillRect(0, 0, EPD_WIDTH, EPD_HEIGHT, TFT_WHITE);
}

static void drawSelectorHeader(const char* title) {
    char line[128];

    // Two centered lines: title (font 4) + live values of all 9 tunable
    // parameters (font 2), so the panel always shows what the serial commands
    // have set.  The band sits in the empty area above the centered card.
    display.fillRect(0, 30, EPD_WIDTH, 90, TFT_WHITE);
    display.setTextDatum(TC_DATUM);
    display.setTextColor(TFT_BLACK);
    display.drawString(title, EPD_WIDTH / 2, 38, 4);

    snprintf(line, sizeof(line),
             "g=%.2f s=%.2f d=%.2f t=%.2f e=%.2f w=%.2f serp=%d clamp=%d %s",
             g_gamma, g_satBoost, g_darknessBias, g_contrast,
             g_diffStrength, g_warmth, g_serpentine ? 1 : 0,
             g_legacyClamp ? 1 : 0,
             g_colorMetric == METRIC_RGB ? "RGB" : "REDMEAN");
    display.drawString(line, EPD_WIDTH / 2, 78, 2);
}

static bool refreshPanel(const char* pageName) {
    const GfxResult result = display.refresh();
    if (!result) {
        LOG.printf(TAG " refresh failed on %s: %s\n", pageName,
                   result.message ? result.message : "unknown error");
        return false;
    }
    return true;
}

// ----- page renderers -------------------------------------------------------

// Load the photo from SD card (once, cached).
static bool loadPhotoFromSD() {
    if (photoLoaded) return true;
    if (!load_image_from_sd(SD_IMAGE_PATH, CARD_W, CARD_H, &sdPhoto)) {
        LOG.printf(TAG " failed to load '%s'\n", SD_IMAGE_PATH);
        return false;
    }
    LOG.printf(TAG " loaded SD photo: %dx%d\n", sdPhoto.width, sdPhoto.height);
    photoLoaded = true;
    return true;
}

// Page 0: native E6 six-color reference (six vertical bars in card area).
static bool renderNativeReference() {
    // Packed buffer is sized for the card region only.
    // Fill with WHITE (code 0x0 → packed byte 0x00).
    memset(packedBuffer, 0x00, CARD_PACKED_BYTES);

    const size_t stride = ((size_t)CARD_W + 1u) / 2u;

    // Fill each color bar within the card buffer.
    for (int color = 0; color < 6; ++color) {
        const int x0 = color * CARD_W / 6;
        const int x1 = (color + 1) * CARD_W / 6;
        for (int y = 0; y < CARD_H; ++y) {
            for (int x = x0; x < x1; ++x) {
                const uint8_t code = NATIVE_CODES[color] & 0x0Fu;
                uint8_t& value = packedBuffer[(size_t)y * stride + (x >> 1)];
                if ((x & 1) == 0)
                    value = (uint8_t)((value & 0x0Fu) | (code << 4));
                else
                    value = (uint8_t)((value & 0xF0u) | code);
            }
        }
    }

    // Clear background and push the card to the center.
    clearBackground();
    if (!display.pushImage4BPP(CARD_X, CARD_Y, CARD_W, CARD_H, packedBuffer)) {
        LOG.println(TAG " pushImage4BPP failed for native reference");
        return false;
    }

    // Header above the card.
    drawSelectorHeader("[0/7] NATIVE E6 REFERENCE");

    // Color name labels below each bar.
    display.setTextDatum(TC_DATUM);
    display.setTextColor(TFT_BLACK);
    for (int color = 0; color < 6; ++color) {
        const int x0 = color * CARD_W / 6;
        const int x1 = (color + 1) * CARD_W / 6;
        display.drawString(NATIVE_NAMES[color],
                           CARD_X + (x0 + x1) / 2, CARD_Y + CARD_H + 16, 4);
    }

    LOG.println(TAG " page 0: native E6 codes W/G/R/Y/B/K");
    return refreshPanel("native reference");
}

// Pages 1-7: one dither method on the color card or SD photo.
static bool renderMethodPage(int page) {
    if (page < 1 || page > METHOD_COUNT) return false;

    const MethodInfo& info = METHODS[page - 1];
    const DitherConfig cfg = makeConfig(info.method);

    // Choose pixel source: SD photo or embedded color card.
    const uint8_t* srcPixels = colorcard_e6_custom_data;
    int srcW = CARD_W;
    int srcH = CARD_H;
    const char* srcLabel = "color card";

    if (usePhotoMode) {
        if (loadPhotoFromSD()) {
            srcPixels = sdPhoto.pixels;
            srcW = sdPhoto.width;
            srcH = sdPhoto.height;
            srcLabel = "SD photo";
        } else {
            LOG.println(TAG " !!! photo load FAILED -- falling back to color card");
            LOG.println(TAG "     check: SD card inserted? file at /img/demo.jpg?");
            LOG.println(TAG "     supported formats: JPEG, BMP, PNG");
            srcLabel = "color card (fallback)";
        }
    }

    LOG.printf(TAG " source: %s (%dx%d)\n", srcLabel, srcW, srcH);

    const uint32_t startMs = millis();

    if (!dither_image_4bpp(srcPixels, srcW, srcH,
                           cfg, ditherContext, packedBuffer, 0x0)) {
        LOG.printf(TAG " dithering failed for %s\n", info.name);
        return false;
    }

    const uint32_t elapsedMs = millis() - startMs;

    clearBackground();
    if (!display.pushImage4BPP(CARD_X, CARD_Y, CARD_W, CARD_H, packedBuffer)) {
        LOG.printf(TAG " pushImage4BPP failed for %s\n", info.name);
        return false;
    }

    // Title above the card.
    char title[128];
    snprintf(title, sizeof(title), "[%d/7] %s (%s)", page, info.name, srcLabel);
    drawSelectorHeader(title);

    // Use case hint below the card (no params -- header already shows them).
    display.setTextDatum(TC_DATUM);
    display.setTextColor(TFT_BLACK);
    display.drawString(info.useCase,
                       EPD_WIDTH / 2, CARD_Y + CARD_H + 16, 2);

    LOG.printf(TAG " page %d/7: %-11s src=%s dither=%lu ms, work=%lu B, "
                   "context=%lu B\n",
               page, info.name, srcLabel, (unsigned long)elapsedMs,
               (unsigned long)dither_working_memory_bytes(
                   CARD_W, PAL_E6, info.method),
               (unsigned long)ditherContext.workingMemoryCapacity());
    LOG.printf(TAG " params: gamma=%.2f sat=%.2f dark=%.2f serp=%d clamp=%d t=%.2f e=%.2f w=%.2f metric=%s\n",
               g_gamma, g_satBoost, g_darknessBias,
               g_serpentine ? 1 : 0, g_legacyClamp ? 1 : 0,
               g_contrast, g_diffStrength, g_warmth,
               g_colorMetric == METRIC_RGB ? "RGB" : "REDMEAN");
    LOG.printf(TAG " suggested use: %s\n", info.useCase);
    return refreshPanel(info.name);
}

static bool renderCurrentPage() {
    LOG.printf(TAG " rendering page %d; ePaper refresh may take a while...\n",
               currentPage);
    LOG.flush();
    return currentPage == 0 ? renderNativeReference()
                            : renderMethodPage(currentPage);
}

// Calibration: fill the ENTIRE panel with one native color.
// The full 1200x1600 gives a large uniform area for colorimeter measurement.
static bool renderCalibrationColor() {
    const uint8_t code = NATIVE_CODES[calibrationColor] & 0x0Fu;

    // Allocate a full-panel packed buffer (960 KB).
    const size_t fullPackedBytes = ((size_t)EPD_WIDTH + 1u) / 2u * EPD_HEIGHT;
    uint8_t* fullBuf = (uint8_t*)ps_malloc(fullPackedBytes);
    if (!fullBuf) fullBuf = (uint8_t*)malloc(fullPackedBytes);
    if (!fullBuf) {
        LOG.println(TAG " OOM for calibration buffer");
        return false;
    }

    // Fill with the target color code (both nibbles).
    const uint8_t packedByte = (uint8_t)((code << 4) | code);
    memset(fullBuf, packedByte, fullPackedBytes);

    if (!display.pushImage4BPP(0, 0, EPD_WIDTH, EPD_HEIGHT, fullBuf)) {
        LOG.println(TAG " calibration pushImage4BPP failed");
        free(fullBuf);
        return false;
    }
    free(fullBuf);

    LOG.printf(TAG " calibration %d/6: %s, raw code=0x%X\n",
               calibrationColor + 1, NATIVE_NAMES[calibrationColor], code);
    LOG.println(TAG " full-screen (1200x1600) for accurate measurement");
    return refreshPanel(NATIVE_NAMES[calibrationColor]);
}

// ----- A/B candidate helpers ------------------------------------------------

static void showCandidate(int page, const char* label) {
    if (page < 1 || page > METHOD_COUNT) {
        LOG.printf(TAG " candidate %s is not set; press f on an algorithm page\n",
                   label);
        return;
    }
    currentPage = page;
    LOG.printf(TAG " showing candidate %s: %s\n", label,
               METHODS[page - 1].name);
    renderCurrentPage();
}

static void markCandidate() {
    if (currentPage < 1 || currentPage > METHOD_COUNT) {
        LOG.println(TAG " native reference cannot be a candidate; choose 1..7");
        return;
    }
    if (currentPage == candidateA || currentPage == candidateB) {
        LOG.println(TAG " current method is already an A/B candidate");
        return;
    }
    if (candidateA < 1) {
        candidateA = currentPage;
        LOG.printf(TAG " candidate A = %s\n", METHODS[currentPage - 1].name);
    } else if (candidateB < 1) {
        candidateB = currentPage;
        LOG.printf(TAG " candidate B = %s\n", METHODS[currentPage - 1].name);
    } else {
        candidateB = currentPage;
        LOG.printf(TAG " candidate B replaced with %s\n",
                   METHODS[currentPage - 1].name);
    }
    LOG.println(TAG " render again to update the on-screen A/B header");
}

static void toggleCandidates() {
    if (candidateA < 1 || candidateB < 1) {
        LOG.println(TAG " set both A and B before using toggle");
        return;
    }
    currentPage = currentPage == candidateA ? candidateB : candidateA;
    LOG.printf(TAG " A/B toggle -> %s\n", METHODS[currentPage - 1].name);
    renderCurrentPage();
}

static void printOutputConfig() {
    if (currentPage < 1 || currentPage > METHOD_COUNT) {
        LOG.println(TAG " choose an algorithm page before outputting config");
        return;
    }

    const MethodInfo& info = METHODS[currentPage - 1];
    LOG.println();
    LOG.printf(TAG " selected: %s  (rating: %d/5)\n", info.name,
               g_ratings[currentPage - 1]);
    LOG.println("// Copy this configuration into your sketch:");
    LOG.println("DitherConfig cfg;");
    LOG.printf("cfg.method = %s;\n", info.enumName);
    LOG.println("cfg.palette = PAL_E6;");
    LOG.printf("cfg.gamma = %.2ff;\n", g_gamma);
    LOG.println("cfg.invert = false;");
    LOG.printf("cfg.serpentine = %s;\n", g_serpentine ? "true" : "false");
    LOG.printf("cfg.legacyClamp = %s;\n", g_legacyClamp ? "true" : "false");
    LOG.printf("cfg.saturationBoost = %.2ff;\n", g_satBoost);
    LOG.printf("cfg.darknessBias = %.2ff;\n", g_darknessBias);
    LOG.printf("cfg.contrast = %.2ff;\n", g_contrast);
    LOG.printf("cfg.errorDiffusionStrength = %.2ff;\n", g_diffStrength);
    LOG.printf("cfg.warmth = %.2ff;\n", g_warmth);
    LOG.printf("cfg.colorMetric = %s;\n",
               g_colorMetric == METRIC_RGB ? "METRIC_RGB" : "METRIC_REDMEAN");
    LOG.println();
    LOG.flush();
}

static void printRatingSummary() {
    LOG.println();
    LOG.println("================ Rating Summary =================");
    LOG.printf("  Parameters: gamma=%.2f  satBoost=%.2f  darknessBias=%.2f  serp=%d  clamp=%d  contrast=%.2f  diff=%.2f  warmth=%.2f\n",
               g_gamma, g_satBoost, g_darknessBias,
               g_serpentine ? 1 : 0, g_legacyClamp ? 1 : 0,
               g_contrast, g_diffStrength, g_warmth);
    LOG.println();
    LOG.println("  #  Algorithm      Rating  Best for");
    LOG.println("  -  -----------    ------  ----------------------------");
    for (int i = 0; i < METHOD_COUNT; ++i) {
        char stars[16];
        if (g_ratings[i] > 0) {
            int n = g_ratings[i];
            for (int j = 0; j < 5; ++j)
                stars[j] = (j < n) ? '*' : '.';
            stars[5] = '\0';
        } else {
            strcpy(stars, "  -  ");
        }
        LOG.printf("  %d  %-12s   %s   %s\n",
                   i + 1, METHODS[i].name, stars, METHODS[i].useCase);
    }
    LOG.println();

    // Find best rated.
    int bestIdx = -1, bestScore = 0;
    for (int i = 0; i < METHOD_COUNT; ++i) {
        if (g_ratings[i] > 0) {
            if (g_ratings[i] > bestScore) {
                bestScore = g_ratings[i];
                bestIdx = i;
            }
        }
    }
    if (bestIdx >= 0) {
        LOG.printf("  >> Top rated: %s (%d/5)\n",
                   METHODS[bestIdx].name, bestScore);
        LOG.println("  >> Press 'o' to output its DitherConfig.");
    } else {
        LOG.println("  >> No algorithms rated yet. Use 'r' to rate.");
    }
    LOG.println("===================================================");
    LOG.flush();
}

// ----- command dispatch -----------------------------------------------------

static void handleSelectorCommand(char command) {
    // Rating input mode: capture next digit 1-5.
    if (waitingForRating) {
        waitingForRating = false;
        if (command >= '1' && command <= '5') {
            if (currentPage >= 1 && currentPage <= METHOD_COUNT) {
                g_ratings[currentPage - 1] = command - '0';
                LOG.printf(TAG " rated %s = %d/5\n",
                           METHODS[currentPage - 1].name,
                           g_ratings[currentPage - 1]);
                LOG.println(TAG " press 'm' for rating summary");
            }
        } else {
            LOG.println(TAG " rating cancelled (not 1-5)");
        }
        return;
    }

    if (command >= '0' && command <= '7') {
        currentPage = command - '0';
        renderCurrentPage();
        return;
    }

    switch (command) {
        case 'n': case 'N': case '+':
            currentPage = (currentPage + 1) % (METHOD_COUNT + 1);
            renderCurrentPage();
            break;
        case 'p': case 'P': case '-':
            currentPage = (currentPage + METHOD_COUNT) % (METHOD_COUNT + 1);
            renderCurrentPage();
            break;

        // --- Source toggle ---
        case 'i': case 'I':
            usePhotoMode = !usePhotoMode;
            LOG.printf(TAG " switched to %s mode\n",
                       usePhotoMode ? "SD photo" : "color card");
            if (currentPage >= 1) renderCurrentPage();
            break;

        // --- Parameter tuning ---
        case 'g':
            g_gamma += 0.05f;
            if (g_gamma > 2.5f) g_gamma = 2.5f;
            LOG.printf(TAG " gamma -> %.2f\n", g_gamma);
            if (currentPage >= 1) renderCurrentPage();
            break;
        case 'G':
            g_gamma -= 0.05f;
            if (g_gamma < 0.3f) g_gamma = 0.3f;
            LOG.printf(TAG " gamma -> %.2f\n", g_gamma);
            if (currentPage >= 1) renderCurrentPage();
            break;
        case 's':
            g_satBoost += 0.05f;
            if (g_satBoost > 1.0f) g_satBoost = 1.0f;
            LOG.printf(TAG " satBoost -> %.2f\n", g_satBoost);
            if (currentPage >= 1) renderCurrentPage();
            break;
        case 'S':
            g_satBoost -= 0.05f;
            if (g_satBoost < 0.0f) g_satBoost = 0.0f;
            LOG.printf(TAG " satBoost -> %.2f\n", g_satBoost);
            if (currentPage >= 1) renderCurrentPage();
            break;
        case 'k':
            g_darknessBias += 0.05f;
            if (g_darknessBias > 0.5f) g_darknessBias = 0.5f;
            LOG.printf(TAG " darknessBias -> %.2f\n", g_darknessBias);
            if (currentPage >= 1) renderCurrentPage();
            break;
        case 'K':
            g_darknessBias -= 0.05f;
            if (g_darknessBias < 0.0f) g_darknessBias = 0.0f;
            LOG.printf(TAG " darknessBias -> %.2f\n", g_darknessBias);
            if (currentPage >= 1) renderCurrentPage();
            break;
        case 'u':
            g_contrast += 0.05f;
            if (g_contrast > 3.0f) g_contrast = 3.0f;
            LOG.printf(TAG " contrast -> %.2f\n", g_contrast);
            if (currentPage >= 1) renderCurrentPage();
            break;
        case 'U':
            g_contrast -= 0.05f;
            if (g_contrast < 0.05f) g_contrast = 0.05f;
            LOG.printf(TAG " contrast -> %.2f\n", g_contrast);
            if (currentPage >= 1) renderCurrentPage();
            break;
        case 'e':
            g_diffStrength += 0.05f;
            if (g_diffStrength > 1.0f) g_diffStrength = 1.0f;
            LOG.printf(TAG " errorDiffusionStrength -> %.2f\n", g_diffStrength);
            if (currentPage >= 1) renderCurrentPage();
            break;
        case 'E':
            g_diffStrength -= 0.05f;
            if (g_diffStrength < 0.0f) g_diffStrength = 0.0f;
            LOG.printf(TAG " errorDiffusionStrength -> %.2f\n", g_diffStrength);
            if (currentPage >= 1) renderCurrentPage();
            break;
        case 'w':
            g_warmth += 0.05f;
            if (g_warmth > 1.0f) g_warmth = 1.0f;
            LOG.printf(TAG " warmth -> %.2f\n", g_warmth);
            if (currentPage >= 1) renderCurrentPage();
            break;
        case 'W':
            g_warmth -= 0.05f;
            if (g_warmth < -1.0f) g_warmth = -1.0f;
            LOG.printf(TAG " warmth -> %.2f\n", g_warmth);
            if (currentPage >= 1) renderCurrentPage();
            break;
        case 'y': case 'Y':
            g_serpentine = !g_serpentine;
            LOG.printf(TAG " serpentine -> %d\n", g_serpentine ? 1 : 0);
            if (currentPage >= 1) renderCurrentPage();
            break;
        case 'l': case 'L':
            g_legacyClamp = !g_legacyClamp;
            LOG.printf(TAG " legacyClamp -> %d (%s)\n", g_legacyClamp ? 1 : 0,
                       g_legacyClamp ? "narrow [0,255]" : "wide [-255,510]");
            if (currentPage >= 1) renderCurrentPage();
            break;
        case 'z': case 'Z':
            g_colorMetric = (g_colorMetric == METRIC_RGB) ? METRIC_REDMEAN
                                                          : METRIC_RGB;
            LOG.printf(TAG " colorMetric -> %s\n",
                       g_colorMetric == METRIC_RGB
                           ? "RGB" : "REDMEAN (deprecated for E6)");
            if (currentPage >= 1) renderCurrentPage();
            break;
        case 'd': case 'D':
            g_gamma        = 1.00f;
            g_satBoost     = 0.00f;
            g_darknessBias = 0.00f;
            g_contrast     = 1.00f;
            g_diffStrength = 1.00f;
            g_warmth       = 0.00f;
            g_serpentine   = false;
            g_legacyClamp  = true;
            g_colorMetric  = METRIC_RGB;
            LOG.println(TAG " all parameters reset to library defaults");
            if (currentPage >= 1) renderCurrentPage();
            break;

        // --- Scoring ---
        case 'r': case 'R':
            if (currentPage < 1 || currentPage > METHOD_COUNT) {
                LOG.println(TAG " choose an algorithm page (1-7) first");
                break;
            }
            waitingForRating = true;
            LOG.printf(TAG " enter rating 1-5 for %s (or any other key to cancel):\n",
                       METHODS[currentPage - 1].name);
            break;
        case 'm': case 'M':
            printRatingSummary();
            break;

        // --- Candidates & config ---
        case 'f': case 'F':
            markCandidate();
            break;
        case 'a': case 'A':
            showCandidate(candidateA, "A");
            break;
        case 'b': case 'B':
            showCandidate(candidateB, "B");
            break;
        case 't': case 'T':
            toggleCandidates();
            break;
        case 'x': case 'X':
            candidateA = -1;
            candidateB = -1;
            LOG.println(TAG " candidates cleared");
            break;
        case 'o': case 'O':
            printOutputConfig();
            break;

        // --- Calibration & help ---
        case 'c': case 'C':
            calibrationMode = true;
            calibrationColor = 0;
            LOG.println(TAG " entered calibration mode; v returns to selector");
            renderCalibrationColor();
            break;
        case 'v': case 'V':
            renderCurrentPage();
            break;
        case 'h': case 'H': case '?':
            printHelp();
            break;
        default:
            LOG.printf(TAG " unknown command '%c'; press h for help\n", command);
            break;
    }
}

static void handleCalibrationCommand(char command) {
    if (command >= '1' && command <= '6') {
        calibrationColor = command - '1';
        renderCalibrationColor();
        return;
    }

    switch (command) {
        case 'n': case 'N': case '+': case ' ':
            calibrationColor = (calibrationColor + 1) % 6;
            renderCalibrationColor();
            break;
        case 'p': case 'P': case '-':
            calibrationColor = (calibrationColor + 5) % 6;
            renderCalibrationColor();
            break;
        case 'v': case 'V':
            calibrationMode = false;
            LOG.println(TAG " returned to selector mode");
            renderCurrentPage();
            break;
        case 'h': case 'H': case '?':
            printHelp();
            break;
        default:
            LOG.println(TAG " calibration: use 1..6, n, p, or v");
            break;
    }
}

// ----- entry points ---------------------------------------------------------

void setup() {
    LOG.begin(115200, SERIAL_8N1, PIN_DBG_RX, PIN_DBG_TX);
    delay(2500);
    LOG.println();
    LOG.println("====================================================");
    LOG.println(" reTerminal E1004 -- Color-Card Dither Selector");
    LOG.println(" Panel: 1200x1600 (portrait), card: 800x480 centered");
    LOG.println("====================================================");
    LOG.printf(TAG " card=%d x %d at (%d,%d), output=%lu bytes\n",
               CARD_W, CARD_H, CARD_X, CARD_Y,
               (unsigned long)CARD_PACKED_BYTES);
    LOG.printf(TAG " chip=%lu MHz, PSRAM=%lu kB\n",
               (unsigned long)ESP.getCpuFreqMHz(),
               (unsigned long)(ESP.getPsramSize() / 1024u));
    if (ESP.getPsramSize() == 0) {
        LOG.println(TAG " WARNING: enable OPI PSRAM in the board menu");
    }
    printMethodList();
#if USE_CALIBRATED_PALETTE
    LOG.println(TAG " palette: CALIBRATED (custom RGB from kCalibratedE6_Rgb)");
#else
    LOG.println(TAG " palette: default PAL_E6 (set USE_CALIBRATED_PALETTE=1 to use measured RGB)");
#endif
    printHelp();

    if (CARD_W != COLORCARD_E6_CUSTOM_WIDTH ||
        CARD_H != COLORCARD_E6_CUSTOM_HEIGHT) {
        LOG.println(TAG " colorcard_e6_custom must be 800x480");
        return;
    }

    LOG.println(TAG " display.begin() ...");
    if (!display.begin()) {
        LOG.printf(TAG " display.begin failed: %s\n",
                   display.lastResult().message);
        return;
    }
    logMemory("after display.begin");

    packedBuffer = (uint8_t*)ps_malloc(CARD_PACKED_BYTES);
    if (!packedBuffer) packedBuffer = (uint8_t*)malloc(CARD_PACKED_BYTES);
    if (!packedBuffer) {
        LOG.println(TAG " cannot allocate packed output buffer");
        return;
    }
    logMemory("after output alloc");

    // --- SD card init (shared SPI with ePaper) ---
    pinMode(PIN_SD_EN, OUTPUT);
    digitalWrite(PIN_SD_EN, HIGH);
    pinMode(PIN_SD_DET, INPUT_PULLUP);
    delay(50);

    {
        Bus_SPI& displayBus = static_cast<Bus_SPI&>(display.panel().driver().bus());
        SPIClass* spi = displayBus.spiInstance();
        if (spi) {
            LOG.println(TAG " SD.begin (shared SPI) ...");
            if (SD.begin(PIN_SD_CS, *spi)) {
                LOG.printf("[sd] mounted; card size = %llu MB\n",
                           (unsigned long long)(SD.cardSize() / (1024ULL * 1024ULL)));
                if (SD.exists(SD_IMAGE_PATH)) {
                    LOG.printf(TAG " photo '%s' found on SD card\n", SD_IMAGE_PATH);
                    LOG.println(TAG " press 'i' to switch to photo mode");
                } else {
                    LOG.printf(TAG " '%s' not found -- photo mode unavailable\n",
                               SD_IMAGE_PATH);
                    LOG.println(TAG "   copy a JPG/BMP/PNG to /img/ on the SD card");
                }
            } else {
                LOG.println(TAG " SD.begin failed -- photo mode unavailable");
            }
        } else {
            LOG.println(TAG " shared SPI unavailable -- photo mode unavailable");
        }
    }

    renderCurrentPage();
    LOG.println(TAG " ready; enter a command and press Send");
    LOG.flush();
}

void loop() {
    while (LOG.available() > 0) {
        const int value = LOG.read();
        if (value < 0) break;
        const char command = (char)value;
        if (command == '\r' || command == '\n' || command == '\t') continue;

        // Log every command received (helps debug double-send / echo issues).
        LOG.printf(TAG " cmd='%c' (0x%02X)  usePhotoMode=%d page=%d\n",
                   command, (unsigned char)command,
                   usePhotoMode ? 1 : 0, currentPage);

        if (!packedBuffer) {
            LOG.println(TAG " display/output buffer is not initialized");
            continue;
        }
        if (calibrationMode) {
            handleCalibrationCommand(command);
        } else {
            handleSelectorCommand(command);
        }
    }
    delay(10);
}
