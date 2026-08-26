#ifndef SEEED_GFX_PRODUCT_H
#define SEEED_GFX_PRODUCT_H

#include <stdint.h>

namespace Seeed_Product {
    enum Product : uint8_t {
        XIAO_ROUND_DISPLAY,
        XIAO_LCD_1INCH47,
        XIAO_ILI9341_240x320,
        XIAO_EPAPER_1INCH54,
        XIAO_EPAPER_2INCH13,
        XIAO_EPAPER_2INCH9,
        // Value 6 was the removed XIAO ePaper 3.97 BW product; reused for
        // the 1.69 inch LCD (ST7789V2, 240x280).
        XIAO_LCD_1INCH69 = 6,
        XIAO_EPAPER_4INCH2 = 7,
        XIAO_EPAPER_4INCH26,
        XIAO_EPAPER_5INCH83,
        XIAO_EPAPER_7INCH5,
        XIAO_EPAPER_10INCH3,
        XIAO_EPAPER_4INCH0_C,
        XIAO_EPAPER_7INCH3_C,
        XIAO_EPAPER_13INCH3_C,
        XIAO_EPAPER_1INCH54_BWRY,
        XIAO_EPAPER_2INCH13_BWRY,
        XIAO_EPAPER_2INCH9_BWRY,
        XIAO_EPAPER_2INCH9_FLEX,  // 2.9" flexible monochrome (128x296 native, UC8151D / GDEW029I6FD) — reuses value 18 (was the removed 3.97 BWRY)
        RETERMINAL_E1001 = 19,
        RETERMINAL_E1002,
        RETERMINAL_E1003,
        RETERMINAL_E1004,
        RETERMINAL_Sticky,
        WIO_TERMINAL_PRODUCT,
        XIAO_EPAPER_7INCH5_JD79686B,
        XIAO_LCD_0INCH96,       // XIAO 0.96" LCD Board (ST7789 80x160)
        XIAO_LCD_1INCH14,       // XIAO 1.14" LCD Board (ST7789 135x240)
        XIAO_LCD_1INCH47_TOUCH, // 1.47" JD9853A + AXS5106L Touch Display
        SENSECAP_WATCHER,
        SENSECAP_INDICATOR_GX,
        SENSECAP_INDICATOR_DX,
        CUSTOM,
    };
}

#endif // SEEED_GFX_PRODUCT_H
