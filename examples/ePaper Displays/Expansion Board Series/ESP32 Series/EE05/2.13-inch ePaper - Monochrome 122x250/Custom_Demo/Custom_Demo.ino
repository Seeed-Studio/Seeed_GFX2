/**
 * Demo: Manual/custom Seeed_GFX2 ePaper stack.
 * Panel: 2.13-inch monochrome 122x250
 * Board: XIAO ePaper Display Board EE05
 *
 * This example deliberately avoids the product enum and
 * begin<Board, PanelConfig>() shortcut. Board, SPI bus, driver IC, panel and
 * Seeed_GFX are constructed explicitly so each layer can be customized.
 */

#include <Seeed_GFX.h>
#include "board/boards/XIAO_EPaper_Boards.h"
#include "driver/epaper/Driver_SSD1680.h"
#include "panel/Panel_EPaper.h"

// 1) Board: power, BUSY and enable-pin behavior.
Board_XIAO_EPaper_EE05 board;

// 2) SPI bus: CS, DC, RST, MOSI, MISO, SCLK and write frequency.
Bus_SPI bus(44, 10, 38, D10, -1, D8, 10000000);

// 3) Driver IC: native controller RAM geometry.
Driver_SSD1680 driver(128, 250);

// 4) ePaper panel binds the driver, bus and board.
Panel_EPaper panel(driver, bus, &board);

// 5) Graphics API uses the caller-owned panel stack above.
Seeed_GFX display(panel);

void setup() {
    Serial.begin(115200);

    // Preserve the board's verified SPI mode/read clock and ESP32-S3 host.
    Board_XIAO_EPaper_EE05::configureBus(bus);
    const GfxResult geometryResult =
        panel.configureVisibleArea(122, 250);
    if (!geometryResult) {
        Serial.println(geometryResult.message);
        return;
    }
    if (!display.begin()) {
        Serial.println(display.lastResult().message);
        return;
    }

    display.setRotation(3);

    const int16_t w = display.width();
    const int16_t h = display.height();

    // Scale factor based on reference 200x200 screen
    const float sx = w / 200.0f;
    const float sy = h / 200.0f;
    const float s = (sx < sy) ? sx : sy;  // uniform scale

    // Adaptive text size: base 1 for small screens, larger for big screens
    const uint8_t titleSize = (h > 300) ? 3 : (h > 200) ? 2 : 1;
    const uint8_t bodySize  = (h > 300) ? 2 : 1;

    display.fillScreen(TFT_WHITE);

    // --- Frame ---
    display.drawRect(0, 0, w, h, TFT_BLACK);

    // --- Title ---
    display.setTextColor(TFT_BLACK);
    display.setTextSize(titleSize);
    display.drawString("Custom Demo", (int)(4 * sx), (int)(2 * sy));
    display.setTextSize(bodySize);
    char res[32];
    snprintf(res, sizeof(res), "%dx%d", w, h);
    display.drawRightString(res, w - (int)(4 * sx), (int)(4 * sy), bodySize);

    int y0 = (int)(22 * sy);
    display.drawLine((int)(4 * sx), y0, w - (int)(4 * sx), y0, TFT_BLACK);

    // --- Shapes row ---
    int shapeY = (int)(32 * sy);
    int shapeH = (int)(26 * sy);
    int gap = (int)(30 * sx);
    int x = (int)(10 * sx);
    int r = (int)(12 * s);

    // Circle
    display.drawCircle(x + r, shapeY + r, r, TFT_BLACK);
    display.fillCircle(x + r, shapeY + r, r / 2, TFT_BLACK);
    x += gap + r;

    // Rectangle
    int rw = (int)(28 * sx), rh = (int)(24 * sy);
    display.drawRect(x, shapeY, rw, rh, TFT_BLACK);
    display.fillRect(x + rw / 4, shapeY + rh / 4, rw / 2, rh / 2, TFT_BLACK);
    x += gap + rw;

    // X lines
    int xw = (int)(30 * sx);
    display.drawLine(x, shapeY, x + xw, shapeY + shapeH, TFT_BLACK);
    display.drawLine(x, shapeY + shapeH, x + xw, shapeY, TFT_BLACK);
    x += gap + xw;

    // Triangle
    int tw = (int)(30 * sx);
    display.drawTriangle(x, shapeY + shapeH, x + tw / 2, shapeY, x + tw, shapeY + shapeH, TFT_BLACK);

    // --- Divider ---
    int y1 = (int)(65 * sy);
    display.drawLine((int)(4 * sx), y1, w - (int)(4 * sx), y1, TFT_BLACK);

    // --- Text sizes ---
    int ty = (int)(72 * sy);
    display.setTextSize(bodySize);
    display.drawString("Text size", (int)(4 * sx), ty);
    ty += (int)(14 * sy);
    if (h > 250) {
        display.setTextSize(bodySize + 1);
        display.drawString("Bigger text", (int)(4 * sx), ty);
    }

    // --- Banner ---
    int bannerH = (int)(18 * sy);
    display.fillRect((int)(4 * sx), h - bannerH - (int)(4 * sy), w - (int)(8 * sx), bannerH, TFT_BLACK);
    display.setTextColor(TFT_WHITE);
    display.setTextSize(bodySize);
    display.drawCentreString("Seeed_GFX2", w / 2, h - bannerH, bodySize);

    const GfxResult refreshResult = display.refresh();
    if (!refreshResult) Serial.println(refreshResult.message);
}

void loop() { delay(1000); }
