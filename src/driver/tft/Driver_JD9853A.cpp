/**
 * @file Driver_JD9853A.cpp
 * @brief JD9853A compatibility implementation for Seeed's 172x320 panel.
 *
 * XIAO-Display-Board-main contains two known-working paths for this glass:
 * an Arduino_ST7796 initialization followed by MADCTL=0x48, and a later
 * ST7789-compatible path with the same orientation fix. Until a controller
 * vendor register table is published, this driver deliberately reuses the
 * verified ST7789-compatible initialization while owning the JD9853A-specific
 * orientation, offsets and inversion state.
 */

#include "Driver_JD9853A.h"

Driver_JD9853A::Driver_JD9853A(uint16_t w, uint16_t h, uint8_t rgbOrder)
    : Driver_ST7789(w, h, rgbOrder)
    , _nativeWidth(w)
    , _nativeHeight(h)
    , _jdRgbOrder(rgbOrder)
    , _jdColstart(0)
    , _jdRowstart(0) {}

bool Driver_JD9853A::init(IBus& bus) {
    if (!Driver_ST7789::init(bus)) return false;

    // The JD9853A glass used by the XIAO Touch Display has normal colors with
    // inversion disabled. Driver_ST7789 enables inversion during its shared
    // compatibility sequence, so make the final controller state explicit.
    bus.writeCommand(ST7789_INVOFF);
    setRotation(0);
    return true;
}

void Driver_JD9853A::setRotation(uint8_t rotation) {
    _rotation = rotation % 4;
    if (!_bus) return;

    _bus->writeCommand(ST7789_MADCTL);
    switch (_rotation) {
        case 0:
            // Verified XIAO-Display-Board value: MX | BGR = 0x48.
            _bus->writeData(TFT_MAD_MX | _jdRgbOrder);
            _jdColstart = 34;
            _jdRowstart = 0;
            _width = _nativeWidth;
            _height = _nativeHeight;
            break;
        case 1:
            _bus->writeData(TFT_MAD_MV | _jdRgbOrder);
            _jdColstart = 0;
            _jdRowstart = 34;
            _width = _nativeHeight;
            _height = _nativeWidth;
            break;
        case 2:
            _bus->writeData(TFT_MAD_MY | _jdRgbOrder);
            _jdColstart = 34;
            _jdRowstart = 0;
            _width = _nativeWidth;
            _height = _nativeHeight;
            break;
        default:
            _bus->writeData(TFT_MAD_MX | TFT_MAD_MY | TFT_MAD_MV |
                            _jdRgbOrder);
            _jdColstart = 0;
            _jdRowstart = 34;
            _width = _nativeHeight;
            _height = _nativeWidth;
            break;
    }
}

void Driver_JD9853A::setAddrWindow(uint16_t xs, uint16_t ys,
                                    uint16_t xe, uint16_t ye) {
    const uint16_t xStart = static_cast<uint16_t>(xs + _jdColstart);
    const uint16_t xEnd = static_cast<uint16_t>(xe + _jdColstart);
    const uint16_t yStart = static_cast<uint16_t>(ys + _jdRowstart);
    const uint16_t yEnd = static_cast<uint16_t>(ye + _jdRowstart);

    _bus->writeCommand(ST7789_CASET);
    _bus->writeData(static_cast<uint8_t>(xStart >> 8));
    _bus->writeData(static_cast<uint8_t>(xStart));
    _bus->writeData(static_cast<uint8_t>(xEnd >> 8));
    _bus->writeData(static_cast<uint8_t>(xEnd));

    _bus->writeCommand(ST7789_RASET);
    _bus->writeData(static_cast<uint8_t>(yStart >> 8));
    _bus->writeData(static_cast<uint8_t>(yStart));
    _bus->writeData(static_cast<uint8_t>(yEnd >> 8));
    _bus->writeData(static_cast<uint8_t>(yEnd));
    _bus->writeCommand(ST7789_RAMWR);
}
