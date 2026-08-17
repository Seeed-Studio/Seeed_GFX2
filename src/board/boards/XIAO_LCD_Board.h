/**
 * @file XIAO_LCD_Board.h
 * @brief Board aliases for LCD products connected to Seeed XIAO MCUs.
 *
 * This header is MCU-independent. It contains the standalone 1.47/1.69 SPI
 * modules, XIAO 0.96/1.14 LCD Boards, XIAO 1.47 Touch Display and the generic
 * XIAO ILI9341 wiring.
 */

#ifndef SEEED_GFX_BOARD_XIAO_LCD_BOARD_H
#define SEEED_GFX_BOARD_XIAO_LCD_BOARD_H

#include "../ConfiguredSpiBoard.h"
#include "../configs/XIAO_LCD_Board_Configs.h"

template <int8_t RST, int8_t BL>
using Board_XIAO_0inch96_LCD =
    ConfiguredSpiBoard<Config_XIAO_0inch96_LCD_Board<RST, BL>>;

template <int8_t RST, int8_t BL>
using Board_XIAO_1inch14_LCD =
    ConfiguredSpiBoard<Config_XIAO_1inch14_LCD_Board<RST, BL>>;

// Existing 0.96/1.14 names remain compatible with sketches that describe
// their XIAO Plus MCU carrier explicitly.
template <int8_t RST, int8_t BL>
using Board_Seeed_0inch96_LCD_Plus = Board_XIAO_0inch96_LCD<RST, BL>;

template <int8_t RST, int8_t BL>
using Board_Seeed_1inch14_LCD_Plus = Board_XIAO_1inch14_LCD<RST, BL>;

template <int8_t RST, int8_t BL, int8_t IRQ = D7>
class Board_XIAO_1inch47_Touch_Display
    : public ConfiguredSpiBoard<
          Config_XIAO_1inch47_Touch_LCD_Board<RST, BL, IRQ>> {
public:
    bool begin() override {
        // The LCD and the on-board microSD socket share SCK/MOSI/MISO.  Keep
        // the card deselected even in sketches that do not use SD, otherwise
        // an inserted card can drive MISO while the display is being started.
        gfxPinModeOutput(D6);
        gfxDigitalWrite(D6, true);
        return ConfiguredSpiBoard<
            Config_XIAO_1inch47_Touch_LCD_Board<RST, BL, IRQ>>::begin();
    }
};

using Board_Seeed_1inch47_LCD =
    ConfiguredSpiBoard<Config_Seeed_1inch47_LCD_Board>;

using Board_Seeed_1inch69_LCD =
    ConfiguredSpiBoard<Config_Seeed_1inch69_LCD_Board>;

using Board_XIAO_ILI9341 =
    ConfiguredSpiBoard<Config_XIAO_ILI9341_LCD_Board>;

#endif // SEEED_GFX_BOARD_XIAO_LCD_BOARD_H
