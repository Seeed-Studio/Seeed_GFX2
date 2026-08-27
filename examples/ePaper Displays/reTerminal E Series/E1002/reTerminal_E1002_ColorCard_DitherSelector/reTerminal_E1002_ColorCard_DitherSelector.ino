/*
 * reTerminal E1002 -- interactive E6 color-card dither selector.
 *
 * Workflows: native color reference, seven dither methods, A/B candidates,
 * copyable DitherConfig output, full-screen native-color calibration,
 * and SD card photo mode for real-image parameter tuning.
 *
 * Color card mode:  embedded 800x480 test card -- best for algorithm comparison.
 * Photo mode:       SD card image -- best for parameter tuning (gamma, satBoost)
 *                   because real photos show visible changes that solid-color
 *                   test cards do not.
 *
 * Hardware: reTerminal E1002 (XIAO ESP32-S3 + 800x480 six-color ePaper)
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

Seeed_GFX display(Seeed_Product::RETERMINAL_E1002);

static constexpr int EPD_WIDTH = 800;
static constexpr int EPD_HEIGHT = 480;
static constexpr int PIN_DBG_RX = 44;
static constexpr int PIN_DBG_TX = 43;
static constexpr size_t PACKED_BYTES =
    ((size_t)EPD_WIDTH + 1u) / 2u * EPD_HEIGHT;

// SD card pins (shared SPI with ePaper).
static constexpr int PIN_SD_SCK  = 7;
static constexpr int PIN_SD_MISO = 8;
static constexpr int PIN_SD_MOSI = 9;
static constexpr int PIN_SD_CS   = 14;
static constexpr int PIN_SD_DET  = 15;
static constexpr int PIN_SD_EN   = 16;

// Path to the photo image on the SD card.
static const char* SD_IMAGE_PATH = "/img/demo.jpg";

#define LOG Serial1
#define TAG "[e1002-selector]"

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
static int currentPage = 0;  // 0=reference, 1..7=METHODS[0..6]
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

// --- Photo mode (SD card image) ---
static RgbImage sdPhoto;            // RGB888 image loaded from SD
static bool photoLoaded = false;    // true when sdPhoto contains valid data
static bool usePhotoMode = false;   // true = use SD photo, false = color card

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
    LOG.println("  i       toggle color card / SD photo mode");
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

static void setPackedPixel(int x, int y, uint8_t code) {
    const size_t stride = ((size_t)EPD_WIDTH + 1u) / 2u;
    uint8_t& value = packedBuffer[(size_t)y * stride + (size_t)(x >> 1)];
    if ((x & 1) == 0) {
        value = (uint8_t)((value & 0x0Fu) | ((code & 0x0Fu) << 4));
    } else {
        value = (uint8_t)((value & 0xF0u) | (code & 0x0Fu));
    }
}

static bool refreshDisplay(const char* pageName) {
    const GfxResult result = display.refresh();
    if (!result) {
        LOG.printf(TAG " refresh failed on %s: %s\n", pageName,
                   result.message ? result.message : "unknown error");
        return false;
    }
    return true;
}

static void drawSelectorHeader(const char* title) {
    char line[128];

    // Clear header area (two lines: title + full live parameter values).
    display.fillRect(0, 0, EPD_WIDTH, 52, TFT_WHITE);
    display.setTextDatum(TL_DATUM);
    display.setTextColor(TFT_BLACK);

    // Line 1: page title.
    display.drawString(title, 8, 2, 4);

    // Line 2: live values of all 9 tunable parameters, so the panel always
    // shows what the serial commands have set.
    snprintf(line, sizeof(line),
             "g=%.2f s=%.2f d=%.2f t=%.2f e=%.2f w=%.2f serp=%d clamp=%d %s",
             g_gamma, g_satBoost, g_darknessBias, g_contrast,
             g_diffStrength, g_warmth, g_serpentine ? 1 : 0,
             g_legacyClamp ? 1 : 0,
             g_colorMetric == METRIC_RGB ? "RGB" : "REDMEAN");
    display.drawString(line, 8, 34, 2);
}

// ----- SD card photo loading -------------------------------------------------

static bool loadPhotoFromSD() {
    if (photoLoaded) return true;  // already loaded

    LOG.println(TAG " loading photo from SD card ...");
    logMemory("before photo load");

    if (!load_image_from_sd(SD_IMAGE_PATH, EPD_WIDTH, EPD_HEIGHT, &sdPhoto)) {
        LOG.printf(TAG " failed to load '%s'\n", SD_IMAGE_PATH);
        LOG.println(TAG "   check SD card, file path, and image format");
        return false;
    }

    LOG.printf(TAG " photo loaded: %dx%d (%lu kB)\n",
               sdPhoto.width, sdPhoto.height,
               (unsigned long)((size_t)sdPhoto.width * sdPhoto.height * 3 / 1024));
    logMemory("after photo load");
    photoLoaded = true;
    return true;
}

static bool renderNativeReference() {
    memset(packedBuffer, 0x00, PACKED_BYTES);
    for (int color = 0; color < 6; ++color) {
        const int x0 = color * EPD_WIDTH / 6;
        const int x1 = (color + 1) * EPD_WIDTH / 6;
        for (int y = 0; y < EPD_HEIGHT; ++y) {
            for (int x = x0; x < x1; ++x) {
                setPackedPixel(x, y, NATIVE_CODES[color]);
            }
        }
    }

    if (!display.pushImage4BPP(0, 0, EPD_WIDTH, EPD_HEIGHT, packedBuffer)) {
        LOG.println(TAG " pushImage4BPP failed for native reference");
        return false;
    }

    drawSelectorHeader("[0/7] NATIVE E6 REFERENCE");
    display.setTextDatum(MC_DATUM);
    display.setTextColor(TFT_BLACK);
    for (int color = 0; color < 6; ++color) {
        const int x0 = color * EPD_WIDTH / 6;
        const int x1 = (color + 1) * EPD_WIDTH / 6;
        display.fillRect(x0 + 4, EPD_HEIGHT - 35, x1 - x0 - 8, 31,
                         TFT_WHITE);
        display.drawString(NATIVE_NAMES[color], (x0 + x1) / 2,
                           EPD_HEIGHT - 18, 4);
    }

    LOG.println(TAG " page 0: native E6 codes W/G/R/Y/B/K");
    return refreshDisplay("native reference");
}

static bool renderMethodPage(int page) {
    if (page < 1 || page > METHOD_COUNT) return false;

    const MethodInfo& info = METHODS[page - 1];
    const DitherConfig cfg = makeConfig(info.method);

    // Choose image source: SD photo or embedded color card.
    const uint8_t* srcPixels;
    int srcW, srcH;
    const char* srcLabel;

    if (usePhotoMode) {
        if (loadPhotoFromSD()) {
            srcPixels = sdPhoto.pixels;
            srcW = sdPhoto.width;
            srcH = sdPhoto.height;
            srcLabel = "SD photo";
        } else {
            // Photo load FAILED -- log clearly and fall back.
            LOG.println(TAG " !!! photo load FAILED -- falling back to color card");
            LOG.println(TAG "     check: SD card inserted? file at /img/demo.jpg?");
            LOG.println(TAG "     supported formats: JPEG, BMP, PNG");
            srcPixels = colorcard_e6_custom_data;
            srcW = COLORCARD_E6_CUSTOM_WIDTH;
            srcH = COLORCARD_E6_CUSTOM_HEIGHT;
            srcLabel = "color card (fallback)";
        }
    } else {
        srcPixels = colorcard_e6_custom_data;
        srcW = COLORCARD_E6_CUSTOM_WIDTH;
        srcH = COLORCARD_E6_CUSTOM_HEIGHT;
        srcLabel = "color card";
    }

    LOG.printf(TAG " source: %s (%dx%d)\n", srcLabel, srcW, srcH);

    const uint32_t startMs = millis();
    if (!dither_image_4bpp(srcPixels, srcW, srcH,
                           cfg, ditherContext, packedBuffer, 0x0)) {
        LOG.printf(TAG " dithering failed for %s\n", info.name);
        return false;
    }

    const uint32_t elapsedMs = millis() - startMs;
    if (!display.pushImage4BPP(0, 0, EPD_WIDTH, EPD_HEIGHT, packedBuffer)) {
        LOG.printf(TAG " pushImage4BPP failed for %s\n", info.name);
        return false;
    }

    char title[96];
    snprintf(title, sizeof(title), "[%d/7] %s (%s)",
             page, info.name, srcLabel);
    drawSelectorHeader(title);

    LOG.printf(TAG " page %d/7: %-11s [%s] dither=%lu ms, work=%lu B\n",
               page, info.name, srcLabel, (unsigned long)elapsedMs,
               (unsigned long)dither_working_memory_bytes(
                   EPD_WIDTH, PAL_E6, info.method),
               (unsigned long)ditherContext.workingMemoryCapacity());
    LOG.printf(TAG " params: gamma=%.2f sat=%.2f dark=%.2f serp=%d clamp=%d t=%.2f e=%.2f w=%.2f metric=%s\n",
               g_gamma, g_satBoost, g_darknessBias,
               g_serpentine ? 1 : 0, g_legacyClamp ? 1 : 0,
               g_contrast, g_diffStrength, g_warmth,
               g_colorMetric == METRIC_RGB ? "RGB" : "REDMEAN");
    LOG.printf(TAG " suggested use: %s\n", info.useCase);
    return refreshDisplay(info.name);
}

static bool renderCurrentPage() {
    LOG.printf(TAG " rendering page %d; ePaper refresh may take a while...\n",
               currentPage);
    LOG.flush();
    return currentPage == 0 ? renderNativeReference()
                            : renderMethodPage(currentPage);
}

static bool renderCalibrationColor() {
    const uint8_t code = NATIVE_CODES[calibrationColor] & 0x0Fu;
    const uint8_t packedCode = (uint8_t)((code << 4) | code);
    memset(packedBuffer, packedCode, PACKED_BYTES);

    if (!display.pushImage4BPP(0, 0, EPD_WIDTH, EPD_HEIGHT, packedBuffer)) {
        LOG.println(TAG " calibration pushImage4BPP failed");
        return false;
    }

    LOG.printf(TAG " calibration %d/6: %s, raw code=0x%X\n",
               calibrationColor + 1, NATIVE_NAMES[calibrationColor], code);
    LOG.println(TAG " the panel is intentionally full-screen with no label");
    return refreshDisplay(NATIVE_NAMES[calibrationColor]);
}

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
    int ratedCount = 0;
    for (int i = 0; i < METHOD_COUNT; ++i) {
        if (g_ratings[i] > 0) {
            ratedCount++;
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

        // --- Image source ---
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

void setup() {
    LOG.begin(115200, SERIAL_8N1, PIN_DBG_RX, PIN_DBG_TX);
    delay(2500);
    LOG.println();
    LOG.println("====================================================");
    LOG.println(" reTerminal E1002 -- Color-Card Dither Selector");
    LOG.println("====================================================");
    LOG.printf(TAG " image=%d x %d, direct output=%lu bytes\n",
               COLORCARD_E6_CUSTOM_WIDTH, COLORCARD_E6_CUSTOM_HEIGHT,
               (unsigned long)PACKED_BYTES);
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

    if (COLORCARD_E6_CUSTOM_WIDTH != EPD_WIDTH ||
        COLORCARD_E6_CUSTOM_HEIGHT != EPD_HEIGHT) {
        LOG.println(TAG " embedded color card must be exactly 800x480");
        return;
    }

    LOG.println(TAG " display.begin() ...");
    if (!display.begin()) {
        LOG.printf(TAG " display.begin failed: %s\n",
                   display.lastResult().message);
        return;
    }
    logMemory("after display.begin");

    // SD card initialization (shares SPI bus with ePaper display).
    pinMode(PIN_SD_EN, OUTPUT);
    digitalWrite(PIN_SD_EN, HIGH);
    pinMode(PIN_SD_DET, INPUT_PULLUP);
    delay(50);

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

    packedBuffer = (uint8_t*)ps_malloc(PACKED_BYTES);
    if (!packedBuffer) packedBuffer = (uint8_t*)malloc(PACKED_BYTES);
    if (!packedBuffer) {
        LOG.println(TAG " cannot allocate the 192000-byte packed buffer");
        return;
    }
    logMemory("after output alloc");

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

