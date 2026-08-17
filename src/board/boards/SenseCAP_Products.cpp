#include "SenseCAP_Products.h"
#include <Arduino.h>
#include <new>

namespace {
constexpr uint16_t kWatcherOutputMask = 0xDF00;
constexpr uint16_t kWatcherStartupMask =
    static_cast<uint16_t>((1U << 8) | (1U << 9) | (1U << 10) |
                          (1U << 11) | (1U << 12) | (1U << 14) |
                          (1U << 15));

struct SidebandCommand {
    uint8_t command;
    uint8_t count;
    uint8_t data[16];
};

const SidebandCommand kSt7701SSequence[] = {
    {0xFF,5,{0x77,0x01,0x00,0x00,0x10}},
    {0xC0,2,{0x3B,0x00}}, {0xC1,2,{0x0D,0x02}},
    {0xC2,2,{0x31,0x05}}, {0xC7,1,{0x04}}, {0xCD,1,{0x08}},
    {0xB0,16,{0x00,0x11,0x18,0x0E,0x11,0x06,0x07,0x08,
              0x07,0x22,0x04,0x12,0x0F,0xAA,0x31,0x18}},
    {0xB1,16,{0x00,0x11,0x19,0x0E,0x12,0x07,0x08,0x08,
              0x08,0x22,0x04,0x11,0x11,0xA9,0x32,0x18}},
    {0xFF,5,{0x77,0x01,0x00,0x00,0x11}},
    {0xB0,1,{0x60}}, {0xB1,1,{0x32}}, {0xB2,1,{0x07}},
    {0xB3,1,{0x80}}, {0xB5,1,{0x49}}, {0xB7,1,{0x85}},
    {0xB8,1,{0x21}}, {0xC1,1,{0x78}}, {0xC2,1,{0x78}},
    {0xE0,3,{0x00,0x1B,0x02}},
    {0xE1,11,{0x08,0xA0,0x00,0x00,0x07,0xA0,0x00,0x00,
              0x00,0x44,0x44}},
    {0xE2,12,{0x11,0x11,0x44,0x44,0xED,0xA0,0x00,0x00,
              0xEC,0xA0,0x00,0x00}},
    {0xE3,4,{0x00,0x00,0x11,0x11}}, {0xE4,2,{0x44,0x44}},
    {0xE5,16,{0x0A,0xE9,0xD8,0xA0,0x0C,0xEB,0xD8,0xA0,
              0x0E,0xED,0xD8,0xA0,0x10,0xEF,0xD8,0xA0}},
    {0xE6,4,{0x00,0x00,0x11,0x11}}, {0xE7,2,{0x44,0x44}},
    {0xE8,16,{0x09,0xE8,0xD8,0xA0,0x0B,0xEA,0xD8,0xA0,
              0x0D,0xEC,0xD8,0xA0,0x0F,0xEE,0xD8,0xA0}},
    {0xEB,7,{0x02,0x00,0xE4,0xE4,0x88,0x00,0x40}},
    {0xEC,2,{0x3C,0x00}},
    {0xED,16,{0xAB,0x89,0x76,0x54,0x02,0xFF,0xFF,0xFF,
              0xFF,0xFF,0xFF,0x20,0x45,0x67,0x98,0xBA}},
    {0x36,1,{0x10}},
    {0xFF,5,{0x77,0x01,0x00,0x00,0x13}}, {0xE5,1,{0xE4}},
    {0xFF,5,{0x77,0x01,0x00,0x00,0x00}}, {0x3A,1,{0x60}},
    {0x21,0,{0}}, {0x11,0,{0}},
};
}

Board_SenseCAP_Watcher::Board_SenseCAP_Watcher()
    : _expander(Wire, 0x21), _expanderReady(false),
      _controlsReady(false) {}

Board_SenseCAP_Watcher::~Board_SenseCAP_Watcher() {
    endControls();
}

