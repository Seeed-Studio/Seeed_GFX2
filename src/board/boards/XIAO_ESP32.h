/** @file XIAO_ESP32.h @brief Legacy XIAO ESP32-compatible board type. */
#ifndef SEEED_GFX_BOARD_XIAO_ESP32_H
#define SEEED_GFX_BOARD_XIAO_ESP32_H

#include "../ConfiguredSpiBoard.h"
#include "../configs/XIAO_Board_Configs.h"

using Board_XIAO_ESP32 = ConfiguredSpiBoard<Config_XIAO_ESP32_Board>;

#endif
