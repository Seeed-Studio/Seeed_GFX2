/**
 * Product: reTerminal E1003
 * Display: 10.3 inch monochrome ePaper, 1404x1872, 16-level grayscale capable
 * Wiki: https://wiki.seeedstudio.com/getting_started_with_reterminal_e1003/
 */

#include <Seeed_GFX.h>
#include <font/GFXFF/FreeSans18pt7b.h>
#include <font/GFXFF/FreeSansBold24pt7b.h>

Seeed_GFX display(Seeed_Product::RETERMINAL_E1003);

static constexpr int E1003_DEBUG_RX = 44;
static constexpr int E1003_DEBUG_TX = 43;
#define LOG Serial1

void setup() {
    Serial.begin(115200);
    LOG.begin(115200, SERIAL_8N1, E1003_DEBUG_RX, E1003_DEBUG_TX);
    delay(2500);
    LOG.println();
    LOG.println("reTerminal E1003 HelloWorld");
    LOG.printf("PSRAM: %lu KB total, %lu KB free\n",
               static_cast<unsigned long>(ESP.getPsramSize() / 1024),
               static_cast<unsigned long>(ESP.getFreePsram() / 1024));
    LOG.println("display.begin() ...");

    if (!display.begin()) {
        LOG.print("display.begin() failed: ");
        LOG.println(display.lastResult().message);
        LOG.flush();
        return;
    }
    LOG.printf("display ready: %u x %u\n", display.width(), display.height());

    // Pre-clear: force physical white refresh to erase previous image
    display.fillScreen(TFT_WHITE);
    display.refresh();
    delay(500);
    display.fillScreen(TFT_WHITE);
    display.fillRect(0, 0, display.width(), 180, TFT_BLACK);
    display.setTextDatum(MC_DATUM);

    // Do not enlarge the default 5x7 GLCD font on this 1872x1404 panel.
    // FreeSans starts from much denser glyph bitmaps, so its edges remain
    // readable even when scaled for the large E1003 glass.
    display.setTextColor(TFT_WHITE);
    display.setFreeFont(&FreeSansBold24pt7b);
    display.setTextSize(2);
    display.drawString("reTerminal E1003", display.width() / 2, 90);

    display.setTextColor(TFT_BLACK);
    display.setFreeFont(&FreeSans18pt7b);
    display.setTextSize(2);
    display.drawString("10.3 inch monochrome ePaper",
                       display.width() / 2, 360);

    display.drawRoundRect(260, 510, display.width() - 520, 430, 28, TFT_BLACK);
    display.setFreeFont(&FreeSansBold24pt7b);
    display.setTextSize(3);
    display.drawString("Hello World", display.width() / 2, 725);

    display.setFreeFont(nullptr);
    display.setTextSize(1);
    display.setTextDatum(TL_DATUM);
    LOG.println("display.refresh() ...");
    LOG.flush();
    const GfxResult refreshResult = display.refresh();
    if (!refreshResult.ok()) {
        LOG.print("display.refresh() failed: ");
        LOG.println(refreshResult.message);
    } else {
        LOG.println("display.refresh() complete");
    }
    LOG.flush();
}

void loop() { delay(1000); }