bool Board_SenseCAP_Watcher::begin() {
#if defined(ARDUINO_ARCH_ESP32)
    if (!Wire.begin(47, 48, 400000)) return false;
#else
    Wire.begin();
#endif
    // Hold the unpowered LCD interface low, matching the original Watcher
    // BSP and preventing undefined QSPI levels while the LCD rail rises.
    const uint8_t lcdPins[] = {7, 9, 1, 14, 13, 45, 8, 39, 38};
    for (uint8_t pin : lcdPins) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);
    }
    if (!_expander.begin()) return false;
    _expanderReady = true;

    // Preserve the Watcher BSP's safe startup: configure all P1 outputs low,
    // then enable system and the product's startup rails.
    for (uint8_t pin = 8; pin < 16; ++pin) {
        if (pin == 13) continue; // battery-detect remains an input
        if (!_expander.pinModeOutput(pin, false)) return false;
    }
    if (!_expander.writeMask(kWatcherOutputMask, false)) return false;
    if (!_expander.writePin(10, true)) return false;
    delay(100);
    if (!_expander.writeMask(kWatcherStartupMask, true)) return false;
    delay(50);

    pinMode(8, OUTPUT);
    analogWrite(8, 0);
    return beginControls();
}

IBus* Board_SenseCAP_Watcher::createBus() {
    const Esp32QspiLcdConfig config = {
        7, 9, 1, 14, 13, 45, 40000000,
        static_cast<size_t>(412U * 412U * 2U)
    };
    return new (std::nothrow) Bus_ESP32QSPI(config);
}

void Board_SenseCAP_Watcher::setBacklight(uint8_t brightness) {
    pinMode(8, OUTPUT);
    analogWrite(8, brightness);
}

void Board_SenseCAP_Watcher::powerOn() {
    if (_expanderReady) (void)_expander.writePin(9, true);
}

void Board_SenseCAP_Watcher::powerOff() {
    setBacklight(0);
    if (_expanderReady) (void)_expander.writePin(9, false);
}

bool Board_SenseCAP_Watcher::beginControls() {
    if (_controlsReady) return true;
    if (!_expanderReady || !_expander.pinModeInput(3)) return false;

    pinMode(41, INPUT_PULLUP);
    pinMode(42, INPUT_PULLUP);
    _knobDecoder.reset(digitalRead(41) != LOW, digitalRead(42) != LOW);
#if defined(ARDUINO_ARCH_ESP32)
    attachInterruptArg(digitalPinToInterrupt(41), knobEdgeThunk, this, CHANGE);
    attachInterruptArg(digitalPinToInterrupt(42), knobEdgeThunk, this, CHANGE);
#endif
    _controlsReady = true;
    return true;
}

void Board_SenseCAP_Watcher::endControls() {
    if (!_controlsReady) return;
#if defined(ARDUINO_ARCH_ESP32)
    detachInterrupt(digitalPinToInterrupt(41));
    detachInterrupt(digitalPinToInterrupt(42));
#endif
    _controlsReady = false;
}

void Board_SenseCAP_Watcher::knobEdgeThunk(void* context) {
    if (context) static_cast<Board_SenseCAP_Watcher*>(context)->sampleKnob();
}

void Board_SenseCAP_Watcher::sampleKnob() {
    _knobDecoder.update(digitalRead(41) != LOW, digitalRead(42) != LOW);
}

int16_t Board_SenseCAP_Watcher::readKnobDelta() {
    if (!_controlsReady) return 0;
#if !defined(ARDUINO_ARCH_ESP32)
    sampleKnob();
#endif
    noInterrupts();
    const int16_t result = _knobDecoder.takeDelta();
    interrupts();
    return result;
}

bool Board_SenseCAP_Watcher::readKnobButton(bool& pressed) {
    if (!_controlsReady) return false;
    uint16_t inputs = 0;
    if (!_expander.readInputs(inputs)) return false;
    pressed = (inputs & static_cast<uint16_t>(1U << 3)) == 0;
    return true;
}

