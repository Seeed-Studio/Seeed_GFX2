/**
 * Product: reTerminal Sticky
 * Display: 3.97 inch monochrome ePaper, 800x480, SSD1677/SSD2677
 * Wiki: https://www.seeedstudio.com/sticky/docs/quick-start
 *
 * Demo: a "sticky-note dashboard" composed entirely from drawing primitives.
 * Exercises in one sketch:
 *   - fillRect / drawRoundRect / fillRoundRect for cards
 *   - drawLine / drawFastHLine for outlines, dividers, strikethrough
 *   - fillTriangle for a folded note corner
 *   - drawPixel checkerboard to fake 50% gray on the 1bpp panel
 *   - drawArc / drawCircle for a progress gauge
 *   - drawString / drawNumber with text datum + textWidth alignment
 *
 * Everything is drawn once in setup() and pushed with a single update().
 */
#include <Seeed_GFX.h>

Seeed_GFX display(Seeed_Product::reTerminal_Sticky);

// ---------------------------------------------------------------- helpers --

// Fake a 50% gray region on the 1bpp panel with a 1px checkerboard.
// Used here as a soft drop shadow behind the sticky note.
static void fillCheckerboard(int16_t x, int16_t y, int16_t w, int16_t h) {
    for (int16_t j = 0; j < h; ++j) {
        for (int16_t i = 0; i < w; ++i) {
            if (((i + j) & 1) == 0) display.drawPixel(x + i, y + j, TFT_BLACK);
        }
    }
}

// One row of the task list: checkbox + label; done rows get a check mark
// and a strikethrough on the text.
static void drawTaskRow(int16_t x, int16_t y, bool done, const char* label) {
    const int16_t box = 16;
    display.drawRect(x, y, box, box, TFT_BLACK);
    if (done) {
        display.drawLine(x + 3, y + 8, x + 7, y + 12, TFT_BLACK);
        display.drawLine(x + 7, y + 12, x + 13, y + 3, TFT_BLACK);
    }
    display.drawString(label, x + box + 10, y + 3);
    if (done) {  // strikethrough, width taken from textWidth()
        const int16_t tw = display.textWidth(label);
        display.drawFastHLine(x + box + 10, y + 10, tw, TFT_BLACK);
    }
}

// Battery icon (white on the black header), ~70% full.
static void drawBatteryIcon(int16_t x, int16_t y) {
    display.drawRect(x, y, 34, 18, TFT_WHITE);
    display.fillRect(x + 34, y + 5, 4, 8, TFT_WHITE);    // terminal nub
    display.fillRect(x + 3, y + 3, 22, 12, TFT_WHITE);   // charge level
}

// Signal-strength icon: four rising bars (white on the black header).
static void drawSignalIcon(int16_t x, int16_t y) {
    const int16_t heights[4] = {6, 10, 14, 18};
    for (int i = 0; i < 4; ++i) {
        display.fillRect(x + i * 12, y + 18 - heights[i], 8, heights[i],
                         TFT_WHITE);
    }
}

// ------------------------------------------------------------------- main --

