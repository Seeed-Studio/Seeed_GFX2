/** @file XIAO_RP2040.h @brief Seeed XIAO RP2040 board type. */
#ifndef SEEED_GFX_BOARD_XIAO_RP2040_H
#define SEEED_GFX_BOARD_XIAO_RP2040_H
#include "../ConfiguredSpiBoard.h"
#include "../configs/XIAO_Board_Configs.h"
using Board_XIAO_RP2040 = ConfiguredSpiBoard<Config_XIAO_RP2040_Board>;
#endif
