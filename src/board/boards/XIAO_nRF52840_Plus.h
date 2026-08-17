/** @file XIAO_nRF52840_Plus.h @brief XIAO nRF52840 Plus family types. */
#ifndef SEEED_GFX_BOARD_XIAO_NRF52840_PLUS_H
#define SEEED_GFX_BOARD_XIAO_NRF52840_PLUS_H
#include "../ConfiguredSpiBoard.h"
#include "../configs/XIAO_Board_Configs.h"
using Board_XIAO_nRF52840_Plus = ConfiguredSpiBoard<Config_XIAO_nRF52840_Plus_Board>;
using Board_XIAO_nRF52840_Sense_Plus =
    ConfiguredSpiBoard<Config_XIAO_nRF52840_Sense_Plus_Board>;
#endif
