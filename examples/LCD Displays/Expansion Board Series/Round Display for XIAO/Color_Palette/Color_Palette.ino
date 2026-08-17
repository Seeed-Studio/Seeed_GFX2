/**
 * Product: Seeed Studio Round Display for XIAO
 * Display: 1.28 inch, 240x240, GC9A01, capacitive touch (circular bezel)
 * Demo:   All built-in Seeed_GFX named color constants.
 * Wiki:   https://wiki.seeedstudio.com/get_start_round_display/
 *
 * The panel is round and only 240x240, so 24 color blocks plus their names
 * cannot all fit readably on one screen. The palette is therefore shown in two
 * pages of 12 (3 columns x 4 rows) that auto-advance every ~2.5 s.
 *
 * Each page clears the screen first (so no leftover frame from a previous
 * sketch bleeds through) and draws each name below its swatch on the black
 * backdrop in white text, so names stay readable on every color.
 */

#include <Seeed_GFX.h>

Seeed_GFX display(Seeed_Product::XIAO_ROUND_DISPLAY);

// All TFT named colors.
struct NamedColor {
    const char* name;
    uint32_t    color;
};

const NamedColor palette[] = {
    {"BLACK",       TFT_BLACK},
    {"NAVY",        TFT_NAVY},
    {"DARKGREEN",   TFT_DARKGREEN},
    {"DARKCYAN",    TFT_DARKCYAN},
    {"MAROON",      TFT_MAROON},
    {"PURPLE",      TFT_PURPLE},
    {"OLIVE",       TFT_OLIVE},
    {"LIGHTGREY",   TFT_LIGHTGREY},
    {"DARKGREY",    TFT_DARKGREY},
    {"BLUE",        TFT_BLUE},
    {"GREEN",       TFT_GREEN},
    {"CYAN",        TFT_CYAN},
    {"RED",         TFT_RED},
    {"MAGENTA",     TFT_MAGENTA},
    {"YELLOW",      TFT_YELLOW},
    {"WHITE",       TFT_WHITE},
    {"ORANGE",      TFT_ORANGE},
    {"GREENYELLOW", TFT_GREENYELLOW},
    {"PINK",        TFT_PINK},
    {"BROWN",       TFT_BROWN},
    {"GOLD",        TFT_GOLD},
    {"SILVER",      TFT_SILVER},
    {"SKYBLUE",     TFT_SKYBLUE},
    {"VIOLET",      TFT_VIOLET},
};

const int NUM_COLORS = sizeof(palette) / sizeof(palette[0]);
const int COLORS_PER_PAGE = 12;
const int NUM_PAGES = (NUM_COLORS + COLORS_PER_PAGE - 1) / COLORS_PER_PAGE;
const unsigned long PAGE_MS = 2500;

int currentPage = 0;
unsigned long lastPageTime = 0;

// 3 columns x 4 rows = 12 swatches per page. Each cell is 80x56; the swatch is
// 74x40 with the name drawn below it on the black backdrop. A 16 px strip at
// the bottom (y=224..240) holds the page indicator, clear of the last row.
void drawPage(int page) {
    display.fillScreen(TFT_BLACK);

    const int cols = 3;          // 3 columns x 4 rows = 12 swatches per page
    const int cellW = 80;        // 240 / 3
    const int cellH = 56;     // 4 * 56 = 224, leaves 16 px for the indicator
    const int swatchW = 74;   // cellW - 6 (3 px margin each side)
    const int swatchH = 40;   // leaves room for the name below
    const int nameDy  = 42;   // swatch y + 2 -> baseline of the name row

    int start = page * COLORS_PER_PAGE;
    int count = COLORS_PER_PAGE;
    if (start + count > NUM_COLORS) count = NUM_COLORS - start;

    display.setTextColor(TFT_WHITE);
    display.setTextSize(1);
    display.setTextDatum(TC_DATUM);

    for (int i = 0; i < count; i++) {
        int col = i % cols;
        int row = i / cols;
        int x = col * cellW + 3;       // 3 px left margin within the cell
        int y = row * cellH + 4;      // 4 px top margin within the cell

        // Swatch + outline.
        display.fillRect(x, y, swatchW, swatchH, palette[start + i].color);
        display.drawRect(x, y, swatchW, swatchH, TFT_WHITE);

        // Name, centered under the swatch on the black backdrop.
        display.drawString(palette[start + i].name,
                           x + swatchW / 2, y + nameDy);
    }

    // Page indicator at the bottom-center (inside the round bezel).
    char label[8];
    snprintf(label, sizeof(label), "%d/%d", page + 1, NUM_PAGES);
    display.setTextColor(TFT_DARKGREY);
    display.drawString(label, display.width() / 2, 228);

    display.setTextColor(TFT_WHITE);
    display.setTextDatum(TL_DATUM);
}

void setup() {
    Serial.begin(115200);

    if (!display.begin()) {
        Serial.println(display.lastResult().message);
        return;
    }

    drawPage(0);
    lastPageTime = millis();
    Serial.println("Color Palette displayed!");
}

void loop() {
    if (millis() - lastPageTime >= PAGE_MS) {
        lastPageTime = millis();
        currentPage = (currentPage + 1) % NUM_PAGES;
        drawPage(currentPage);
    }
}
