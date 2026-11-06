/**
 * XIAO Console AMOLED — CO5300 quad-SPI AMOLED + touch demo
 *
 * Board  : Seeed "XIAO console" 2.16" AMOLED carrier (Board_XIAO_AMOLED_Seeed)
 * LCD    : CO5300, quad-SPI, RGB565 (Driver_CO5300)
 * Touch  : shared I2C1, controller follows the glass:
 *            2.16" square      -> CST9217  @0x5A  (command-page protocol)
 *            2.05"/1.75" round -> CST820/B @0x15  (5-byte single-point)
 *
 * No third-party dependencies: the board owns the on-board PCA9535 expander
 * (power rails VBAT/VCI/IOVCC + LCD/touch reset) through Seeed_GFX2's built-in
 * IoExpander9535, so this sketch only needs Seeed_GFX2 + the Arduino core.
 *
 * Select the glass with -DPANEL=216|205|175 (default 175). Driver_CO5300
 * derives the per-glass source-GRAM gap / factory MADCTL / init set from the
 * Config's resolution, so switching PANEL is the only change needed.
 */

#include <Arduino.h>
#include <Wire.h>

#include <Seeed_GFX.h>

#include "board/boards/XIAO_AMOLED_Seeed_Board.h"
#include "driver/tft/Driver_CO5300.h"
#include "panel/Panel_TFT.h"
#include "panel/configs/XIAO_AMOLED_Configs.h"
#include "touch/Touch_CST820.h"
#include "touch/Touch_CST9217.h"

/* ---- panel selection ------------------------------------------------------- */
#ifndef PANEL
#define PANEL 175
#endif

#if PANEL == 216
using PanelConfig = Config_XIAO_AMOLED;
#define PANEL_W 480
#define PANEL_H 480
#define PANEL_TAG "2.16\" square"
#define HAS_TOUCH 1
#define TOUCH_CHIP_CST9217 1
#elif PANEL == 205
using PanelConfig = Config_XIAO_AMOLED_205;
#define PANEL_W 368
#define PANEL_H 448
#define PANEL_TAG "2.05\" round"
#define HAS_TOUCH 1
#define TOUCH_CHIP_CST820 1
#elif PANEL == 175
using PanelConfig = Config_XIAO_AMOLED_175;
#define PANEL_W 466
#define PANEL_H 466
#define PANEL_TAG "1.75\" round"
#define HAS_TOUCH 1
#define TOUCH_CHIP_CST820 1
#else
#error "PANEL must be 216, 205, or 175"
#endif

Seeed_GFX display;

#if HAS_TOUCH
#if defined(TOUCH_CHIP_CST820)
static Touch_CST820 touch(/*intPin*/3, Wire, 0x15, PANEL_W, PANEL_H,
                          /*rstPin*/-1, /*sda*/-1, /*scl*/-1,
                          /*freq*/400000, /*manageWire*/false);
#else
static Touch_CST9217 touch(/*intPin*/3, Wire, 0x5A, PANEL_W, PANEL_H,
                           /*rstPin*/-1, /*sda*/-1, /*scl*/-1,
                           /*freq*/400000, /*manageWire*/false);
#endif
#endif

/* ---- one-shot demo frame ------------------------------------------------ */
static void drawDemo(void)
{
    display.fillScreen(TFT_BLACK);

    /* top: three colour bars */
    const int16_t barH = PANEL_H / 6;
    display.fillRect(0, 0,        PANEL_W, barH, TFT_RED);
    display.fillRect(0, barH,     PANEL_W, barH, TFT_GREEN);
    display.fillRect(0, 2 * barH, PANEL_W, barH, TFT_BLUE);
    // display.fillRect(0, 3 * barH, PANEL_W, barH, TFT_ORANGE);
    // display.fillRect(0, 4 * barH, PANEL_W, barH, TFT_MAGENTA);
    // display.fillRect(0, 5 * barH, PANEL_W, barH, TFT_YELLOW);
    /* mid: title + panel tag */
    const int16_t cy = PANEL_H / 2;
    display.setTextDatum(MC_DATUM);
    display.setTextSize(2);
    display.setTextColor(TFT_WHITE);
    display.drawString("CO5300 AMOLED", PANEL_W / 2, cy - 20);
    // textsize 1 renders glyphs via drawPixel (1x1 window), which the CO5300
    // silently drops; size >= 2 goes through fillRect and lands. See
    // Driver_CO5300.cpp writePixels()/writeFill().
    display.setTextSize(2);
    display.setTextColor(TFT_YELLOW);
    char buf[48];
    snprintf(buf, sizeof(buf), "%s  %dx%d  RGB565", PANEL_TAG, PANEL_W, PANEL_H);
    display.drawString(buf, PANEL_W / 2, cy + 12);

    /* bottom hint line */
    display.setTextColor(TFT_DARKGREY);
#if HAS_TOUCH
    display.drawString("Touch the panel", PANEL_W / 2, PANEL_H - 12);
#else
    display.drawString("touch disabled", PANEL_W / 2, PANEL_H - 12);
#endif
    display.setTextDatum(TL_DATUM);
}

