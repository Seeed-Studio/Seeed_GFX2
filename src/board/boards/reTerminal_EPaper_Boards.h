/**
 * @file reTerminal_EPaper_Boards.h
 * @brief Public board aliases for reTerminal ePaper products.
 */

#ifndef SEEED_GFX_RETERMINAL_EPAPER_BOARDS_H
#define SEEED_GFX_RETERMINAL_EPAPER_BOARDS_H

#include "../ConfiguredSpiBoard.h"
#include "../configs/reTerminal_EPaper_Board_Configs.h"

/**
 * reTerminal E10xx boards share the display SPI pins with the microSD slot.
 * Deselect and power the slot deterministically before the display bus starts;
 * an inserted card must never be allowed to drive MISO while the panel/TCON is
 * being initialized.
 */
template <typename Config>
class reTerminalEPaperBoard : public ConfiguredSpiBoard<Config> {
public:
    bool begin() override {
        gfxPinModeOutput(Config::sdChipSelectPin());
        gfxDigitalWrite(Config::sdChipSelectPin(), true);
        gfxPinModeInputPullup(Config::sdDetectPin());
        gfxPinModeOutput(Config::sdEnablePin());
        gfxDigitalWrite(Config::sdEnablePin(), true);
        return ConfiguredSpiBoard<Config>::begin();
    }
};

using Board_reTerminal_E1001 =
    reTerminalEPaperBoard<Config_reTerminal_E1001_Board>;
using Board_reTerminal_E1002 =
    reTerminalEPaperBoard<Config_reTerminal_E1002_Board>;
using Board_reTerminal_E1003 =
    reTerminalEPaperBoard<Config_reTerminal_E1003_Board>;
using Board_reTerminal_E1004 =
    reTerminalEPaperBoard<Config_reTerminal_E1004_Board>;
using Board_reTerminal_Sticky =
    ConfiguredSpiBoard<Config_reTerminal_Sticky_Board>;
using Board_reTerminal_Sticky_BWRY =
    ConfiguredSpiBoard<Config_reTerminal_Sticky_BWRY_Board>;

#endif // SEEED_GFX_RETERMINAL_EPAPER_BOARDS_H
