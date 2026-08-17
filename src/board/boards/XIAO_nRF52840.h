/** @file XIAO_nRF52840.h @brief Seeed XIAO nRF52840 family board types. */
#ifndef SEEED_GFX_BOARD_XIAO_NRF52840_H
#define SEEED_GFX_BOARD_XIAO_NRF52840_H
#include "../ConfiguredSpiBoard.h"
#include "../configs/XIAO_Board_Configs.h"
using Board_XIAO_nRF52840 = ConfiguredSpiBoard<Config_XIAO_nRF52840_Board>;
using Board_XIAO_nRF52840_Sense = ConfiguredSpiBoard<Config_XIAO_nRF52840_Sense_Board>;
#endif
