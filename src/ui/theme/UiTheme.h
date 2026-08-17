#ifndef SEEED_UI_THEME_H
#define SEEED_UI_THEME_H

#include <stdint.h>
#include "../UiTypes.h"

struct UiPalette {
    uint16_t background = 0x0000;
    uint16_t surface = 0x18E3;
    uint16_t textPrimary = 0xFFFF;
    uint16_t textSecondary = 0xBDF7;
    uint16_t accent = 0x07FF;
    uint16_t focus = 0xFFE0;
    uint16_t disabled = 0x7BEF;
    uint16_t error = 0xF800;
};

struct UiMetrics {
    int16_t spacingXs = 2;
    int16_t spacingSm = 4;
    int16_t spacingMd = 8;
    int16_t spacingLg = 12;
    int16_t rowHeight = 36;
    int16_t titleHeight = 32;
    int16_t footerHeight = 28;
    int16_t cornerRadius = 4;
    int16_t borderWidth = 2;
    int16_t touchSlop = 6;
    int16_t minTouchTarget = 32;
};

struct UiTheme {
    UiPalette colors;
    UiMetrics metrics;
    uint8_t titleTextSize = 2;
    uint8_t bodyTextSize = 2;
    uint8_t smallTextSize = 1;
};

inline UiTheme uiWioTerminalTheme() { return UiTheme(); }

#endif
