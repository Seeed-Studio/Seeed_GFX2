/**
 * @file   Driver_SSD2677.cpp
 * @brief  SSD2677 ePaper display driver implementation
 *
 * Ported from TFT_Drivers/SSD2677_Defines.h, SSD2677_Init.h, SSD2677_Rotation.h
 * 4-bit color depth (16-level grayscale). Shares init with JD79686B family.
 */

#include "Driver_SSD2677.h"
#include "../../core/Gpio.h"

Driver_SSD2677::Driver_SSD2677(uint16_t w, uint16_t h, int8_t busyPin)
    : _init_width(w), _init_height(h), _busyPin(busyPin) {
    _width = w; _height = h;
}

void Driver_SSD2677::busyWait() {
    if (_busyPin < 0) return;
    (void)waitForReadyPin(_busyPin, true);
}

uint8_t Driver_SSD2677::colorGet(uint8_t color) {
    switch (color) {
        case 0x00: return 0x01;
        case 0x0B: return 0x02;
        case 0x06: return 0x03;
        case 0x0F: return 0x00;
        default:   return 0x00;
    }
}

bool Driver_SSD2677::init(IBus& bus) {
    _bus = &bus;
    if (_busyPin >= 0) pinMode(_busyPin, INPUT);
    hardwareReset(20, 50);
    busyWait();
    if (lastOperationError() != DriverOperationError::None) return false;
    if (applyWaveformProfile(EPaperWaveformMode::Full, _busyPin, true)) {
        setRotation(0);
        return lastOperationError() == DriverOperationError::None;
    }
    if (lastOperationError() != DriverOperationError::None) return false;
    _bus->writeCommand(0x00);
    _bus->writeData(0x2B);
    _bus->writeData(0x29);
    _bus->writeCommand(0x06);
    _bus->writeData(0x0F);
    _bus->writeData(0x8B);
    _bus->writeData(0x93);
    _bus->writeData(0xC1);
    _bus->writeCommand(0x50);
    _bus->writeData(0x37);
    _bus->writeCommand(0x30);
    _bus->writeData(0x08);
    _bus->writeCommand(0x61);
    _bus->writeData(_init_width / 256);
    _bus->writeData(_init_width % 256);
    _bus->writeData(_init_height / 256);
    _bus->writeData(_init_height % 256);
    _bus->writeCommand(0x62);
    _bus->writeData(0x76);
    _bus->writeData(0x76);
    _bus->writeData(0x76);
    _bus->writeData(0x5A);
    _bus->writeData(0x9D);
    _bus->writeData(0x8A);
    _bus->writeData(0x76);
    _bus->writeData(0x62);
    _bus->writeCommand(0x65);
    _bus->writeData(0x00);
    _bus->writeData(0x00);
    _bus->writeData(0x00);
    _bus->writeData(0x00);
    _bus->writeCommand(0xE0);
    _bus->writeData(0x10);
    _bus->writeCommand(0xE7);
    _bus->writeData(0xA4);
    _bus->writeCommand(0xE9);
    _bus->writeData(0x01);
    _bus->writeCommand(0x04); // Power on
    busyWait();
    setRotation(0);
    return lastOperationError() == DriverOperationError::None;
}

void Driver_SSD2677::setRotation(uint8_t m) {
    _rotation = m % 4;
    _bus->writeCommand(0x00);
    switch (_rotation) {
        case 0:
            _bus->writeData(0x2B); _bus->writeData(0x29);
            _width = _init_width; _height = _init_height;
            break;
        case 1:
            _bus->writeData(0x2B); _bus->writeData(0x29);
            _width = _init_height; _height = _init_width;
            break;
        case 2:
            _bus->writeData(0x2B); _bus->writeData(0x29);
            _width = _init_width; _height = _init_height;
            break;
        case 3:
            _bus->writeData(0x2B); _bus->writeData(0x29);
            _width = _init_height; _height = _init_width;
            break;
    }
}

void Driver_SSD2677::invertDisplay(bool) {}
void Driver_SSD2677::displayOn()  { update(); }
void Driver_SSD2677::displayOff() {}
void Driver_SSD2677::setAddrWindow(uint16_t, uint16_t, uint16_t, uint16_t) {}
void Driver_SSD2677::writePixel(uint16_t) {}
void Driver_SSD2677::writePixels(const uint16_t*, size_t) {}
void Driver_SSD2677::writeFill(uint16_t, size_t) {}

void Driver_SSD2677::sleep() {
    _bus->writeCommand(0x02);
    busyWait();
    delay(100);
    _bus->writeCommand(0x07);
    _bus->writeData(0xA5);
}

void Driver_SSD2677::wake() {
    init(*_bus);
}

void Driver_SSD2677::update() {
    _bus->writeCommand(0x12);
    _bus->writeData(0x00);
    busyWait();
}

void Driver_SSD2677::pushColors(const uint8_t* colors, uint16_t w, uint16_t h) {
    uint16_t bytes_per_row = w / 2;
    _bus->writeCommand(0x10);
    for (uint16_t row = 0; row < h; row++) {
        for (uint16_t col = 0; col < bytes_per_row; col += 2) {
            uint8_t b = colors[bytes_per_row * row + col];
            uint8_t c = colors[bytes_per_row * row + col + 1];
            uint8_t temp1 = (b >> 4) & 0x0F;
            uint8_t temp2 =  b       & 0x0F;
            uint8_t temp3 = (c >> 4) & 0x0F;
            uint8_t temp4 =  c       & 0x0F;
            _bus->writeData((colorGet(temp1) << 6) | (colorGet(temp2) << 4) |
                           (colorGet(temp3) << 2) | (colorGet(temp4) << 0));
        }
    }
}

void Driver_SSD2677::pushColorsFlip(const uint8_t* colors, uint16_t w, uint16_t h) {
    uint16_t bytes_per_row = w / 2;
    _bus->writeCommand(0x10);
    for (uint16_t row = 0; row < h; row++) {
        for (uint16_t col = 0; col < bytes_per_row; col += 2) {
            uint8_t b = colors[bytes_per_row * row + (bytes_per_row - 1 - col)];
            uint8_t c = colors[bytes_per_row * row + (bytes_per_row - 1 - col) - 1];
            uint8_t temp1 = (b >> 4) & 0x0F;
            uint8_t temp2 =  b       & 0x0F;
            uint8_t temp3 = (c >> 4) & 0x0F;
            uint8_t temp4 =  c       & 0x0F;
            _bus->writeData((colorGet(temp1) << 6) | (colorGet(temp2) << 4) |
                           (colorGet(temp3) << 2) | (colorGet(temp4) << 0));
        }
    }
}

void Driver_SSD2677::pushOldColors(const uint8_t*, uint16_t, uint16_t) {}
void Driver_SSD2677::pushOldColorsFlip(const uint8_t*, uint16_t, uint16_t) {}
