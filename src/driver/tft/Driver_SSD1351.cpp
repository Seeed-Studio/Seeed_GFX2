/**
 * @file   Driver_SSD1351.cpp
 * @brief  SSD1351 OLED display driver implementation
 *
 * Adapted from TFT_Drivers/SSD1351_Init.h, SSD1351_Rotation.h
 */

#include "Driver_SSD1351.h"

Driver_SSD1351::Driver_SSD1351(uint16_t w, uint16_t h) : _init_width(w), _init_height(h) {
    _width = w; _height = h;
}

bool Driver_SSD1351::init(IBus& bus) {
    _bus = &bus;
    _bus->writeCommand(0xFD); _bus->writeData(0x12); // Unlock
    _bus->writeCommand(0xFD); _bus->writeData(0xB1); // Unlock
    _bus->writeCommand(0xAE); // Display off
    _bus->writeCommand(0xB3); _bus->writeData(0xF1); // Clock divider
    _bus->writeCommand(0xCA); _bus->writeData(static_cast<uint8_t>(_init_height - 1));
    _bus->writeCommand(0xA2); _bus->writeData(0x00); // Display offset
    _bus->writeCommand(0xB5); _bus->writeData(0x00); // GPIO
    _bus->writeCommand(0xAB); _bus->writeData(0x01); // VDD regulator
    _bus->writeCommand(0xB1); _bus->writeData(0x32); // Precharge
    _bus->writeCommand(0xBE); _bus->writeData(0x05); // VCOMH
    _bus->writeCommand(0xA6); // Normal display
    _bus->writeCommand(0xC1); _bus->writeData(0xC8); _bus->writeData(0x80); _bus->writeData(0xC8);
    _bus->writeCommand(0xC7); _bus->writeData(0x0F);
    _bus->writeCommand(0xB4); _bus->writeData(0xA0); _bus->writeData(0xB5); _bus->writeData(0x55);
    _bus->writeCommand(0xB6); _bus->writeData(0x01);
    _bus->writeCommand(0xAF); // Display on
    setRotation(0);
    return true;
}

void Driver_SSD1351::setRotation(uint8_t m) {
    _rotation = m % 4;
    _bus->writeCommand(0xA0);
    switch (_rotation) {
        case 0: _bus->writeData(0x74); _width = _init_width; _height = _init_height; break;
        case 1: _bus->writeData(0x77); _width = _init_height; _height = _init_width; break;
        case 2: _bus->writeData(0x75); _width = _init_width; _height = _init_height; break;
        case 3: _bus->writeData(0x76); _width = _init_height; _height = _init_width; break;
    }
}

void Driver_SSD1351::invertDisplay(bool invert) { _bus->writeCommand(invert ? 0xA7 : 0xA6); }
void Driver_SSD1351::displayOn()  { _bus->writeCommand(0xAF); }
void Driver_SSD1351::displayOff() { _bus->writeCommand(0xAE); }

void Driver_SSD1351::setAddrWindow(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye) {
    _bus->writeCommand(0x15);
    _bus->writeData(xs); _bus->writeData(xe);
    _bus->writeCommand(0x75);
    _bus->writeData(ys); _bus->writeData(ye);
    _bus->writeCommand(0x5C);
}

void Driver_SSD1351::writePixel(uint16_t color) { _bus->writeData16(color); }
void Driver_SSD1351::writePixels(const uint16_t* data, size_t len) { _bus->writePixels(data, len); }
void Driver_SSD1351::writeFill(uint16_t color, size_t len) { _bus->writeRepeat(color, len); }
uint16_t Driver_SSD1351::readPixel(uint16_t x, uint16_t y) {
    return readRgb565Pixel(x, y, 0x5D);
}
void Driver_SSD1351::sleep() { _bus->writeCommand(0xAE); }
void Driver_SSD1351::wake()  { _bus->writeCommand(0xAF); }
