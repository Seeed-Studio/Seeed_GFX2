// This example plots a rotated Sprite to the screen using the pushRotated()
// function. Adapted for any screen size.
// Two rotation pivot points must be set, one for the Sprite and one for the
// display using setPivot(). These do not need to be within the visible area.
// When the Sprite is rotated and pushed with pushRotated(angle), the two
// pivot points coincide. Rotation is clockwise, angle in degrees.

// Adapted from the original TFT_eSPI example by Bodmer.

#include <Seeed_GFX.h>

// -- Board & Driver: edit for your hardware --
#include "board/boards/Wio_Terminal.h"
#include "driver/tft/Driver_ILI9341.h"
#include "panel/Panel_TFT.h"

Seeed_GFX display;
Seeed_Sprite spr;

void drawX(int x, int y);
void showMessage(String msg);

void setup() {
    Serial.begin(115200);

    // -- Edit for your board/config --
    if (!display.begin<Board_Wio_Terminal, Config_Wio_Terminal_ILI9341>()) {
        Serial.println(display.lastResult().message);
        while (true) delay(1000);
    }
    // Wio Terminal's upright landscape orientation is rotation 3.
    display.setRotation(3);
}

void loop() {
    int sw = display.width();
    int sh = display.height();
    int cx = sw / 2;     // center x
    int topH = 20;       // top message bar height

    // ---- Section 1: 90 degree angles ----
    showMessage("90 degree angles");
    display.setPivot(cx, topH + (sh - topH) / 2);  // pivot in center of play area
    drawX(cx, topH + (sh - topH) / 2);

    // Scale sprite size to screen
    int sprW = min(sw / 2, 64);
    int sprH = min(sprW / 2, 30);

    spr.setColorDepth(8);
    if (!spr.createSprite(&display, sprW, sprH)) {
        Serial.println("Rotated sprite allocation failed");
        while (true) delay(1000);
    }
    spr.setPivot(sprW / 2, sprH + sprH / 2);  // pivot below sprite center
    spr.fillSprite(TFT_BLACK);

    spr.setTextColor(TFT_GREEN);
    spr.setTextDatum(MC_DATUM);
    spr.drawString("Hello", sprW / 2, sprH / 2, 2);

    spr.pushRotated(0);
    spr.pushRotated(90);
    spr.pushRotated(180);
    spr.pushRotated(270);

    delay(2000);

    // ---- Section 2: 45 degree angles ----
    showMessage("45 degree angles");
    drawX(cx, topH + (sh - topH) / 2);

    spr.pushRotated(45);
    spr.pushRotated(135);
    spr.pushRotated(225);
    spr.pushRotated(315);

    delay(2000);

    // ---- Section 3: Moved Sprite pivot ----
    showMessage("Moved Sprite pivot");
    drawX(cx, topH + (sh - topH) / 2);

    spr.setPivot(-sprW / 3, sprH / 2);  // pivot outside sprite
    spr.pushRotated(45);
    spr.pushRotated(135);
    spr.pushRotated(225);
    spr.pushRotated(315);

    delay(2000);

    // ---- Section 4: Moved display pivot ----
    showMessage("Moved display pivot");
    int offX = sw / 3;
    int offY = topH + (sh - topH) / 3;
    display.setPivot(offX, offY);
    drawX(offX, offY);

    spr.pushRotated(45);
    spr.pushRotated(135);
    spr.pushRotated(225);
    spr.pushRotated(315);

    delay(2000);

    // ---- Section 5: Transparent rotations ----
    showMessage("Transparent rotation");
    int circleR = min(sw, sh - topH) / 3;
    int circCx = cx;
    int circCy = topH + (sh - topH) / 2;
    display.fillCircle(circCx, circCy, circleR, TFT_DARKGREY);

    display.setPivot(circCx, circCy);
    drawX(circCx, circCy);

    spr.deleteSprite();

    int numW = min(40, sw / 3);
    int numH = min(30, sh / 8);
    spr.setColorDepth(8);
    if (!spr.createSprite(&display, numW, numH)) {
        Serial.println("Number sprite allocation failed");
        while (true) delay(1000);
    }
    spr.setPivot(numW / 2, numH + numH);  // pivot below sprite

    spr.setTextColor(TFT_RED);
    spr.setTextDatum(MC_DATUM);

    int num = 1;
    for (int16_t angle = 30; angle <= 360; angle += 30) {
        spr.fillSprite(TFT_BLACK);
        spr.drawNumber(num, numW / 2, numH / 2, 2);
        spr.pushRotated(angle, TFT_BLACK);  // black = transparent
        num++;
    }

    spr.setTextColor(TFT_WHITE);
    spr.setPivot(-numW, numH / 2);  // pivot far outside

    for (int16_t angle = -90; angle < 270; angle += 30) {
        spr.fillSprite(TFT_BLACK);
        spr.drawNumber(angle + 90, numW / 2, numH / 2, 2);
        spr.pushRotated(angle, TFT_BLACK);
    }

    delay(5000);
    spr.deleteSprite();
}

// Draw an X centered on x,y
void drawX(int x, int y) {
    display.drawLine(x - 5, y - 5, x + 5, y + 5, TFT_WHITE);
    display.drawLine(x - 5, y + 5, x + 5, y - 5, TFT_WHITE);
}

// Show message bar at top of screen
void showMessage(String msg) {
    int sw = display.width();
    int sh = display.height();
    display.fillRect(0, 0, sw, 20, TFT_BLACK);
    display.fillRect(0, 20, sw, sh - 20, TFT_BLUE);

    uint8_t td = display.getTextDatum();
    display.setTextDatum(TC_DATUM);
    display.drawString(msg, sw / 2, 2, 2);
    display.setTextDatum(td);
}
