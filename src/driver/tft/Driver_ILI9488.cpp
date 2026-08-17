/**
 * @file   Driver_ILI9488.cpp
 * @brief  ILI9488 display driver implementation
 *
 * Adapted from TFT_Drivers/ILI9488_Init.h, ILI9488_Rotation.h
 */

#include "Driver_ILI9488.h"

Driver_ILI9488::Driver_ILI9488(uint16_t w, uint16_t h) : _init_width(w), _init_height(h) {
    _width = w; _height = h;
}

bool Driver_ILI9488::init(IBus& bus) {
    _bus = &bus;
    static const uint8_t gammaPositive[] = {
        0x00,0x03,0x09,0x08,0x16,0x0A,0x3F,0x78,0x4C,0x09,0x0A,0x08,0x16,0x1A,0x0F};
    static const uint8_t gammaNegative[] = {
        0x00,0x16,0x19,0x03,0x0F,0x05,0x32,0x45,0x46,0x04,0x0E,0x0D,0x35,0x37,0x0F};
    auto command = [this](uint8_t cmd, const uint8_t* data, size_t len) {
        _bus->writeCommand(cmd);
        if (len) _bus->writeData(data, len);
    };
    command(0xE0, gammaPositive, sizeof(gammaPositive));
    command(0xE1, gammaNegative, sizeof(gammaNegative));
    const uint8_t power1[] = {0x17, 0x15}; command(0xC0, power1, sizeof(power1));
    const uint8_t power2[] = {0x41}; command(0xC1, power2, sizeof(power2));
    const uint8_t vcom[] = {0x00, 0x12, 0x80}; command(0xC5, vcom, sizeof(vcom));
    const uint8_t madctl[] = {0x48}; command(0x36, madctl, sizeof(madctl));
    const uint8_t format[] = {static_cast<uint8_t>(_bus->isParallel() ? 0x55 : 0x66)};
    command(0x3A, format, sizeof(format));
    const uint8_t interfaceMode[] = {0x00}; command(0xB0, interfaceMode, 1);
    const uint8_t frameRate[] = {0xA0}; command(0xB1, frameRate, 1);
    const uint8_t inversion[] = {0x02}; command(0xB4, inversion, 1);
    const uint8_t function[] = {0x02, 0x02, 0x3B}; command(0xB6, function, 3);
    const uint8_t entry[] = {0xC6}; command(0xB7, entry, 1);
    const uint8_t adjust[] = {0xA9, 0x51, 0x2C, 0x82}; command(0xF7, adjust, 4);
    _bus->writeCommand(0x11); delay(120);
    _bus->writeCommand(0x29); delay(25);
    setRotation(0);
    return true;
}

void Driver_ILI9488::setRotation(uint8_t m) {
    _rotation = m % 4;
    _bus->writeCommand(0x36);
    switch (_rotation) {
        case 0: _bus->writeData(0x48); _width = _init_width; _height = _init_height; break;
        case 1: _bus->writeData(0x28); _width = _init_height; _height = _init_width; break;
        case 2: _bus->writeData(0x98); _width = _init_width; _height = _init_height; break;
        case 3: _bus->writeData(0xF8); _width = _init_height; _height = _init_width; break;
    }
}

void Driver_ILI9488::invertDisplay(bool invert) { _bus->writeCommand(invert ? 0x21 : 0x20); }
void Driver_ILI9488::displayOn()  { _bus->writeCommand(0x29); }
void Driver_ILI9488::displayOff() { _bus->writeCommand(0x28); }

void Driver_ILI9488::setAddrWindow(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye) {
    _bus->writeCommand(0x2A);
    _bus->writeData16(xs); _bus->writeData16(xe);
    _bus->writeCommand(0x2B);
    _bus->writeData16(ys); _bus->writeData16(ye);
    _bus->writeCommand(0x2C);
}

void Driver_ILI9488::writePixel(uint16_t color) {
    if (_bus->isParallel()) { _bus->writeData16(color); return; }
    _bus->writeData(static_cast<uint8_t>((color & 0xF800U) >> 8));
    _bus->writeData(static_cast<uint8_t>((color & 0x07E0U) >> 3));
    _bus->writeData(static_cast<uint8_t>((color & 0x001FU) << 3));
}
void Driver_ILI9488::writePixels(const uint16_t* data, size_t len) {
    if (!data) return;
    if (_bus->isParallel()) { _bus->writePixels(data, len); return; }
    for (size_t i = 0; i < len; ++i) writePixel(data[i]);
}
void Driver_ILI9488::writeFill(uint16_t color, size_t len) {
    if (_bus->isParallel()) { _bus->writeRepeat(color, len); return; }
    while (len--) writePixel(color);
}

uint16_t Driver_ILI9488::readPixel(uint16_t x, uint16_t y) {
    return readRgb565Pixel(x, y, 0x2E);
}
void Driver_ILI9488::sleep() { _bus->writeCommand(0x10); delay(5); }
void Driver_ILI9488::wake()  { _bus->writeCommand(0x11); delay(120); }
