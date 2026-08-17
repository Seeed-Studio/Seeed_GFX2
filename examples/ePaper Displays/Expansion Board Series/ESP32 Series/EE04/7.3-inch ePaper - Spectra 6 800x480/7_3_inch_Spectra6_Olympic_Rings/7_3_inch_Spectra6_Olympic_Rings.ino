/**
 * XIAO ePaper Display Board (ESP32-S3) - EE04.
 * 7.3-inch full-color E Ink Spectra 6 Olympic rings example.
 *
 * Hardware:
 *   - XIAO ESP32S3
 *   - XIAO ePaper Display Board (ESP32-S3) - EE04
 *   - 7.3-inch 800x480 E Ink Spectra 6 panel (ED2208)
 *
 * The display is refreshed once after the complete image is rendered.
 */

#include <Seeed_GFX.h>
#include "board/boards/XIAO_EPaper_Boards.h"
#include "driver/epaper/Driver_ED2208.h"
#include "panel/Panel_EPaper.h"

Seeed_GFX display;

static constexpr int16_t RING_RADIUS = 68;
static constexpr int16_t RING_THICKNESS = 11;

/** Draw a ring as several concentric circle outlines. */
void drawThickRing(int16_t cx, int16_t cy, uint16_t color) {
    const int16_t innerRadius = RING_RADIUS - RING_THICKNESS / 2;
    const int16_t outerRadius = RING_RADIUS + RING_THICKNESS / 2;

    for (int16_t radius = innerRadius; radius <= outerRadius; ++radius) {
        display.drawCircle(cx, cy, radius, color);
    }
}

void drawOlympicRings() {
    display.fillScreen(TFT_WHITE);

    // Simple header and border keep the drawing centered on the 800x480 panel.
    display.drawRect(12, 12, display.width() - 24, display.height() - 24, TFT_BLACK);
    display.drawRect(17, 17, display.width() - 34, display.height() - 34, TFT_BLACK);

    display.setTextColor(TFT_BLACK);
    display.setTextSize(3);
    const char* title = "OLYMPIC RINGS";
    const int16_t titleWidth = display.textWidth(title);
    display.drawString(title, (display.width() - titleWidth) / 2, 48);

    // Standard five-ring arrangement: blue, black, red / yellow, green.
    // Draw the lower row first so the upper row remains visually prominent
    // where the strokes overlap on the limited ePaper color palette.
    drawThickRing(325, 275, TFT_YELLOW);
    drawThickRing(475, 275, TFT_GREEN);

    drawThickRing(250, 190, TFT_BLUE);
    drawThickRing(400, 190, TFT_BLACK);
    drawThickRing(550, 190, TFT_RED);

    display.setTextSize(1);
    const char* hardware = "EE04 + 7.3-inch Spectra 6 ePaper";
    const int16_t hardwareWidth = display.textWidth(hardware);
    display.drawString(hardware, (display.width() - hardwareWidth) / 2, 405);
}

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("EE04 7.3-inch Olympic rings example");

    if (!display.begin<Board_XIAO_EPaper_EE04,
                       Config_XIAO_EPaper_7inch3_Colorful_ED2208>()) {
        Serial.printf("Display initialization failed: %s\n",
                      display.lastResult().message);
        return;
    }

    Panel_EPaper& epaper = static_cast<Panel_EPaper&>(display.panel());
    epaper.initColorfulMode();

    drawOlympicRings();

    Serial.println("Refreshing ePaper; this can take several seconds...");
    const GfxResult refreshResult = display.refresh();
    if (!refreshResult) {
        Serial.printf("Refresh failed: %s\n", refreshResult.message);
        return;
    }
    Serial.println("Olympic rings drawing complete.");
}

void loop() {
    // ePaper retains the image without continuous redraws.
    delay(1000);
}
