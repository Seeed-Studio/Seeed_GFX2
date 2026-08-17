/**
 * @file   Driver_ST7789.cpp
 * @brief  ST7789 display driver implementation
 *
 * Adapted from TFT_Drivers/ST7789_Init.h, ST7789_Rotation.h, ST7789_Defines.h
 */

#include "Driver_ST7789.h"

Driver_ST7789::Driver_ST7789(uint16_t w, uint16_t h, uint8_t rgbOrder)
    : _init_width(w), _init_height(h), _rgbOrder(rgbOrder)
    , _colstart(0), _rowstart(0)
{
    _width  = w;
    _height = h;
}

// Initialization

bool Driver_ST7789::init(IBus& bus) {
    _bus = &bus;

    // The 80x160 ST7789 used by the XIAO 0.96" Display Board needs the
    // controller sequence used by its verified Arduino_GFX example.  The
    // generic path below is the JLX240/TFT_eSPI sequence and works for the
    // other Seeed ST7789 panels, but it omits SWRESET and programs different
    // RAM/power values.
    if (_init_width == 80 && _init_height == 160) {
        bus.writeCommand(ST7789_SWRESET);
        delay(150);

        bus.writeCommand(ST7789_SLPOUT);
        delay(120);

        bus.writeCommand(ST7789_COLMOD);
        bus.writeData(0x55);

        bus.writeCommand(ST7789_MADCTL);
        bus.writeData(_rgbOrder);

        // RAM control: RGB interface disabled, 65K-color SPI data format.
        bus.writeCommand(ST7789_RAMCTRL);
        bus.writeData(0x00);
        bus.writeData(0xF0);

        bus.writeCommand(ST7789_PORCTRL);
        bus.writeData(0x0C);
        bus.writeData(0x0C);
        bus.writeData(0x00);
        bus.writeData(0x33);
        bus.writeData(0x33);

        bus.writeCommand(ST7789_GCTRL);
        bus.writeData(0x35);

        bus.writeCommand(ST7789_VCOMS);
        bus.writeData(0x19);

        bus.writeCommand(ST7789_LCMCTRL);
        bus.writeData(0x2C);

        bus.writeCommand(ST7789_VDVVRHEN);
        bus.writeData(0x01);

        bus.writeCommand(ST7789_VRHS);
        bus.writeData(0x12);

        bus.writeCommand(ST7789_VDVSET);
        bus.writeData(0x20);

        bus.writeCommand(ST7789_FRCTR2);
        bus.writeData(0x0F);

        bus.writeCommand(ST7789_PWCTRL1);
        bus.writeData(0xA4);
        bus.writeData(0xA1);

        static const uint8_t positiveGamma[] = {
            0xF0, 0x09, 0x13, 0x12, 0x12, 0x2B, 0x3C,
            0x44, 0x4B, 0x1B, 0x18, 0x17, 0x1D, 0x21
        };
        bus.writeCommand(ST7789_PVGAMCTRL);
        bus.writeData(positiveGamma, sizeof(positiveGamma));

        static const uint8_t negativeGamma[] = {
            0xF0, 0x09, 0x13, 0x0C, 0x0D, 0x27, 0x3B,
            0x44, 0x4D, 0x0B, 0x17, 0x17, 0x1D, 0x21
        };
        bus.writeCommand(ST7789_NVGAMCTRL);
        bus.writeData(negativeGamma, sizeof(negativeGamma));

        bus.writeCommand(ST7789_NORON);
        delay(10);
        bus.writeCommand(ST7789_DISPON);

        // Arduino_GFX's IPS=true initialization finishes in INVON.  The
        // panel config subsequently applies its verified final INVOFF state.
        bus.writeCommand(ST7789_INVON);

        setRotation(0);
        return true;
    }

    // Sleep out
    bus.writeCommand(ST7789_SLPOUT);
    delay(120);

    // Normal display mode on
    bus.writeCommand(ST7789_NORON);

    // Memory data access control (color order)
    bus.writeCommand(ST7789_MADCTL);
    bus.writeData(_rgbOrder);

    // JLX240 display datasheet settings
    bus.writeCommand(0xB6);
    bus.writeData(0x0A);
    bus.writeData(0x82);

    // RAM control - 5 to 6-bit conversion
    bus.writeCommand(ST7789_RAMCTRL);
    bus.writeData(0x00);
    bus.writeData(0xE0);

    // Color mode: 16-bit (65K colors)
    bus.writeCommand(ST7789_COLMOD);
    bus.writeData(0x55);
    delay(10);

    // Porch control
    bus.writeCommand(ST7789_PORCTRL);
    bus.writeData(0x0c);
    bus.writeData(0x0c);
    bus.writeData(0x00);
    bus.writeData(0x33);
    bus.writeData(0x33);

    // Gate control
    bus.writeCommand(ST7789_GCTRL);
    bus.writeData(0x35);

    // VCOMS setting
    bus.writeCommand(ST7789_VCOMS);
    bus.writeData(0x28);

    // LCM control
    bus.writeCommand(ST7789_LCMCTRL);
    bus.writeData(0x0C);

    // VDV and VRH command enable
    bus.writeCommand(ST7789_VDVVRHEN);
    bus.writeData(0x01);
    bus.writeData(0xFF);

    // VRH set
    bus.writeCommand(ST7789_VRHS);
    bus.writeData(0x10);

    // VDV set
    bus.writeCommand(ST7789_VDVSET);
    bus.writeData(0x20);

    // Frame rate control
    bus.writeCommand(ST7789_FRCTR2);
    bus.writeData(0x0f);

    // Power control
    bus.writeCommand(ST7789_PWCTRL1);
    bus.writeData(0xa4);
    bus.writeData(0xa1);

    // Positive gamma correction
    bus.writeCommand(ST7789_PVGAMCTRL);
    bus.writeData(0xd0); bus.writeData(0x00); bus.writeData(0x02);
    bus.writeData(0x07); bus.writeData(0x0a); bus.writeData(0x28);
    bus.writeData(0x32); bus.writeData(0x44); bus.writeData(0x42);
    bus.writeData(0x06); bus.writeData(0x0e); bus.writeData(0x12);
    bus.writeData(0x14); bus.writeData(0x17);

    // Negative gamma correction
    bus.writeCommand(ST7789_NVGAMCTRL);
    bus.writeData(0xd0); bus.writeData(0x00); bus.writeData(0x02);
    bus.writeData(0x07); bus.writeData(0x0a); bus.writeData(0x28);
    bus.writeData(0x31); bus.writeData(0x54); bus.writeData(0x47);
    bus.writeData(0x0e); bus.writeData(0x1c); bus.writeData(0x17);
    bus.writeData(0x1b); bus.writeData(0x1e);

    // Inversion on
    bus.writeCommand(ST7789_INVON);

    // Set default address window
    bus.writeCommand(ST7789_CASET);
    bus.writeData(0x00); bus.writeData(0x00);
    bus.writeData(0x00); bus.writeData((uint8_t)(_init_width - 1));

    bus.writeCommand(ST7789_RASET);
    bus.writeData(0x00); bus.writeData(0x00);
    bus.writeData((uint8_t)((_init_height - 1) >> 8));
    bus.writeData((uint8_t)((_init_height - 1) & 0xFF));

    delay(120);

    // Display on
    bus.writeCommand(ST7789_DISPON);
    delay(120);

    setRotation(0);
    return true;
}

