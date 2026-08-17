/**
 * @file   Driver_ST7735.cpp
 * @brief  ST7735 display driver implementation
 *
 * Adapted from TFT_Drivers/ST7735_Init.h, ST7735_Rotation.h
 */

#include "Driver_ST7735.h"

Driver_ST7735::Driver_ST7735(uint16_t w, uint16_t h, Variant v)
    : _init_width(w), _init_height(h), _variant(v)
    , _colstart(0), _rowstart(0), _colstart2(0), _rowstart2(0)
{
    _width  = w;
    _height = h;
}

bool Driver_ST7735::init(IBus& bus) {
    _bus = &bus;
    auto command = [this](uint8_t cmd, const uint8_t* data, size_t len) {
        _bus->writeCommand(cmd);
        if (len) _bus->writeData(data, len);
    };
    if (_variant == INITB) {
        _bus->writeCommand(0x01); delay(50);
        _bus->writeCommand(0x11); delay(500);
        const uint8_t format[] = {0x05}; command(0x3A, format, 1); delay(10);
        const uint8_t frame[] = {0x00, 0x06, 0x03}; command(0xB1, frame, 3); delay(10);
        const uint8_t madctl[] = {0x48}; command(0x36, madctl, 1);
        const uint8_t disset[] = {0x15, 0x02}; command(0xB6, disset, 2);
        const uint8_t invctr[] = {0x00}; command(0xB4, invctr, 1);
        const uint8_t power1[] = {0x02, 0x70}; command(0xC0, power1, 2); delay(10);
        const uint8_t power2[] = {0x05}; command(0xC1, power2, 1);
        const uint8_t power3[] = {0x01, 0x02}; command(0xC2, power3, 2);
        const uint8_t vcom[] = {0x3C, 0x38}; command(0xC5, vcom, 2); delay(10);
        const uint8_t power6[] = {0x11, 0x15}; command(0xFC, power6, 2);
        static const uint8_t gammaPositiveB[] = {
            0x09,0x16,0x09,0x20,0x21,0x1B,0x13,0x19,0x17,0x15,0x1E,0x2B,0x04,0x05,0x02,0x0E};
        static const uint8_t gammaNegativeB[] = {
            0x0B,0x14,0x08,0x1E,0x22,0x1D,0x18,0x1E,0x1B,0x1A,0x24,0x2B,0x06,0x06,0x02,0x0F};
        command(0xE0, gammaPositiveB, sizeof(gammaPositiveB));
        command(0xE1, gammaNegativeB, sizeof(gammaNegativeB)); delay(10);
        _colstart = _rowstart = 0;
        setRotation(0);
        _bus->writeCommand(0x13); delay(10);
        _bus->writeCommand(0x29); delay(500);
        return true;
    }

    _bus->writeCommand(0x01); delay(150); // Software reset
    _bus->writeCommand(0x11); delay(500); // Sleep out
    const uint8_t frame1[] = {0x01, 0x2C, 0x2D}; command(0xB1, frame1, 3);
    command(0xB2, frame1, 3);
    const uint8_t frame3[] = {0x01,0x2C,0x2D,0x01,0x2C,0x2D}; command(0xB3, frame3, 6);
    const uint8_t inv[] = {0x07}; command(0xB4, inv, 1);
    const uint8_t power1[] = {0xA2,0x02,0x84}; command(0xC0, power1, 3);
    const uint8_t power2[] = {0xC5}; command(0xC1, power2, 1);
    const uint8_t power3[] = {0x0A,0x00}; command(0xC2, power3, 2);
    const uint8_t power4[] = {0x8A,0x2A}; command(0xC3, power4, 2);
    const uint8_t power5[] = {0x8A,0xEE}; command(0xC4, power5, 2);
    const uint8_t vcom[] = {0x0E}; command(0xC5, vcom, 1);
    _bus->writeCommand(0x20); // Inversion off
    const uint8_t format[] = {0x05}; command(0x3A, format, 1); // RGB565
    static const uint8_t gammaPositive[] = {
        0x02,0x1C,0x07,0x12,0x37,0x32,0x29,0x2D,0x29,0x25,0x2B,0x39,0x00,0x01,0x03,0x10};
    static const uint8_t gammaNegative[] = {
        0x03,0x1D,0x07,0x06,0x2E,0x2C,0x29,0x2D,0x2E,0x2E,0x37,0x3F,0x00,0x00,0x02,0x10};
    command(0xE0, gammaPositive, sizeof(gammaPositive));
    command(0xE1, gammaNegative, sizeof(gammaNegative));

    _colstart = 0; _rowstart = 0;
    switch (_variant) {
        case GREEN_TAB: case GREEN_TAB2: _colstart = 2; _rowstart = 1; break;
        case GREEN_TAB3: _colstart = 2; _rowstart = 3; break;
        case GREEN_TAB128: _rowstart = 32; break;
        case GREEN_TAB160x80: _colstart = 26; _rowstart = 1; break;
        case RED_TAB160x80: _colstart = 24; break;
        default: break;
    }
    if (_variant == GREEN_TAB160x80) _bus->writeCommand(0x21);
    setRotation(0);
    _bus->writeCommand(0x13); delay(10); // Normal display
    _bus->writeCommand(0x29); delay(100);
    return true;
}

