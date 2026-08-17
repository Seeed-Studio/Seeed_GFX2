#include "DisplayInstance.h"
#include "../core/Board.h"
#include "../core/Bus.h"
#include "../core/Driver.h"
#include "../core/Panel.h"
#include "../core/Touch.h"

DisplayInstance::DisplayInstance()
    : _board(nullptr), _bus(nullptr), _driver(nullptr), _panel(nullptr), _touch(nullptr),
      _initialized(false), _lastResult() {}

DisplayInstance::~DisplayInstance() {
    reset();
}

GfxResult DisplayInstance::adopt(IBoard* board, IBus* bus,
                                 IDriver* driver, IPanel* panel,
                                 ITouch* touch) {
    reset();
    if (!board || !bus || !driver || !panel) {
        delete panel;
        delete touch;
        delete driver;
        delete bus;
        delete board;
        _lastResult = GfxResult(GfxError::InvalidArgument,
                                "display stack contains a null component");
        return _lastResult;
    }
    _board = board;
    _bus = bus;
    _driver = driver;
    _panel = panel;
    _touch = touch;
    _lastResult = GfxResult::success();
    return _lastResult;
}

GfxResult DisplayInstance::begin() {
    if (!_panel) {
        _lastResult = GfxResult(GfxError::NotInitialized,
                                "display instance has no panel");
        return _lastResult;
    }
    if (!_panel->begin()) {
        _lastResult = _panel->lastResult();
        if (_lastResult.ok()) {
            _lastResult = GfxResult(GfxError::PanelInitFailed,
                                    "display stack initialization failed");
        }
        return _lastResult;
    }
    if (_touch && !_touch->begin(*_bus)) {
        (void)_panel->end();
        _lastResult = GfxResult(GfxError::TouchInitFailed,
                                "touch controller initialization failed");
        return _lastResult;
    }
    if (_touch) _touch->setDisplayRotation(_panel->rotation());
    _initialized = true;
    _lastResult = GfxResult::success();
    return _lastResult;
}

GfxResult DisplayInstance::end() {
    if (!_panel) {
        _lastResult = GfxResult(GfxError::NotInitialized,
                                "display instance has no panel");
        return _lastResult;
    }
    if (!_initialized) {
        _lastResult = GfxResult::success();
        return _lastResult;
    }
    _lastResult = _panel->end();
    _initialized = false;
    return _lastResult;
}

void DisplayInstance::reset() {
    if (_initialized && _panel) (void)end();
    delete _touch;
    delete _panel;
    delete _driver;
    delete _bus;
    delete _board;
    _panel = nullptr;
    _touch = nullptr;
    _driver = nullptr;
    _bus = nullptr;
    _board = nullptr;
}