void setup()
{
    Serial.begin(115200);
    delay(300);
    Serial.printf("\n=== XIAO Console AMOLED (%s) demo ===\n", PANEL_TAG);

    /* The board's begin() owns I2C (SDA=5 SCL=6), the PCA9535 expander, the
     * power rails and LCD/touch reset — nothing extra to wire up here. */
    if (!display.begin<Board_XIAO_AMOLED_Seeed, PanelConfig>()) {
        Serial.printf("[LCD] FAIL: %s\n", display.lastResult().message);
        for (;;) delay(1000);
    }
    Serial.printf("[LCD] OK  CO5300 %dx%d QSPI\n", PANEL_W, PANEL_H);

    drawDemo();

#if HAS_TOUCH
    /* Re-pulse TP-RST right before the touch controller's ID/config read: the
     * board's reset ran before the ~1.2 s QSPI LCD init and the controller can
     * idle back to non-ACKing in between (same sequence the factory UI uses). */
    if (Board_XIAO_AMOLED_Seeed* board =
            static_cast<Board_XIAO_AMOLED_Seeed*>(display.boardPtr())) {
        board->resetTouch();
    }
#if defined(TOUCH_CHIP_CST820)
    if (display.attachTouch(touch, display.panel().driver().bus())) {
        uint8_t cid = 0;
        touch.readChipId(cid);
        Serial.printf("[TCH] OK  CST820 @0x15 chipId=0x%02X\n", cid);
    } else {
        // Distinguish "not powered / held in reset" from "alive but ID read
        // failed": probe the 7-bit I2C address directly for an ACK.
        Wire.beginTransmission(0x15);
        const bool acks = (Wire.endTransmission() == 0);
        Serial.printf("[TCH] FAIL: %s (0x15 %s)\n",
                      display.lastResult().message,
                      acks ? "ACKs" : "no ACK");
    }
#else
    if (display.attachTouch(touch, display.panel().driver().bus())) {
        Serial.printf("[TCH] OK  CST9217 @0x5A chip=0x%04X proj=0x%04X res=%ux%u\n",
                      touch.chipType(), touch.projectId(),
                      touch.resolutionX(), touch.resolutionY());
    } else {
        // Distinguish "not powered / held in reset" from "alive but config
        // read failed": probe the 7-bit I2C address directly for an ACK.
        Wire.beginTransmission(0x5A);
        const bool acks = (Wire.endTransmission() == 0);
        Serial.printf("[TCH] FAIL: %s (0x5A %s)\n",
                      display.lastResult().message,
                      acks ? "ACKs" : "no ACK");
    }
#endif
#endif

    Serial.println("=== init done ===");
}

/* ---- touch crosshair + coordinate readout ------------------------------ */
#if HAS_TOUCH
static int32_t lastX = -1, lastY = -1;
static bool    lastTouched = false;

/* Bring-up: print the raw CST820 touch-report registers so the true byte
 * layout can be read off hardware instead of guessed.  Call this on every
 * down/move and read off the hex. */
#if defined(TOUCH_CHIP_CST820)
static void dumpCst820Regs(uint8_t base, uint8_t n)
{
    uint8_t rb[10] = {0};
    Wire.beginTransmission(0x15);
    Wire.write(base);
    if (Wire.endTransmission(false) != 0) {
        Serial.printf("[RAW@0x%02X] bus err\n", base);
        return;
    }
    const size_t got = Wire.requestFrom(0x15, (int)n);
    for (size_t i = 0; i < got && i < n; i++) rb[i] = (uint8_t)Wire.read();
    while (Wire.available()) Wire.read();   /* drain any overrun */
    Serial.printf("[RAW@0x%02X] ", base);
    for (size_t i = 0; i < n; i++) Serial.printf("%02X ", rb[i]);
    Serial.println();
}
#endif

static void updateTouch(void)
{
    int32_t tx = 0, ty = 0;
    const bool touched = display.getTouch(&tx, &ty);

    if (!touched) {
        if (lastTouched) {
            Serial.printf("[TCH] up   last=(%ld,%ld)\n",
                          (long)lastX, (long)lastY);
            display.fillRect(0, PANEL_H - 24, PANEL_W, 16, TFT_BLACK);
            lastTouched = false;
            lastX = lastY = -1;
        }
        return;
    }

    if (!lastTouched || tx != lastX || ty != lastY) {
        if (!lastTouched) {
            Serial.printf("[TCH] down (%ld,%ld)\n", (long)tx, (long)ty);
        } else {
            const int32_t dx = tx > lastX ? tx - lastX : lastX - tx;
            const int32_t dy = ty > lastY ? ty - lastY : lastY - ty;
            if (dx >= 2 || dy >= 2) {
                Serial.printf("[TCH] move (%ld,%ld)\n", (long)tx, (long)ty);
            }
        }

#if defined(TOUCH_CHIP_CST820)
        dumpCst820Regs(0x00, 8);
        dumpCst820Regs(0x02, 6);
#endif

        display.drawFastHLine(tx - 8, ty, 16, TFT_WHITE);
        display.drawFastVLine(tx, ty - 8, 16, TFT_WHITE);
        char buf[40];
        snprintf(buf, sizeof(buf), "touch %4d,%4d", (int)tx, (int)ty);
        display.fillRect(0, PANEL_H - 24, PANEL_W, 16, TFT_BLACK);
        display.setTextDatum(TL_DATUM);
        display.setTextSize(2);
        display.setTextColor(TFT_GREEN);
        display.drawString(buf, 4, PANEL_H - 24);
    }
    lastTouched = true;
    lastX = tx;
    lastY = ty;
}
#endif

void loop()
{
#if HAS_TOUCH
    updateTouch();
#else
    delay(100);
#endif
    delay(10);
}