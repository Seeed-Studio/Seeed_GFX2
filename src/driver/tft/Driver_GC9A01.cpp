/**
 * @file   Driver_GC9A01.cpp
 * @brief  GC9A01 round display driver implementation
 *
 * Adapted from TFT_Drivers/GC9A01_Init.h, GC9A01_Rotation.h
 */

#include "Driver_GC9A01.h"

Driver_GC9A01::Driver_GC9A01(uint16_t w, uint16_t h)
    : _init_width(w), _init_height(h)
{
    _width  = w;
    _height = h;
}

bool Driver_GC9A01::init(IBus& bus) {
    _bus = &bus;
    // GC9A01 init sequence (from GC9A01_Init.h)
    bus.writeCommand(0xEF);
    bus.writeCommand(0xEB);
    bus.writeData(0x14);

    bus.writeCommand(0xFE);
    bus.writeCommand(0xEF);

    bus.writeCommand(0xEB);
    bus.writeData(0x14);

    bus.writeCommand(0x84);
    bus.writeData(0x40);

    bus.writeCommand(0x85);
    bus.writeData(0xFF);

    bus.writeCommand(0x86);
    bus.writeData(0xFF);

    bus.writeCommand(0x87);
    bus.writeData(0xFF);

    bus.writeCommand(0x88);
    bus.writeData(0x0A);

    bus.writeCommand(0x89);
    bus.writeData(0x21);

    bus.writeCommand(0x8A);
    bus.writeData(0x00);

    bus.writeCommand(0x8B);
    bus.writeData(0x80);

    bus.writeCommand(0x8C);
    bus.writeData(0x01);

    bus.writeCommand(0x8D);
    bus.writeData(0x01);

    bus.writeCommand(0x8E);
    bus.writeData(0xFF);

    bus.writeCommand(0x8F);
    bus.writeData(0xFF);

    bus.writeCommand(0xB6);
    bus.writeData(0x00);
    bus.writeData(0x20);

    bus.writeCommand(GC9A01_COLMOD);
    bus.writeData(0x05);

    bus.writeCommand(0x90);
    bus.writeData(0x08); bus.writeData(0x08);
    bus.writeData(0x08); bus.writeData(0x08);

    bus.writeCommand(0xBD);
    bus.writeData(0x06);

    bus.writeCommand(0xBC);
    bus.writeData(0x00);

    bus.writeCommand(0xFF);
    bus.writeData(0x60); bus.writeData(0x01);
    bus.writeData(0x04);

    bus.writeCommand(0xC3);
    bus.writeData(0x13);
    bus.writeCommand(0xC4);
    bus.writeData(0x13);

    bus.writeCommand(0xC9);
    bus.writeData(0x22);

    bus.writeCommand(0xBE);
    bus.writeData(0x11);

    bus.writeCommand(0xE1);
    bus.writeData(0x10); bus.writeData(0x0E);

    bus.writeCommand(0xDF);
    bus.writeData(0x21); bus.writeData(0x0c);
    bus.writeData(0x02);

    // Gamma settings
    bus.writeCommand(0xF0);
    bus.writeData(0x45); bus.writeData(0x09); bus.writeData(0x08);
    bus.writeData(0x08); bus.writeData(0x26); bus.writeData(0x2A);

    bus.writeCommand(0xF1);
    bus.writeData(0x43); bus.writeData(0x70); bus.writeData(0x72);
    bus.writeData(0x36); bus.writeData(0x37); bus.writeData(0x6F);

    bus.writeCommand(0xF2);
    bus.writeData(0x45); bus.writeData(0x09); bus.writeData(0x08);
    bus.writeData(0x08); bus.writeData(0x26); bus.writeData(0x2A);

    bus.writeCommand(0xF3);
    bus.writeData(0x43); bus.writeData(0x70); bus.writeData(0x72);
    bus.writeData(0x36); bus.writeData(0x37); bus.writeData(0x6F);

    bus.writeCommand(0xED);
    bus.writeData(0x1B); bus.writeData(0x0B);

    bus.writeCommand(0xAE);
    bus.writeData(0x77);

    bus.writeCommand(0xCD);
    bus.writeData(0x63);

    bus.writeCommand(0x70);
    bus.writeData(0x07); bus.writeData(0x07); bus.writeData(0x04);
    bus.writeData(0x0E); bus.writeData(0x0F); bus.writeData(0x09);
    bus.writeData(0x07); bus.writeData(0x08); bus.writeData(0x03);

    bus.writeCommand(0xE8);
    bus.writeData(0x34);

    bus.writeCommand(0x62);
    bus.writeData(0x18); bus.writeData(0x0D); bus.writeData(0x71);
    bus.writeData(0xED); bus.writeData(0x70); bus.writeData(0x70);
    bus.writeData(0x18); bus.writeData(0x0F); bus.writeData(0x71);
    bus.writeData(0xEF); bus.writeData(0x70); bus.writeData(0x70);

    bus.writeCommand(0x63);
    bus.writeData(0x18); bus.writeData(0x11); bus.writeData(0x71);
    bus.writeData(0xF1); bus.writeData(0x70); bus.writeData(0x70);
    bus.writeData(0x18); bus.writeData(0x13); bus.writeData(0x71);
    bus.writeData(0xF3); bus.writeData(0x70); bus.writeData(0x70);

    bus.writeCommand(0x64);
    bus.writeData(0x28); bus.writeData(0x29); bus.writeData(0xF1);
    bus.writeData(0x01); bus.writeData(0xF1); bus.writeData(0x00);
    bus.writeData(0x07);

    bus.writeCommand(0x66);
    bus.writeData(0x3C); bus.writeData(0x00); bus.writeData(0xCD);
    bus.writeData(0x67); bus.writeData(0x45); bus.writeData(0x45);
    bus.writeData(0x10); bus.writeData(0x00); bus.writeData(0x00);
    bus.writeData(0x00);

    bus.writeCommand(0x67);
    bus.writeData(0x00); bus.writeData(0x3C); bus.writeData(0x00);
    bus.writeData(0x00); bus.writeData(0x00); bus.writeData(0x01);
    bus.writeData(0x54); bus.writeData(0x10); bus.writeData(0x32);
    bus.writeData(0x98);

    bus.writeCommand(0x74);
    bus.writeData(0x10); bus.writeData(0x85); bus.writeData(0x80);
    bus.writeData(0x00); bus.writeData(0x00); bus.writeData(0x4E);
    bus.writeData(0x00);

    bus.writeCommand(0x98);
    bus.writeData(0x3e); bus.writeData(0x07);

    bus.writeCommand(0x35);
    bus.writeCommand(0x21);

    bus.writeCommand(GC9A01_SLPOUT);
    delay(120);
    bus.writeCommand(GC9A01_DISPON);
    delay(20);

    setRotation(0);
    return true;
}

