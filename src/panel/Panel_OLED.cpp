/**
 * @file   Panel_OLED.cpp
 * @brief  OLED panel implementation
 */

#include "Panel_OLED.h"

Panel_OLED::Panel_OLED(IDriver& driver, IBus& bus)
    : _driver(driver), _bus(bus), _brightness(255)
    , _initialized(false), _lastResult() {}

Panel_OLED::~Panel_OLED() {
    (void)end();
}

bool Panel_OLED::begin() {
    if (_initialized) {
        _lastResult = GfxResult::success();
        return true;
    }
    if (!_bus.begin()) {
        _lastResult = GfxResult(GfxError::BusInitFailed,
                                "OLED bus initialization failed");
        return false;
    }
    if (!_driver.init(_bus)) {
        _bus.end();
        _lastResult = GfxResult(GfxError::DriverInitFailed,
                                "OLED driver initialization failed");
        return false;
    }
    _initialized = true;
    _lastResult = GfxResult::success();
    return true;
}

GfxResult Panel_OLED::end() {
    if (!_initialized) {
        _lastResult = GfxResult::success();
        return _lastResult;
    }
    sleep();
    _bus.end();
    _initialized = false;
    _lastResult = GfxResult::success();
    return _lastResult;
}

void Panel_OLED::sleep() {
    _driver.sleep();
}

void Panel_OLED::wake() {
    _driver.wake();
}

void Panel_OLED::setBacklight(uint8_t brightness) {
    _brightness = brightness;
    // OLED brightness is typically controlled via contrast/precharge
    // For now, just use display on/off
    if (brightness == 0) {
        _driver.displayOff();
    } else {
        _driver.displayOn();
    }
}
