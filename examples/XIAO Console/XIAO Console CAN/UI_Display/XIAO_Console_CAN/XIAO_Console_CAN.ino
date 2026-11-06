/**
 * XIAO Console CAN — ST7789 240x320 SPI TFT + GT911 touch demo
 *
 * Board  : XIAO ESP32-C5 "console for CAN" carrier (Board_XIAO_Console_CAN)
 * LCD    : ST7789V, 4-wire SPI, RGB565, RGB order, inverted (Driver_ST7789)
 * Touch  : GT911 @0x5D on the shared I2C1 (SDA=23 SCL=24), INT=GPIO0
 *
 * No third-party dependencies: the board owns the on-board PCA9535 expander
 * (LCD reset / backlight / touch reset) through Seeed_GFX2's built-in
 * IoExpander9535, so this sketch only needs Seeed_GFX2 + the Arduino core.
 *
 * Pin table (mirrors can_console_bsp):
 *   SCLK=8  MOSI=10  CS=7  DC=25  |  SDA=23  SCL=24  |  expander P0=LCD-RST
 *   P5=backlight  P1=TP-RST  |  touch INT=GPIO0
 */

#include <Arduino.h>
#include <Wire.h>

#include <Seeed_GFX.h>

#include "board/boards/XIAO_Console_CAN_Board.h"
#include "driver/tft/Driver_ST7789.h"
#include "panel/Panel_TFT.h"
#include "panel/configs/XIAO_Console_CAN_Configs.h"
#include "touch/Touch_GT911.h"

// Panel is the CAN console's fixed ST7789 240x320.
using PanelConfig = Config_XIAO_Console_CAN;
#define PANEL_W 240
#define PANEL_H 320

Seeed_GFX display;

// GT911 on the shared I2C1; reset is behind the expander (board->resetTouch()),
// so rstPin = -1 and manageWire = false (the board already ran Wire.begin).
static Touch_GT911 touch(/*intPin*/0, Wire, 0x5D, PANEL_W, PANEL_H,
                         /*rstPin*/-1, /*sda*/-1, /*scl*/-1,
                         /*freq*/400000, /*manageWire*/false);

/* ---- bring-up aid -------------------------------------------------------
 * Dump the raw GT911 status + INT line so a dead touch path can be told apart
 * from a decode bug without a scope:
 *   st bit7 never sets and INT stays high  -> GT911 not sensing (panel/hw)
 *   INT drops low on touch, st bit7 never  -> read/clear mismatch
 *   st bit7 sets but no point printed      -> point decode bug
 * INT is GPIO0 (direct MCU pin, NOT the expander). Flip GT911_DIAG to 0 once
 * touch is confirmed. */
#define GT911_DIAG 1

#if GT911_DIAG
static void dumpGt911Raw(void)
{
    static uint32_t lastMs = 0;
    const uint32_t now = millis();
    if (now - lastMs < 500) return;
    lastMs = now;

    uint8_t st = 0;
    Wire.beginTransmission(0x5D);
    Wire.write(0x81);   // 0x814E (big-endian register address)
    Wire.write(0x4E);
    if (Wire.endTransmission(false) != 0) {
        Serial.println("[GT911] bus err");
        return;
    }
    if (Wire.requestFrom(0x5D, 1) != 1) {
        Serial.println("[GT911] read err");
        return;
    }
    st = (uint8_t)Wire.read();

    Serial.printf("[GT911] st=%02X int=%u pts=%u",
                  (unsigned)st,
                  (unsigned)digitalRead(0),   // GT911 INT = GPIO0
                  (unsigned)(st & 0x0F));

    if (st & 0x80) {
        uint8_t p[8] = {0};
        Wire.beginTransmission(0x5D);
        Wire.write(0x81);   // 0x814F point 1
        Wire.write(0x4F);
        if (Wire.endTransmission(false) == 0 && Wire.requestFrom(0x5D, 8) == 8) {
            for (size_t i = 0; i < 8; i++) p[i] = (uint8_t)Wire.read();
            const uint16_t x = (uint16_t)(((uint16_t)(p[2] & 0x0F) << 8) | p[1]);
            const uint16_t y = (uint16_t)(((uint16_t)(p[4] & 0x0F) << 8) | p[3]);
            Serial.printf(" p0=(%u,%u) id=%u", (unsigned)x, (unsigned)y, (unsigned)p[0]);
        }
    }
    Serial.println();
}
#endif

/* ---- one-shot demo frame ------------------------------------------------ */
static void drawDemo(void)
{
    display.fillScreen(TFT_BLACK);

    /* three colour bars */
    const int16_t barH = PANEL_H / 6;
    display.fillRect(0, 0,        PANEL_W, barH, TFT_RED);
    display.fillRect(0, barH,     PANEL_W, barH, TFT_GREEN);
    display.fillRect(0, 2 * barH, PANEL_W, barH, TFT_BLUE);

    /* title + resolution tag */
    display.setTextDatum(MC_DATUM);
    display.setTextSize(2);
    display.setTextColor(TFT_WHITE);
    display.drawString("ST7789 SPI", PANEL_W / 2, PANEL_H / 2 - 20);
    char buf[48];
    snprintf(buf, sizeof(buf), "240x320 RGB565", PANEL_W, PANEL_H);
    display.setTextColor(TFT_YELLOW);
    display.drawString(buf, PANEL_W / 2, PANEL_H / 2 + 12);

    /* bottom hint */
    display.setTextColor(TFT_DARKGREY);
    display.drawString("Touch the panel", PANEL_W / 2, PANEL_H - 12);
    display.setTextDatum(TL_DATUM);
}

void setup()
{
    Serial.begin(115200);
    delay(300);
    Serial.println("\n=== XIAO Console CAN (ST7789 + GT911) demo ===");

    /* begin<>(): board->begin() owns I2C + PCA9535 + LCD reset/backlight;
     * the generic SPI init runs next (ST7789 init + INVON + RGB MADCTL). */
    if (!display.begin<Board_XIAO_Console_CAN, PanelConfig>()) {
        Serial.printf("[LCD] FAIL: %s\n", display.lastResult().message);
        for (;;) delay(1000);
    }
    Serial.println("[LCD] OK  ST7789 240x320 SPI");

    drawDemo();

    /* GT911 reset + I2C address select must land close to the controller probe:
     * the board's begin() deliberately skips touch reset, so pulse it here. */
    if (Board_XIAO_Console_CAN* board =
            static_cast<Board_XIAO_Console_CAN*>(display.boardPtr())) {
        board->resetTouch();
    }

    if (display.attachTouch(touch, display.panel().driver().bus())) {
        char id[3] = {0, 0, 0};
        touch.readProductId(id);
        Serial.printf("[TCH] OK  GT911 @0x5D id=%c%c%c\n", id[0], id[1], id[2]);
    } else {
        // Distinguish "held in reset / not powered" from "ACKing but ID read
        // failed": probe the 7-bit address directly for an ACK.
        Wire.beginTransmission(0x5D);
        const bool acks = (Wire.endTransmission() == 0);
        Serial.printf("[TCH] FAIL: %s (0x5D %s)\n",
                      display.lastResult().message,
                      acks ? "ACKs" : "no ACK");
    }

    Serial.println("=== init done ===");
}

/* ---- touch crosshair + coordinate readout ------------------------------ */
static int32_t lastX = -1, lastY = -1;
static bool    lastTouched = false;

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

void loop()
{
#if GT911_DIAG
    dumpGt911Raw();
#endif
    updateTouch();
    delay(10);
}