void setup() {
    Serial.begin(115200);
    delay(2000);  // let USB CDC enumerate so the early prints are not lost
    Serial.println("[sticky] sketch start");
    Serial.println("[sticky] display.begin() ... (busy timeouts can take a while)");
    if (!display.begin()) {
        Serial.println(display.lastResult().message);
        return;
    }

    // Diagnostics: Sticky production mixes SSD1677 and SSD2677 modules;
    // Driver_Sticky_Auto probes at begin() (reset -> 0x70 -> read one byte,
    // 0x07 = SSD2677). Print the resolution so units with a missing image
    // can be traced to either the probe or the driver path.
    // No driverAs<>() here: Arduino targets build with -fno-rtti, so
    // dynamic_cast is unavailable; IDriver::probedChipId() covers it.
    IDriver* sticky = display.driverPtr();
    const int chipId = sticky->probedChipId();
    if (chipId >= 0) {
        Serial.printf("[sticky] probe chipId=0x%02X -> driver %s\n",
                      chipId, sticky->name());
    } else {
        Serial.printf("[sticky] driver: %s\n", sticky->name());
    }

    // Pre-clear: force physical white refresh to erase the previous image
    display.fillScreen(TFT_WHITE);
    display.refresh();
    delay(500);
    display.fillScreen(TFT_WHITE);

    const uint32_t t0 = millis();
    const int16_t w = display.width();   // 800
    const int16_t h = display.height();  // 480

    // -------------------------------------------------------------- header --
    display.fillRect(0, 0, w, 64, TFT_BLACK);
    display.setTextColor(TFT_WHITE);
    display.setTextSize(3);
    display.drawString("reTerminal Sticky", 16, 18);

    display.setTextSize(2);
    display.setTextDatum(TR_DATUM);  // right-aligned date
    display.drawString("MON 2026-08-24", w - 112, 24);
    display.setTextDatum(TL_DATUM);
    drawSignalIcon(w - 104, 22);
    drawBatteryIcon(w - 46, 23);

    // ---------------------------------------------------- left: task card --
    const int16_t cardX = 20, cardY = 84, cardW = 370, cardH = 330;
    display.drawRoundRect(cardX, cardY, cardW, cardH, 10, TFT_BLACK);
    // Title bar: rounded top corners, square bottom edge
    display.fillRoundRect(cardX, cardY, cardW, 34, 10, TFT_BLACK);
    display.fillRect(cardX, cardY + 17, cardW, 17, TFT_BLACK);
    display.setTextSize(2);
    display.drawString("TASKS", cardX + 12, cardY + 9);
    display.setTextDatum(TR_DATUM);
    display.drawString("2/4", cardX + cardW - 12, cardY + 9);
    display.setTextDatum(TL_DATUM);

    display.setTextColor(TFT_BLACK);
    const char* tasks[] = {
        "Calibrate E6 palette",
        "Sync Arduino libraries",
        "Write Sticky dashboard demo",
        "Audit SSD168x waveforms",
    };
    const bool done[] = {true, true, false, false};
    for (int i = 0; i < 4; ++i) {
        drawTaskRow(cardX + 16, cardY + 44 + i * 60, done[i], tasks[i]);
    }

    // Card footer: divider + completion status
    display.drawFastHLine(cardX + 12, cardY + cardH - 34, cardW - 24,
                          TFT_BLACK);
    display.setTextSize(1);
    display.drawString("2 of 4 completed", cardX + 12, cardY + cardH - 24);
    display.setTextDatum(TR_DATUM);
    display.drawString("this week", cardX + cardW - 12, cardY + cardH - 24);
    display.setTextDatum(TL_DATUM);

    // ------------------------------------------- right top: sticky note --
    const int16_t noteX = 412, noteY = 84, noteW = 368, noteH = 140;
    const int16_t fold = 26;
    fillCheckerboard(noteX + 8, noteY + 8, noteW, noteH);     // drop shadow
    display.fillRect(noteX, noteY, noteW, noteH, TFT_WHITE);  // note body
    // Outline with a folded bottom-right corner (segment by segment)
    display.drawLine(noteX, noteY, noteX + noteW, noteY, TFT_BLACK);
    display.drawLine(noteX, noteY, noteX, noteY + noteH, TFT_BLACK);
    display.drawLine(noteX, noteY + noteH,
                     noteX + noteW - fold, noteY + noteH, TFT_BLACK);
    display.drawLine(noteX + noteW, noteY,
                     noteX + noteW, noteY + noteH - fold, TFT_BLACK);
    display.drawLine(noteX + noteW - fold, noteY + noteH,
                     noteX + noteW, noteY + noteH - fold, TFT_BLACK);
    // The folded flap itself
    display.fillTriangle(noteX + noteW - fold, noteY + noteH,
                         noteX + noteW, noteY + noteH - fold,
                         noteX + noteW - fold, noteY + noteH - fold,
                         TFT_BLACK);
    display.setTextSize(2);
    display.drawString("NOTE", noteX + 12, noteY + 10);
    display.setTextSize(1);
    display.drawString("Don't forget:", noteX + 12, noteY + 36);
    display.drawString("- Water the plants", noteX + 12, noteY + 56);
    display.drawString("- Ship E1005 samples", noteX + 12, noteY + 74);
    display.drawString("- Review dither audit", noteX + 12, noteY + 92);

    // ----------------------------------------- right bottom: bar chart --
    const int16_t chartX = 412, chartY = 240, chartW = 210, chartH = 174;
    display.drawRoundRect(chartX, chartY, chartW, chartH, 8, TFT_BLACK);
    display.setTextSize(1);
    display.drawString("COMMITS / WEEK", chartX + 10, chartY + 8);

    const int16_t values[7] = {3, 7, 2, 9, 5, 1, 4};
    const char days[7] = {'M', 'T', 'W', 'T', 'F', 'S', 'S'};
    const int16_t baseY = chartY + chartH - 22;   // x axis
    const int16_t maxBar = 96;                    // height of the max value
    display.drawFastHLine(chartX + 12, baseY, chartW - 24, TFT_BLACK);
    for (int i = 0; i < 7; ++i) {
        const int16_t barW = 18;
        const int16_t bx = chartX + 21 + i * (barW + 7);
        const int16_t bh = values[i] * maxBar / 9;
        display.fillRect(bx, baseY - bh, barW, bh, TFT_BLACK);
        display.drawNumber(values[i], bx + 5, baseY - bh - 12);
        char dl[2] = {days[i], 0};
        display.drawString(dl, bx + 6, baseY + 5);
    }

    // --------------------------------------- right bottom: progress gauge --
    const int16_t gaugeX = 642, gaugeY = 240, gaugeW = 138, gaugeH = 174;
    display.drawRoundRect(gaugeX, gaugeY, gaugeW, gaugeH, 8, TFT_BLACK);
    const int16_t gx = gaugeX + gaugeW / 2;       // 711
    const int16_t gy = gaugeY + 82;               // 322
    const int16_t gr = 56, gir = 40;
    const int16_t pct = 68;
    // drawArc angles: 0 deg = 3 o'clock, increasing clockwise on screen
    // (atan2 based). Start at 12 o'clock (270 deg) and sweep pct of a turn.
    const uint32_t sweep = 360UL * pct / 100;
    display.drawArc(gx, gy, gr, gir, 270, 270 + sweep,
                    TFT_BLACK, TFT_WHITE, false);
    display.drawCircle(gx, gy, gr, TFT_BLACK);    // crisp ring outline
    display.drawCircle(gx, gy, gir, TFT_BLACK);
    display.setTextSize(2);
    display.setTextDatum(MC_DATUM);
    display.drawString("68%", gx, gy);
    display.setTextSize(1);
    display.setTextDatum(TC_DATUM);
    display.drawString("TODAY", gx, gy + gr + 8);
    display.setTextDatum(TL_DATUM);

    // -------------------------------------------------------------- footer --
    display.fillRect(0, h - 40, w, 40, TFT_BLACK);
    display.setTextColor(TFT_WHITE);
    display.setTextSize(1);
    display.drawString("Seeed Studio  |  seeedstudio.com", 16, h - 26);
    display.setTextDatum(TC_DATUM);
    display.drawString("Sticky Dashboard demo", w / 2, h - 26);
    display.setTextDatum(TR_DATUM);
    display.drawString("800x480  |  1 bpp", w - 16, h - 26);
    display.setTextDatum(TL_DATUM);

    display.update();
    Serial.printf("[sticky-dashboard] drawn in %lu ms\n",
                  (unsigned long)(millis() - t0));
}

void loop() { delay(1000); }
