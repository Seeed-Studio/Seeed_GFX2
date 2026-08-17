/**
 * @file   Driver_SSD1683.cpp
 * @brief  SSD1683 ePaper display driver implementation
 *
 * Ported from TFT_Drivers/SSD1683_Defines.h, SSD1683_Init.h, SSD1683_Rotation.h
 */

#include "Driver_SSD1683.h"
#include "../../core/Gpio.h"

Driver_SSD1683::Driver_SSD1683(uint16_t w, uint16_t h, int8_t busyPin)
    : _init_width(w), _init_height(h), _busyPin(busyPin) {
    _width = w; _height = h;
}

void Driver_SSD1683::busyWait() {
    if (_busyPin < 0) return;
    (void)waitForReadyPin(_busyPin, false);
}

bool Driver_SSD1683::init(IBus& bus) {
    _bus = &bus;
    if (_busyPin >= 0) pinMode(_busyPin, INPUT);
    hardwareReset(10, 10);
    _bus->writeCommand(0x12); // Software reset
    busyWait();
    if (lastOperationError() != DriverOperationError::None) return false;
    if (applyWaveformProfile(EPaperWaveformMode::Full, _busyPin, false)) {
        setRotation(0);
        return lastOperationError() == DriverOperationError::None;
    }
    if (lastOperationError() != DriverOperationError::None) return false;
    _bus->writeCommand(0x01); // Driver output control
    _bus->writeData((_init_height - 1) % 256);
    _bus->writeData((_init_height - 1) / 256);
    _bus->writeData(0x00);
    _bus->writeCommand(0x11); // Data entry mode
    _bus->writeData(0x03);
    _bus->writeCommand(0x44); // RAM X range
    _bus->writeData(0x00);
    _bus->writeData(((_init_width + 7U) / 8U) - 1U);
    _bus->writeCommand(0x45); // RAM Y range
    _bus->writeData(0x00);
    _bus->writeData(0x00);
    _bus->writeData((_init_height - 1) % 256);
    _bus->writeData((_init_height - 1) / 256);
    _bus->writeCommand(0x3C); // Border waveform
    _bus->writeData(0x05);
    _bus->writeCommand(0x21); // Display update control
    _bus->writeData(0x40);
    _bus->writeData(0x00);
    _bus->writeCommand(0x18); // Temperature sensor
    _bus->writeData(0x80);
    _bus->writeCommand(0x4E); // RAM X counter
    _bus->writeData(0x00);
    _bus->writeCommand(0x4F); // RAM Y counter
    _bus->writeData(0x00);
    _bus->writeData(0x00);
    busyWait();
    setRotation(0);
    return lastOperationError() == DriverOperationError::None;
}

void Driver_SSD1683::setRotation(uint8_t m) {
    _rotation = m % 4;
    _bus->writeCommand(0x11);
    switch (_rotation) {
        case 0:
            _bus->writeData(0x03);  // X+ Y+ — matches init profile; pushColors writes top-to-bottom (row0=Y0). Was 0x01 (X+Y-) which scrambled rows (Y wrapped from 0 -> 299 -> 298...).
            _width = _init_width; _height = _init_height;
            break;
        case 1:
            _bus->writeData(0x03);
            _width = _init_height; _height = _init_width;
            break;
        case 2:
            _bus->writeData(0x00);
            _width = _init_width; _height = _init_height;
            break;
        case 3:
            _bus->writeData(0x02);
            _width = _init_height; _height = _init_width;
            break;
    }
}

void Driver_SSD1683::invertDisplay(bool) {}

void Driver_SSD1683::displayOn()  { update(); }
void Driver_SSD1683::displayOff() {}

void Driver_SSD1683::setAddrWindow(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
    _bus->writeCommand(0x44);
    _bus->writeData(x1 >> 3);
    _bus->writeData(x2 >> 3);
    _bus->writeCommand(0x45);
    _bus->writeData(y1 & 0xFF);
    _bus->writeData(y1 >> 8);
    _bus->writeData(y2 & 0xFF);
    _bus->writeData(y2 >> 8);
    _bus->writeCommand(0x4E);
    _bus->writeData(x1 >> 3);
    _bus->writeCommand(0x4F);
    _bus->writeData(y1 & 0xFF);
    _bus->writeData(y1 >> 8);
}

