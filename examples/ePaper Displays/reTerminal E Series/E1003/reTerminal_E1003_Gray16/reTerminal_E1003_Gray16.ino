/**
 * Product: reTerminal E1003
 * Display: 10.3-inch ePaper, native 1872x1404 transfer orientation
 * Source:  Seeed_GFX examples/ePaper/Gray/GrayLevel16
 *
 * Gray16 allocates an approximately 1.25 MiB packed 4bpp frame buffer in
 * PSRAM. Index 0 is black and index 15 is white.
 */

#include <Seeed_GFX.h>

Seeed_GFX display(Seeed_Product::RETERMINAL_E1003);

static constexpr int E1003_DEBUG_RX = 44;
static constexpr int E1003_DEBUG_TX = 43;
#define LOG Serial1

void setup() {
    Serial.begin(115200);
    LOG.begin(115200, SERIAL_8N1, E1003_DEBUG_RX, E1003_DEBUG_TX);
    delay(2500);

    LOG.println();
    LOG.println("reTerminal E1003 Gray16");
    LOG.printf("PSRAM: %lu KB total, %lu KB free\n",
               static_cast<unsigned long>(ESP.getPsramSize() / 1024),
               static_cast<unsigned long>(ESP.getFreePsram() / 1024));

    if (!display.begin()) {
        LOG.printf("Display initialization failed: %s\n",
                   display.lastResult().message);
        LOG.flush();
        return;
    }

    // Match the original Seeed_GFX GrayLevel16 sequence: first force the
    // physical panel to a known pure-white state in 1bpp GC16 mode.  Merely
    // filling the newly allocated Gray16 framebuffer does not erase pigment
    // state left by the previously flashed sketch.
    LOG.println("Clearing previous image with a full white refresh ...");
    display.fillScreen(TFT_WHITE);
    const GfxResult clearResult = display.refresh();
    if (!clearResult) {
        LOG.printf("White pre-clear failed: %s\n", clearResult.message);
        LOG.flush();
        return;
    }

    const GfxResult modeResult =
        display.panel().configure(PanelMode::Gray16);
    if (!modeResult) {
        LOG.printf("Gray16 configuration failed: %s\n",
                   modeResult.message);
        LOG.flush();
        return;
    }

    LOG.printf("Gray16 buffer ready: %u x %u\n",
               display.width(), display.height());

    display.fillScreen(15);
    const int16_t bandHeight = display.height() / 16;
    for (uint8_t level = 0; level < 16; ++level) {
        const int16_t y = level * bandHeight;
        const int16_t h = (level == 15)
            ? display.height() - y : bandHeight;
        // In Gray16 mode the lower nibble is the native gray index.
        display.fillRect(0, y, display.width(), h, level);
    }

    const GfxResult refreshResult = display.refresh();
    if (!refreshResult) {
        LOG.printf("Gray16 refresh failed: %s\n", refreshResult.message);
    } else {
        LOG.println("reTerminal E1003 Gray16 example complete");
    }
    LOG.flush();
}

void loop() {
    delay(1000);
}
