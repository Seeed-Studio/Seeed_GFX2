/**
 * @file   Driver_ILI9341.cpp
 * @brief  ILI9341 display driver implementation
 *
 * Adapted from TFT_Drivers/ILI9341_Init.h, ILI9341_Rotation.h, ILI9341_Defines.h
 */

#include "Driver_ILI9341.h"

Driver_ILI9341::Driver_ILI9341(uint16_t w, uint16_t h, uint8_t rgbOrder)
    : _init_width(w), _init_height(h), _rgbOrder(rgbOrder)
{
    _width  = w;
    _height = h;
}

bool Driver_ILI9341::init(IBus& bus) {
    _bus = &bus;
    // Keep this sequence in sync with Seeed_GFX's proven ILI9341 setup.
    // The Wio Terminal panel needs the extended power/timing setup below;
    // the abbreviated sequence used previously could accept the first few
    // commands but become unstable during the first full-frame transfer.
    bus.writeCommand(ILI9341_SWRESET);
    delay(150);

    bus.writeCommand(0xEF);
    bus.writeData(0x03); bus.writeData(0x80); bus.writeData(0x02);

    bus.writeCommand(0xCF);
    bus.writeData(0x00); bus.writeData(0xC1); bus.writeData(0x30);

    bus.writeCommand(0xED);
    bus.writeData(0x64); bus.writeData(0x03);
    bus.writeData(0x12); bus.writeData(0x81);

    bus.writeCommand(0xE8);
    bus.writeData(0x85); bus.writeData(0x00); bus.writeData(0x78);

    bus.writeCommand(0xCB);
    bus.writeData(0x39); bus.writeData(0x2C); bus.writeData(0x00);
    bus.writeData(0x34); bus.writeData(0x02);

    bus.writeCommand(0xF7);
    bus.writeData(0x20);

    bus.writeCommand(0xEA);
    bus.writeData(0x00); bus.writeData(0x00);

    // Power control
    bus.writeCommand(ILI9341_PWCTR1);
    bus.writeData(0x23);

    bus.writeCommand(ILI9341_PWCTR2);
    bus.writeData(0x10);

    // VCOM control 1
    bus.writeCommand(ILI9341_VMCTR1);
    bus.writeData(0x3E);
    bus.writeData(0x28);

    // VCOM control 2
    bus.writeCommand(ILI9341_VMCTR2);
    bus.writeData(0x86);

    // Memory access control
    bus.writeCommand(ILI9341_MADCTL);
    bus.writeData(_rgbOrder);

    // Pixel format: 16-bit
    bus.writeCommand(ILI9341_COLMOD);
    bus.writeData(0x55);

    // Frame rate control
    bus.writeCommand(0xB1);
    bus.writeData(0x00);
    bus.writeData(0x13);

    // Display function control
    bus.writeCommand(0xB6);
    bus.writeData(0x08);
    bus.writeData(0x82);
    bus.writeData(0x27);

    // Disable 3-gamma and select gamma curve 1.
    bus.writeCommand(0xF2);
    bus.writeData(0x00);

    bus.writeCommand(0x26);
    bus.writeData(0x01);

    // Gamma control
    bus.writeCommand(ILI9341_GMCTRP1);
    bus.writeData(0x0F); bus.writeData(0x31); bus.writeData(0x2B);
    bus.writeData(0x0C); bus.writeData(0x0E); bus.writeData(0x08);
    bus.writeData(0x4E); bus.writeData(0xF1); bus.writeData(0x37);
    bus.writeData(0x07); bus.writeData(0x10); bus.writeData(0x03);
    bus.writeData(0x0E); bus.writeData(0x09); bus.writeData(0x00);

    bus.writeCommand(ILI9341_GMCTRN1);
    bus.writeData(0x00); bus.writeData(0x0E); bus.writeData(0x14);
    bus.writeData(0x03); bus.writeData(0x11); bus.writeData(0x07);
    bus.writeData(0x31); bus.writeData(0xC1); bus.writeData(0x48);
    bus.writeData(0x08); bus.writeData(0x0F); bus.writeData(0x0C);
    bus.writeData(0x31); bus.writeData(0x36); bus.writeData(0x0F);

    // Sleep out
    bus.writeCommand(ILI9341_SLPOUT);
    delay(120);

    // Display on
    bus.writeCommand(ILI9341_DISPON);

    setRotation(0);
    return true;
}

void Driver_ILI9341::setRotation(uint8_t m) {
    _rotation = m % 4;
    _bus->writeCommand(ILI9341_MADCTL);

    switch (_rotation) {
        case 0:
            _bus->writeData(TFT_MAD_MX | _rgbOrder);
            _width  = _init_width;
            _height = _init_height;
            break;
        case 1:
            _bus->writeData(TFT_MAD_MV | _rgbOrder);
            _width  = _init_height;
            _height = _init_width;
            break;
        case 2:
            _bus->writeData(TFT_MAD_MY | _rgbOrder);
            _width  = _init_width;
            _height = _init_height;
            break;
        case 3:
            _bus->writeData(TFT_MAD_MX | TFT_MAD_MY | TFT_MAD_MV | _rgbOrder);
            _width  = _init_height;
            _height = _init_width;
            break;
    }
}

void Driver_ILI9341::invertDisplay(bool invert) {
    _bus->writeCommand(invert ? ILI9341_INVON : ILI9341_INVOFF);
}

void Driver_ILI9341::displayOn()  { _bus->writeCommand(ILI9341_DISPON); }
void Driver_ILI9341::displayOff() { _bus->writeCommand(ILI9341_DISPOFF); }

void Driver_ILI9341::setAddrWindow(uint16_t xs, uint16_t ys,
                                    uint16_t xe, uint16_t ye) {
    _bus->writeCommand(ILI9341_CASET);
    _bus->writeData(xs >> 8); _bus->writeData(xs & 0xFF);
    _bus->writeData(xe >> 8); _bus->writeData(xe & 0xFF);

    _bus->writeCommand(ILI9341_PASET);
    _bus->writeData(ys >> 8); _bus->writeData(ys & 0xFF);
    _bus->writeData(ye >> 8); _bus->writeData(ye & 0xFF);

    _bus->writeCommand(ILI9341_RAMWR);
}

void Driver_ILI9341::writePixel(uint16_t color)      { _bus->writeData16(color); }
void Driver_ILI9341::writePixels(const uint16_t* data, size_t len) { _bus->writePixels(data, len); }
void Driver_ILI9341::writeFill(uint16_t color, size_t len)        { _bus->writeRepeat(color, len); }
uint16_t Driver_ILI9341::readPixel(uint16_t x, uint16_t y) {
    return readRgb565Pixel(x, y, 0x2E);
}
void Driver_ILI9341::sleep() { _bus->writeCommand(ILI9341_SLPIN); delay(5); }
void Driver_ILI9341::wake()  { _bus->writeCommand(ILI9341_SLPOUT); delay(120); }