void Driver_ST7735::setRotation(uint8_t m) {
    _rotation = m % 4;
    _bus->writeCommand(0x36); // MADCTL
    switch (_rotation) {
        case 0:
            _bus->writeData(_variant == INITB ? 0x48 :
                            (_variant == GREEN_TAB128 || _variant == GREEN_TAB160x80 ||
                             _variant == RED_TAB160x80) ? 0xCC : 0xC8);
            _width = _init_width; _height = _init_height;
            _colstart2 = _colstart; _rowstart2 = _rowstart;
            break;
        case 1:
            _bus->writeData(_variant == INITB ? 0xE8 : 0xA8);
            _width = _init_height; _height = _init_width;
            if (_variant == GREEN_TAB2)       { _colstart2 = 1;  _rowstart2 = 2; }
            else if (_variant == GREEN_TAB3)  { _colstart2 = 3;  _rowstart2 = 2; }
            else if (_variant == GREEN_TAB128){ _colstart2 = 32; _rowstart2 = 0; }
            else if (_variant == GREEN_TAB160x80){ _colstart2 = 1; _rowstart2 = 26; }
            else if (_variant == RED_TAB160x80)  { _colstart2 = 0; _rowstart2 = 24; }
            else { _colstart2 = _rowstart; _rowstart2 = _colstart; }
            break;
        case 2:
            _bus->writeData(_variant == INITB ? 0x88 : 0x08);
            _width = _init_width; _height = _init_height;
            _colstart2 = _colstart;
            _rowstart2 = (_variant == GREEN_TAB3) ? 1 :
                         (_variant == GREEN_TAB128 ? 0 : _rowstart);
            break;
        case 3:
            _bus->writeData(_variant == INITB ? 0x28 : 0x68);
            _width = _init_height; _height = _init_width;
            if (_variant == GREEN_TAB2 || _variant == GREEN_TAB3) {
                _colstart2 = 1; _rowstart2 = 2;
            } else if (_variant == GREEN_TAB128) {
                _colstart2 = 0; _rowstart2 = 0;
            } else if (_variant == GREEN_TAB160x80) {
                _colstart2 = 1; _rowstart2 = 26;
            } else if (_variant == RED_TAB160x80) {
                _colstart2 = 0; _rowstart2 = 24;
            } else {
                _colstart2 = _rowstart; _rowstart2 = _colstart;
            }
            break;
    }
}

void Driver_ST7735::invertDisplay(bool invert) { _bus->writeCommand(invert ? 0x21 : 0x20); }
void Driver_ST7735::displayOn()  { _bus->writeCommand(0x29); }
void Driver_ST7735::displayOff() { _bus->writeCommand(0x28); }

void Driver_ST7735::setAddrWindow(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye) {
    xs = static_cast<uint16_t>(xs + _colstart2);
    xe = static_cast<uint16_t>(xe + _colstart2);
    ys = static_cast<uint16_t>(ys + _rowstart2);
    ye = static_cast<uint16_t>(ye + _rowstart2);
    _bus->writeCommand(0x2A);
    _bus->writeData(xs >> 8); _bus->writeData(xs & 0xFF);
    _bus->writeData(xe >> 8); _bus->writeData(xe & 0xFF);
    _bus->writeCommand(0x2B);
    _bus->writeData(ys >> 8); _bus->writeData(ys & 0xFF);
    _bus->writeData(ye >> 8); _bus->writeData(ye & 0xFF);
    _bus->writeCommand(0x2C);
}

void Driver_ST7735::writePixel(uint16_t color) { _bus->writeData16(color); }
void Driver_ST7735::writePixels(const uint16_t* data, size_t len) { _bus->writePixels(data, len); }
void Driver_ST7735::writeFill(uint16_t color, size_t len) { _bus->writeRepeat(color, len); }
uint16_t Driver_ST7735::readPixel(uint16_t x, uint16_t y) {
    return readRgb565Pixel(x, y, 0x2E, true);
}
void Driver_ST7735::sleep() { _bus->writeCommand(0x10); delay(5); }
void Driver_ST7735::wake()  { _bus->writeCommand(0x11); delay(120); }