Board_SenseCAP_Indicator::Board_SenseCAP_Indicator(
    SenseCAPIndicatorPanel panel)
    : _panel(panel), _expander20(Wire, 0x20),
      _expander39(Wire, 0x39), _expander(nullptr),
      _controllerSleeping(false) {}

bool Board_SenseCAP_Indicator::begin() {
#if defined(ARDUINO_ARCH_ESP32)
    if (!Wire.begin(39, 40, 400000)) return false;
#else
    Wire.begin();
#endif
    if (_expander20.begin()) _expander = &_expander20;
    else if (_expander39.begin()) _expander = &_expander39;
    else return false;

    if (!_expander->pinModeOutput(7, false)) return false;
    delay(5);
    if (!_expander->writePin(7, true)) return false;
    if (!_expander->pinModeOutput(8, true)) return false;
    if (!_expander->pinModeOutput(10, true)) return false;
    pinMode(45, OUTPUT);
    digitalWrite(45, LOW);
    return true;
}

IBus* Board_SenseCAP_Indicator::createBus() {
    Esp32RgbLcdConfig config = {};
    config.width = 480;
    config.height = 480;
    config.frequency = 18000000;
    config.hsync = 16;
    config.vsync = 17;
    config.de = 18;
    config.pclk = 21;
    const int8_t pins[16] = {
        15,14,13,12,11, 10,9,8,7,6,5, 4,3,2,1,0
    };
    for (uint8_t i = 0; i < 16; ++i) config.data[i] = pins[i];
    config.hsyncBackPorch = 50;
    config.hsyncFrontPorch = 10;
    config.hsyncPulseWidth = 8;
    config.vsyncBackPorch = 20;
    config.vsyncFrontPorch = 10;
    config.vsyncPulseWidth = 8;
    config.pclkActiveNegative = false;
    // Arduino Core 3.3.11 enables CONFIG_LCD_RGB_RESTART_IN_VSYNC. On
    // ESP32-S3 the no-bounce restart link remains rooted at framebuffer 0,
    // which prevents an internally allocated second framebuffer from becoming
    // the visible scan source. A small DRAM bounce pair gives VSYNC a stable
    // restart target while the driver selects the PSRAM source only after a
    // complete frame. Ten lines matches Espressif's recommended starting size.
    config.bounceBufferLines = 10;
    config.frameBufferCount = 2;
    config.panelInit = &Board_SenseCAP_Indicator::initializePanelThunk;
    config.panelSetEnabled = &Board_SenseCAP_Indicator::setPanelEnabledThunk;
    config.panelInitContext = this;
    return new (std::nothrow) Bus_ESP32RGB(config);
}

void Board_SenseCAP_Indicator::setBacklight(uint8_t brightness) {
    pinMode(45, OUTPUT);
    // Match Seeed's Arduino example for the two endpoint states. In
    // particular, full brightness must be a plain HIGH rather than depending
    // on an LEDC PWM channel being attachable after RGB initialization.
    if (brightness == 0) digitalWrite(45, LOW);
    else if (brightness == 255) digitalWrite(45, HIGH);
    else analogWrite(45, brightness);
}

bool Board_SenseCAP_Indicator::initializePanelThunk(void* context) {
    return static_cast<Board_SenseCAP_Indicator*>(context)->initializePanel();
}

bool Board_SenseCAP_Indicator::setPanelEnabledThunk(void* context,
                                                    bool enabled) {
    return static_cast<Board_SenseCAP_Indicator*>(context)
        ->setSt7701SEnabled(enabled);
}

bool Board_SenseCAP_Indicator::select(bool active) {
    return _expander && _expander->writePin(4, !active);
}

