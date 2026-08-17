/** @file XIAO_EPaper_Boards.h @brief XIAO ePaper adapter board types. */
#ifndef SEEED_GFX_BOARD_XIAO_EPAPER_BOARDS_H
#define SEEED_GFX_BOARD_XIAO_EPAPER_BOARDS_H
#include "../ConfiguredSpiBoard.h"
#include "../configs/XIAO_EPaper_Board_Configs.h"
using Board_XIAO_EPaper_Driver = ConfiguredSpiBoard<Config_XIAO_EPaper_Driver_Board>;

/** XIAO Breakout applies front-view correction only to panel configurations
 *  that explicitly opt in after hardware verification. */
class Board_XIAO_EPaper_Breakout
    : public ConfiguredSpiBoard<Config_XIAO_EPaper_Breakout_Board> {
public:
    static constexpr bool usesPanelConfigBreakoutMirror = true;
    bool panelDisplayHorizontalMirror() const override { return false; }
};

using Board_XIAO_EPaper_EE02 = ConfiguredSpiBoard<Config_XIAO_EPaper_EE02_Board>;
using Board_XIAO_EPaper_EE03 = ConfiguredSpiBoard<Config_XIAO_EPaper_EE03_Board>;
using Board_XIAO_EPaper_EE04 = ConfiguredSpiBoard<Config_XIAO_EPaper_EE04_Board>;
using Board_XIAO_EPaper_EE05 = ConfiguredSpiBoard<Config_XIAO_EPaper_EE05_Board>;
using Board_XIAO_EPaper_EN04 = ConfiguredSpiBoard<Config_XIAO_EPaper_EN04_Board>;
using Board_XIAO_EPaper_EN05 = ConfiguredSpiBoard<Config_XIAO_EPaper_EN05_Board>;
#endif