void Driver_SSD1683::writePixel(uint16_t) {}
void Driver_SSD1683::writePixels(const uint16_t*, size_t) {}
void Driver_SSD1683::writeFill(uint16_t, size_t) {}

void Driver_SSD1683::sleep() {
    _bus->writeCommand(0x10);
    _bus->writeData(0x01);
    delay(100);
}

void Driver_SSD1683::wake() {
    init(*_bus);
}

void Driver_SSD1683::update() {
    _bus->writeCommand(0x22);
    _bus->writeData(0xF7);
    _bus->writeCommand(0x20);
    busyWait();
}

void Driver_SSD1683::updateFast() {
    _bus->writeCommand(0x22);
    _bus->writeData(0xCF);
    _bus->writeCommand(0x20);
    busyWait();
}

void Driver_SSD1683::updateGray() {
    updateFast();
}

void Driver_SSD1683::updatePartial() {
    _bus->writeCommand(0x22);
    _bus->writeData(0xFF);
    _bus->writeCommand(0x20);
    busyWait();
}

void Driver_SSD1683::initGray() {
    if (applyWaveformProfile(EPaperWaveformMode::Gray, _busyPin, false)) return;
    if (lastOperationError() != DriverOperationError::None) return;
    _bus->writeCommand(0x21);
    _bus->writeData(0x00);
    _bus->writeData(0x00);
    _bus->writeCommand(0x18);
    _bus->writeData(0x80);
    _bus->writeCommand(0x22);
    _bus->writeData(0xB1);
    _bus->writeCommand(0x20);
    busyWait();
    _bus->writeCommand(0x1A);
    _bus->writeData(0x5A);
    _bus->writeData(0x00);
    _bus->writeCommand(0x22);
    _bus->writeData(0x91);
    _bus->writeCommand(0x20);
    busyWait();
}

void Driver_SSD1683::initPartial() {
    if (applyWaveformProfile(EPaperWaveformMode::Partial, _busyPin, false)) return;
    if (lastOperationError() != DriverOperationError::None) return;
    _bus->writeCommand(0x21);
    _bus->writeData(0x00);
    _bus->writeData(0x00);
    _bus->writeCommand(0x18);
    _bus->writeData(0x80);
    _bus->writeCommand(0x3C);
    _bus->writeData(0x80);
}

void Driver_SSD1683::wakeGray() { wake(); }

void Driver_SSD1683::wakePartial() {
    init(*_bus);
    initPartial();
}

void Driver_SSD1683::pushColors(const uint8_t* data, uint16_t w, uint16_t h) {
    _bus->writeCommand(0x24);
    uint32_t count = (uint32_t)w * h / 8;
    for (uint32_t i = 0; i < count; i++) {
        _bus->writeData(data[i]);
    }
}

void Driver_SSD1683::pushColorsFlip(const uint8_t* data, uint16_t w, uint16_t h) {
    _bus->writeCommand(0x24);
    uint16_t bytes_per_row = w / 8;
    for (uint16_t row = 0; row < h; row++) {
        uint16_t start = row * bytes_per_row;
        for (uint16_t col = 0; col < bytes_per_row; col++) {
            uint8_t b = data[start + (bytes_per_row - 1 - col)];
            b = ((b & 0xF0) >> 4) | ((b & 0x0F) << 4);
            b = ((b & 0xCC) >> 2) | ((b & 0x33) << 2);
            b = ((b & 0xAA) >> 1) | ((b & 0x55) << 1);
            _bus->writeData(b);
        }
    }
}

void Driver_SSD1683::pushOldColors(const uint8_t* data, uint16_t w, uint16_t h) {
    _bus->writeCommand(0x26);
    uint32_t count = (uint32_t)w * h / 8;
    for (uint32_t i = 0; i < count; i++) {
        _bus->writeData(data[i]);
    }
}

void Driver_SSD1683::pushOldColorsFlip(const uint8_t* data, uint16_t w, uint16_t h) {
    _bus->writeCommand(0x26);
    uint16_t bytes_per_row = w / 8;
    for (uint16_t row = 0; row < h; row++) {
        uint16_t start = row * bytes_per_row;
        for (uint16_t col = 0; col < bytes_per_row; col++) {
            uint8_t b = data[start + (bytes_per_row - 1 - col)];
            b = ((b & 0xF0) >> 4) | ((b & 0x0F) << 4);
            b = ((b & 0xCC) >> 2) | ((b & 0x33) << 2);
            b = ((b & 0xAA) >> 1) | ((b & 0x55) << 1);
            _bus->writeData(b);
        }
    }
}

