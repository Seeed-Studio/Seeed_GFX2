/** @file XIAO_ESP32S2.h @brief Legacy XIAO ESP32S2-compatible board type. */
#ifndef SEEED_GFX_BOARD_XIAO_ESP32S2_H
#define SEEED_GFX_BOARD_XIAO_ESP32S2_H

#include "../ConfiguredSpiBoard.h"
#include "../configs/XIAO_Board_Configs.h"

using Board_XIAO_ESP32S2 = ConfiguredSpiBoard<Config_XIAO_ESP32S2_Board>;

#endif
