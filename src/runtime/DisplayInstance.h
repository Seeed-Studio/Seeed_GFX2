#ifndef SEEED_GFX_DISPLAY_INSTANCE_H
#define SEEED_GFX_DISPLAY_INSTANCE_H

#include "../core/Result.h"

class IBoard;
class IBus;
class IDriver;
class IPanel;
class ITouch;

/**
 * Owns one complete display stack. Destruction happens in dependency order:
 * Panel -> Driver -> Bus -> Board.
 */
class DisplayInstance {
public:
    DisplayInstance();
    ~DisplayInstance();

    DisplayInstance(const DisplayInstance&) = delete;
    DisplayInstance& operator=(const DisplayInstance&) = delete;

    GfxResult adopt(IBoard* board, IBus* bus, IDriver* driver, IPanel* panel,
                    ITouch* touch = nullptr);
    GfxResult begin();
    GfxResult end();
    void reset();

    IBoard* board() const { return _board; }
    IBus* bus() const { return _bus; }
    IDriver* driver() const { return _driver; }
    IPanel* panel() const { return _panel; }
    ITouch* touch() const { return _touch; }
    bool hasStack() const { return _panel != nullptr; }
    bool initialized() const { return _initialized; }
    GfxResult lastResult() const { return _lastResult; }

private:
    IBoard* _board;
    IBus* _bus;
    IDriver* _driver;
    IPanel* _panel;
    ITouch* _touch;
    bool _initialized;
    GfxResult _lastResult;
};

#endif // SEEED_GFX_DISPLAY_INSTANCE_H