void Driver_GC9A01::setRotation(uint8_t m) {
    _rotation = m % 4;
    _bus->writeCommand(GC9A01_MADCTL);

    switch (_rotation) {
        case 0:
            _bus->writeData(TFT_MAD_BGR);
            _width  = _init_width;
            _height = _init_height;
            break;
        case 1:
            _bus->writeData(TFT_MAD_MX | TFT_MAD_MV | TFT_MAD_BGR);
            _width  = _init_height;
            _height = _init_width;
            break;
        case 2:
            _bus->writeData(TFT_MAD_MX | TFT_MAD_MY | TFT_MAD_BGR);
            _width  = _init_width;
            _height = _init_height;
            break;
        case 3:
            _bus->writeData(TFT_MAD_MV | TFT_MAD_MY | TFT_MAD_BGR);
            _width  = _init_height;
            _height = _init_width;
            break;
    }
}

void Driver_GC9A01::invertDisplay(bool invert) {
    _bus->writeCommand(invert ? GC9A01_INVON : GC9A01_INVOFF);
}

void Driver_GC9A01::displayOn()  { _bus->writeCommand(GC9A01_DISPON); }
void Driver_GC9A01::displayOff() { _bus->writeCommand(GC9A01_DISPOFF); }

void Driver_GC9A01::setAddrWindow(uint16_t xs, uint16_t ys,
                                   uint16_t xe, uint16_t ye) {
    _bus->writeCommand(GC9A01_CASET);
    _bus->writeData(xs >> 8); _bus->writeData(xs & 0xFF);
    _bus->writeData(xe >> 8); _bus->writeData(xe & 0xFF);

    _bus->writeCommand(GC9A01_RASET);
    _bus->writeData(ys >> 8); _bus->writeData(ys & 0xFF);
    _bus->writeData(ye >> 8); _bus->writeData(ye & 0xFF);

    _bus->writeCommand(GC9A01_RAMWR);
}

void Driver_GC9A01::writePixel(uint16_t color) { _bus->writeData16(color); }
void Driver_GC9A01::writePixels(const uint16_t* data, size_t len) { _bus->writePixels(data, len); }
void Driver_GC9A01::writeFill(uint16_t color, size_t len) { _bus->writeRepeat(color, len); }
uint16_t Driver_GC9A01::readPixel(uint16_t x, uint16_t y) {
    return readRgb565Pixel(x, y, 0x2E);
}
void Driver_GC9A01::sleep() { _bus->writeCommand(GC9A01_SLPIN); delay(5); }
void Driver_GC9A01::wake()  { _bus->writeCommand(GC9A01_SLPOUT); delay(120); }
