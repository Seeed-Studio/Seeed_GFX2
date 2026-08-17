/**
 * @file   Panel_TFT.cpp
 * @brief  TFT LCD panel implementation
 */

#include "Panel_TFT.h"

Panel_TFT::Panel_TFT(IDriver& driver, IBus& bus, IBoard* board,
                     uint8_t initialRotation)
    : _driver(driver), _bus(bus), _board(board), _backlight(255)
    , _initialized(false), _initialRotation(initialRotation % 4), _lastResult() {}

Panel_TFT::~Panel_TFT() {
    (void)end();
}

bool Panel_TFT::begin() {
    if (_initialized) {
        _lastResult = GfxResult::success();
        return true;
    }

    // Initialize board GPIOs
    if (_board) {
        if (!_board->begin()) {
            _lastResult = GfxResult(GfxError::BoardInitFailed,
                                    "TFT board initialization failed");
            return false;
        }
    }

    // Initialize bus
    if (!_bus.begin()) {
        const char* detail = _bus.lastErrorMessage();
        if (_board) _board->powerOff();
        _lastResult = GfxResult(GfxError::BusInitFailed,
                                detail ? detail
                                       : "TFT bus initialization failed");
        return false;
    }

    // Initialize driver with bus
    if (!_driver.init(_bus)) {
        _bus.end();
        if (_board) _board->powerOff();
        _lastResult = GfxResult(GfxError::DriverInitFailed,
                                "TFT driver initialization failed");
        return false;
    }
    _driver.setRotation(_initialRotation);

    // Turn on backlight
    if (_board) {
        _board->setBacklight(_backlight);
    }

    _initialized = true;
    _lastResult = GfxResult::success();
    return true;
}

GfxResult Panel_TFT::end() {
    if (!_initialized) {
        _lastResult = GfxResult::success();
        return _lastResult;
    }
    sleep();
    _bus.end();
    if (_board) _board->powerOff();
    _initialized = false;
    _lastResult = GfxResult::success();
    return _lastResult;
}

void Panel_TFT::sleep() {
    _driver.sleep();
    // Turn off backlight
    if (_board) {
        _board->setBacklight(0);
    }
}

void Panel_TFT::wake() {
    _driver.wake();
    // Restore backlight
    if (_board) {
        _board->setBacklight(_backlight);
    }
}

void Panel_TFT::setBacklight(uint8_t brightness) {
    _backlight = brightness;
    if (_board) {
        _board->setBacklight(brightness);
    }
}