void Driver_SSD1683::pushGrayColors(const uint8_t* colors, uint16_t w, uint16_t h) {
    initGray();
    uint32_t totalBytes = (uint32_t)w * h / 8;
    uint8_t temp1, temp2, temp3;

    _bus->writeCommand(0x24);
    for (uint32_t i = 0; i < totalBytes; i++) {
        uint8_t c0 = colors[i * 4 + 0];
        uint8_t c1 = colors[i * 4 + 1];
        uint8_t c2 = colors[i * 4 + 2];
        uint8_t c3 = colors[i * 4 + 3];
        uint8_t p0 = (c0 >> 4) & 0x03;
        uint8_t p1 = (c0 >> 0) & 0x03;
        uint8_t p2 = (c1 >> 4) & 0x03;
        uint8_t p3 = (c1 >> 0) & 0x03;
        uint8_t p4 = (c2 >> 4) & 0x03;
        uint8_t p5 = (c2 >> 0) & 0x03;
        uint8_t p6 = (c3 >> 4) & 0x03;
        uint8_t p7 = (c3 >> 0) & 0x03;
        uint8_t packed_byte0 = (p0 << 6) | (p1 << 4) | (p2 << 2) | p3;
        uint8_t packed_byte1 = (p4 << 6) | (p5 << 4) | (p6 << 2) | p7;

        temp3 = 0;
        for (uint8_t j = 0; j < 2; j++) {
            temp1 = (j == 0) ? packed_byte0 : packed_byte1;
            for (uint8_t k = 0; k < 4; k++) {
                temp2 = temp1 & 0xC0;
                if (temp2 == 0xC0)
                    temp3 |= 0x01;
                else if (temp2 == 0x00)
                    temp3 |= 0x00;
                else if ((temp2 >= 0x80) && (temp2 < 0xC0))
                    temp3 |= 0x00;
                else if (temp2 == 0x40)
                    temp3 |= 0x01;
                if ((j == 0 && k <= 3) || (j == 1 && k <= 2)) {
                    temp3 <<= 1;
                    temp1 <<= 2;
                }
            }
        }
        _bus->writeData(temp3);
    }

    _bus->writeCommand(0x26);
    for (uint32_t i = 0; i < totalBytes; i++) {
        uint8_t c0 = colors[i * 4 + 0];
        uint8_t c1 = colors[i * 4 + 1];
        uint8_t c2 = colors[i * 4 + 2];
        uint8_t c3 = colors[i * 4 + 3];
        uint8_t p0 = (c0 >> 4) & 0x03;
        uint8_t p1 = (c0 >> 0) & 0x03;
        uint8_t p2 = (c1 >> 4) & 0x03;
        uint8_t p3 = (c1 >> 0) & 0x03;
        uint8_t p4 = (c2 >> 4) & 0x03;
        uint8_t p5 = (c2 >> 0) & 0x03;
        uint8_t p6 = (c3 >> 4) & 0x03;
        uint8_t p7 = (c3 >> 0) & 0x03;
        uint8_t packed_byte0 = (p0 << 6) | (p1 << 4) | (p2 << 2) | p3;
        uint8_t packed_byte1 = (p4 << 6) | (p5 << 4) | (p6 << 2) | p7;

        temp3 = 0;
        for (uint8_t j = 0; j < 2; j++) {
            temp1 = (j == 0) ? packed_byte0 : packed_byte1;
            for (uint8_t k = 0; k < 4; k++) {
                temp2 = temp1 & 0xC0;
                if (temp2 == 0xC0)
                    temp3 |= 0x01;
                else if (temp2 == 0x00)
                    temp3 |= 0x00;
                else if ((temp2 >= 0x80) && (temp2 < 0xC0))
                    temp3 |= 0x01;
                else if (temp2 == 0x40)
                    temp3 |= 0x00;
                if ((j == 0 && k <= 3) || (j == 1 && k <= 2)) {
                    temp3 <<= 1;
                    temp1 <<= 2;
                }
            }
        }
        _bus->writeData(temp3);
    }
}

void Driver_SSD1683::pushGrayColorsFlip(const uint8_t* data, uint16_t w, uint16_t h) {
    pushGrayColors(data, w, h);
}