// Rotation

void Driver_ST7789::setRotation(uint8_t m) {
    _rotation = m % 4;
    _bus->writeCommand(ST7789_MADCTL);

    switch (_rotation) {
        case 0: // Portrait
            if (_init_width == 135)      { _colstart = 52; _rowstart = 40; }
            else if (_init_height == 280){ _colstart = 0;  _rowstart = 20; }
            else if (_init_width == 172) { _colstart = 34; _rowstart = 0;  }
            else if (_init_width == 170) { _colstart = 35; _rowstart = 0;  }
            else if (_init_width == 80 && _init_height == 160)
                                         { _colstart = 24; _rowstart = 0;  }
            else                         { _colstart = 0;  _rowstart = 0;  }
            _bus->writeData(_rgbOrder);
            _width  = _init_width;
            _height = _init_height;
            break;

        case 1: // Landscape (Portrait + 90)
            if (_init_width == 135)      { _colstart = 40; _rowstart = 53; }
            else if (_init_height == 280){ _colstart = 20; _rowstart = 0;  }
            else if (_init_width == 172) { _colstart = 0;  _rowstart = 34; }
            else if (_init_width == 170) { _colstart = 0;  _rowstart = 35; }
            else if (_init_width == 80 && _init_height == 160)
                                         { _colstart = 0;  _rowstart = 24; }
            else                         { _colstart = 0;  _rowstart = 0;  }
            _bus->writeData(TFT_MAD_MX | TFT_MAD_MV | _rgbOrder);
            _width  = _init_height;
            _height = _init_width;
            break;

        case 2: // Inverted portrait
            if (_init_width == 135)      { _colstart = 53; _rowstart = 40; }
            else if (_init_height == 280){ _colstart = 0;  _rowstart = 20; }
            else if (_init_width == 172) { _colstart = 34; _rowstart = 0;  }
            else if (_init_width == 170) { _colstart = 35; _rowstart = 0;  }
            else if (_init_width == 240 && _init_height == 240)
                                         { _colstart = 0;  _rowstart = 80; }
            else if (_init_width == 80 && _init_height == 160)
                                         { _colstart = 24; _rowstart = 0;  }
            else                         { _colstart = 0;  _rowstart = 0;  }
            _bus->writeData(TFT_MAD_MX | TFT_MAD_MY | _rgbOrder);
            _width  = _init_width;
            _height = _init_height;
            break;

        case 3: // Inverted landscape
            if (_init_width == 135)      { _colstart = 40; _rowstart = 52; }
            else if (_init_height == 280){ _colstart = 20; _rowstart = 0;  }
            else if (_init_width == 172) { _colstart = 0;  _rowstart = 34; }
            else if (_init_width == 170) { _colstart = 0;  _rowstart = 35; }
            else if (_init_width == 240 && _init_height == 240)
                                         { _colstart = 80; _rowstart = 0;  }
            else if (_init_width == 80 && _init_height == 160)
                                         { _colstart = 0;  _rowstart = 24; }
            else                         { _colstart = 0;  _rowstart = 0;  }
            _bus->writeData(TFT_MAD_MV | TFT_MAD_MY | _rgbOrder);
            _width  = _init_height;
            _height = _init_width;
            break;
    }
}

