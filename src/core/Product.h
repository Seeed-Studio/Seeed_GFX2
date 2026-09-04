#ifndef SEEED_GFX_PRODUCT_H
#define SEEED_GFX_PRODUCT_H

#include <stdint.h>

namespace Seeed_Product {
    enum Product : uint8_t {
        Seeed_Round_Display_XIAO,
        Seeed_LCD_1INCH47,
        Seeed_ILI9341_240x320,
        Seeed_ePaper_1INCH54,
        Seeed_ePaper_2INCH13,
        Seeed_ePaper_2INCH9,
        // Value 6 was the removed Seeed ePaper 3.97 BW product; reused for
        // the 1.69 inch LCD (ST7789V2, 240x280).
        Seeed_LCD_1INCH69 = 6,
        Seeed_ePaper_4INCH2 = 7,
        Seeed_ePaper_4INCH26,
        Seeed_ePaper_5INCH83,
        Seeed_ePaper_7INCH5,
        Seeed_ePaper_10INCH3,
        Seeed_ePaper_4INCH0_C,
        Seeed_ePaper_7INCH3_C,
        Seeed_ePaper_13INCH3_C,
        Seeed_ePaper_1INCH54_BWRY,
        Seeed_ePaper_2INCH13_BWRY,
        Seeed_ePaper_2INCH9_BWRY,
        Seeed_ePaper_2INCH9_FLEX,  // 2.9" flexible monochrome (128x296 native, UC8151D / GDEW029I6FD) — reuses value 18 (was the removed 3.97 BWRY)
        reTerminal_E1001 = 19,
        reTerminal_E1002,
        reTerminal_E1003,
        reTerminal_E1004,
        reTerminal_Sticky,
        Wio_Terminal,
        Seeed_ePaper_7INCH5_JD79686B,
        Seeed_LCD_0INCH96,       // Seeed 0.96" LCD Board (ST7789 80x160)
        Seeed_LCD_1INCH14,       // Seeed 1.14" LCD Board (ST7789 135x240)
        Seeed_LCD_1INCH47_TOUCH, // 1.47" JD9853A + AXS5106L Touch Display
        SenseCAP_Watcher,
        SenseCAP_Indicator_GX,
        SenseCAP_Indicator_DX,
        Seeed_ePaper_7INCH09_C,  // Seeed 7.09" Spectra 6 colorful (GDEB0709E01, default board EE02)
        CUSTOM,
    };
}

#endif // SEEED_GFX_PRODUCT_H