void Board_SenseCAP_Indicator::sidebandSend9(uint16_t value) {
    for (uint8_t bit = 0; bit < 9; ++bit) {
        digitalWrite(48, (value & 0x100U) ? HIGH : LOW);
        value <<= 1;
        digitalWrite(41, HIGH);
        delayMicroseconds(10);
        digitalWrite(41, LOW);
        delayMicroseconds(10);
    }
}

bool Board_SenseCAP_Indicator::sidebandCommand(uint16_t command) {
    if (!select(true)) return false;
    delayMicroseconds(10);
    digitalWrite(41, LOW);
    delayMicroseconds(10);
    // Preserve the original board sequence: a high command byte frame,
    // followed by a separate low-byte command frame.
    sidebandSend9(static_cast<uint16_t>((command >> 8) & 0xFFU));
    digitalWrite(41, HIGH);
    delayMicroseconds(10);
    digitalWrite(41, LOW);
    if (!select(false)) return false;
    delayMicroseconds(10);
    if (!select(true)) return false;
    delayMicroseconds(10);
    sidebandSend9(static_cast<uint16_t>(command & 0xFFU));
    if (!select(false)) return false;
    delayMicroseconds(10);
    return true;
}

bool Board_SenseCAP_Indicator::sidebandData(uint8_t data) {
    if (!select(true)) return false;
    delayMicroseconds(10);
    digitalWrite(41, LOW);
    delayMicroseconds(10);
    sidebandSend9(static_cast<uint16_t>(0x100U | data));
    digitalWrite(41, HIGH);
    delayMicroseconds(10);
    digitalWrite(41, LOW);
    delayMicroseconds(10);
    if (!select(false)) return false;
    delayMicroseconds(10);
    return true;
}

bool Board_SenseCAP_Indicator::initializeSt7701S() {
    if (!_expander) return false;
    if (!_expander->pinModeOutput(4, true)) return false;
    if (!_expander->pinModeOutput(5, true)) return false;
    pinMode(41, OUTPUT);
    pinMode(48, OUTPUT);
    digitalWrite(41, HIGH);
    digitalWrite(48, HIGH);

    for (size_t i = 0;
         i < sizeof(kSt7701SSequence) / sizeof(kSt7701SSequence[0]); ++i) {
        const SidebandCommand& item = kSt7701SSequence[i];
        if (!sidebandCommand(item.command)) return false;
        for (uint8_t j = 0; j < item.count; ++j) {
            if (!sidebandData(item.data[j])) return false;
        }
        if (item.command == 0xC2 && item.count == 1 && item.data[0] == 0x78) {
            delay(20);
        }
        if (item.command == 0x11 && item.count == 0) delay(120);
    }
    if (!sidebandCommand(0x29)) return false;
    delay(120);
    digitalWrite(41, HIGH);
    digitalWrite(48, HIGH);
    _controllerSleeping = false;
    return select(false);
}

bool Board_SenseCAP_Indicator::setSt7701SEnabled(bool enabled) {
    if (_panel != SenseCAPIndicatorPanel::GX_ST7701S) return true;
    if (!_expander || enabled == !_controllerSleeping) return _expander != nullptr;

    if (enabled) {
        if (!sidebandCommand(0x11)) return false; // Sleep OUT
        delay(120);
        if (!sidebandCommand(0x29)) return false; // Display ON
        delay(120);
        _controllerSleeping = false;
    } else {
        if (!sidebandCommand(0x28)) return false; // Display OFF
        delay(20);
        if (!sidebandCommand(0x10)) return false; // Sleep IN
        delay(120);
        _controllerSleeping = true;
    }
    digitalWrite(41, HIGH);
    digitalWrite(48, HIGH);
    return select(false);
}

bool Board_SenseCAP_Indicator::initializePanel() {
    if (!_expander) return false;
    if (_panel == SenseCAPIndicatorPanel::GX_ST7701S) {
        return initializeSt7701S();
    }
    return _expander->pinModeOutput(4, true) &&
           _expander->pinModeOutput(5, true);
}