// Display control

void Driver_ST7789::invertDisplay(bool invert) {
    _bus->writeCommand(invert ? ST7789_INVON : ST7789_INVOFF);
}

void Driver_ST7789::displayOn() {
    _bus->writeCommand(ST7789_DISPON);
}

void Driver_ST7789::displayOff() {
    _bus->writeCommand(ST7789_DISPOFF);
}

// Address window

void Driver_ST7789::setAddrWindow(uint16_t xs, uint16_t ys,
                                   uint16_t xe, uint16_t ye) {
    uint16_t x_start = xs + _colstart;
    uint16_t x_end   = xe + _colstart;
    uint16_t y_start = ys + _rowstart;
    uint16_t y_end   = ye + _rowstart;

    _bus->writeCommand(ST7789_CASET);
    _bus->writeData(x_start >> 8);
    _bus->writeData(x_start & 0xFF);
    _bus->writeData(x_end >> 8);
    _bus->writeData(x_end & 0xFF);

    _bus->writeCommand(ST7789_RASET);
    _bus->writeData(y_start >> 8);
    _bus->writeData(y_start & 0xFF);
    _bus->writeData(y_end >> 8);
    _bus->writeData(y_end & 0xFF);

    _bus->writeCommand(ST7789_RAMWR);
}

// Pixel writing

void Driver_ST7789::writePixel(uint16_t color) {
    _bus->writeData16(color);
}

void Driver_ST7789::writePixels(const uint16_t* data, size_t len) {
    _bus->writePixels(data, len);
}

void Driver_ST7789::writeFill(uint16_t color, size_t len) {
    _bus->writeRepeat(color, len);
}

uint16_t Driver_ST7789::readPixel(uint16_t x, uint16_t y) {
    return readRgb565Pixel(x, y, 0x2E);
}

// Power management

void Driver_ST7789::sleep() {
    _bus->writeCommand(ST7789_SLPIN);
    delay(5);
}

void Driver_ST7789::wake() {
    _bus->writeCommand(ST7789_SLPOUT);
    delay(120);
}
