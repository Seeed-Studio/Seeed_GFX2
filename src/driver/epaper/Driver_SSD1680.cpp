/**
 * @file   Driver_SSD1680.cpp
 * @brief  SSD1680 ePaper display driver implementation
 *
 * All macro-based code from the original Seeed_GFX-master library
 * (SSD1680_Defines.h, SSD1680_Init.h, SSD1680_Rotation.h) has been
 * converted into proper C++ class methods.
 */

#include "Driver_SSD1680.h"
#include "../../core/Gpio.h"

// Constructor

Driver_SSD1680::Driver_SSD1680(uint16_t w, uint16_t h)
    : _init_width(w), _init_height(h)
{
    _width  = w;
    _height = h;
}

// Private helpers

void Driver_SSD1680::checkBusy() {
    if (_busy_pin < 0) return;
    (void)waitForReadyPin(_busy_pin, false);
}

void Driver_SSD1680::reset() {
    // Hardware reset via RST pin
    if (_rst_pin >= 0) {
        digitalWrite(_rst_pin, LOW);
        delay(10);
        digitalWrite(_rst_pin, HIGH);
        delay(120);
    }

    checkBusy();

    // Software reset
    _bus->writeCommand(SSD1680_SWRESET);  // 0x12
    checkBusy();
}

// IDriver :: init

bool Driver_SSD1680::init(IBus& bus) {
    _bus = &bus;
    // Configure enable pin if provided
    if (_enable_pin >= 0) {
        pinMode(_enable_pin, OUTPUT);
        digitalWrite(_enable_pin, HIGH);
    }

    // Configure busy pin as input if provided
    if (_busy_pin >= 0) {
        pinMode(_busy_pin, INPUT);
    }

    // Hardware + software reset
    reset();
    if (lastOperationError() != DriverOperationError::None) return false;

    if (applyWaveformProfile(EPaperWaveformMode::Full, _busy_pin, false)) {
        setRotation(0);
        return lastOperationError() == DriverOperationError::None;
    }
    if (lastOperationError() != DriverOperationError::None) return false;

    // ---- Full init sequence (from SSD1680_Init.h) ------------------------

    // Border waveform
    _bus->writeCommand(SSD1680_PTLIN);  // 0x3C
    _bus->writeData(0x05);

    // Driver output control: gate lines = height
    _bus->writeCommand(SSD1680_DRVOUT);  // 0x01
    _bus->writeData((_init_height - 1) & 0xFF);       // low byte
    _bus->writeData(((_init_height - 1) >> 8) & 0xFF); // high byte
    _bus->writeData(0x00);

    // Data entry mode
    _bus->writeCommand(SSD1680_DTM);  // 0x11
    _bus->writeData(0x03);

    // RAM X address range: 0 to (width/8 - 1)
    _bus->writeCommand(SSD1680_SETX);  // 0x44
    _bus->writeData(0x00);
    _bus->writeData(((_init_width + 7U) / 8U) - 1U);

    // RAM Y address range: 0 to (height - 1)
    _bus->writeCommand(SSD1680_SETY);  // 0x45
    _bus->writeData(0x00);
    _bus->writeData(0x00);
    _bus->writeData((_init_height - 1) & 0xFF);
    _bus->writeData(((_init_height - 1) >> 8) & 0xFF);

    // Temperature sensor: internal
    _bus->writeCommand(SSD1680_TEMP);  // 0x18
    _bus->writeData(0x80);

    // RAM X counter start
    _bus->writeCommand(SSD1680_RAMXCNT);  // 0x4E
    _bus->writeData(0x00);

    // RAM Y counter start
    _bus->writeCommand(SSD1680_RAMYCNT);  // 0x4F
    _bus->writeData(0x00);
    _bus->writeData(0x00);

    checkBusy();

    // Apply default rotation
    setRotation(0);

    return lastOperationError() == DriverOperationError::None;
}

// IDriver :: setRotation

void Driver_SSD1680::setRotation(uint8_t m) {
    _rotation = m % 4;

    _bus->writeCommand(SSD1680_DTM);  // 0x11

    switch (_rotation) {
        case 0:  // Portrait, 0 degrees (canonical — Panel_EPaper always calls setRotation(0))
            // X+ Y+ (0x03) — matches the init profile (0x11=0x03) and pushColors'
            // top-to-bottom order (row0=Y0). Was SSD1680_DTM_YDEC_XINC (0x01,
            // X+ Y-): Y decremented from 0, so row0->Y0, row1->Y(h-1),
            // row2->Y(h-2)... rows wrapped/scrambled -> garble. Same fix as
            // SSD1681 1.54" and SSD1683 4.2". Panel_EPaper owns coordinate
            // rotation; the controller stays in its canonical orientation.
            _bus->writeData(0x03);
            _width  = _init_width;
            _height = _init_height;
            break;

        case 2:  // Inverted portrait, 180 degrees
            _bus->writeData(SSD1680_DTM_YINC_XDEC);  // 0x02
            _width  = _init_width;
            _height = _init_height;
            break;

        case 1:  // Landscape 90 degrees — Panel_EPaper handles the swap;
                 // controller stays in canonical orientation (same as rot 0).
        case 3:  // Landscape 270 degrees
        default:
            _bus->writeData(0x03);  // X+ Y+ — same as rot 0; was SSD1680_DTM_YDEC_XINC (0x01, Y-)
            _width  = _init_width;
            _height = _init_height;
            break;
    }
}

// IDriver :: display control

void Driver_SSD1680::invertDisplay(bool /*invert*/) {
    // ePaper panels do not support hardware color inversion.
}

void Driver_SSD1680::displayOn() {
    // ePaper retains the image; no explicit "display on" command.
    // Use update() to refresh the display.
}

void Driver_SSD1680::displayOff() {
    // ePaper retains the image; no explicit "display off" command.
}

// IDriver :: address window

void Driver_SSD1680::setAddrWindow(uint16_t xs, uint16_t ys,
                                    uint16_t xe, uint16_t ye) {
    // Converted from EPD_SET_WINDOW(x1, y1, x2, y2) macro

    // RAM X start/end (in bytes, not pixels)
    _bus->writeCommand(SSD1680_SETX);  // 0x44
    _bus->writeData(xs >> 3);
    _bus->writeData(xe >> 3);

    // RAM Y start/end
    _bus->writeCommand(SSD1680_SETY);  // 0x45
    _bus->writeData(ys & 0xFF);
    _bus->writeData((ys >> 8) & 0xFF);
    _bus->writeData(ye & 0xFF);
    _bus->writeData((ye >> 8) & 0xFF);

    // RAM X counter
    _bus->writeCommand(SSD1680_RAMXCNT);  // 0x4E
    _bus->writeData(xs >> 3);

    // RAM Y counter
    _bus->writeCommand(SSD1680_RAMYCNT);  // 0x4F
    _bus->writeData(ys & 0xFF);
    _bus->writeData((ys >> 8) & 0xFF);
}

// IDriver :: pixel writing (handled by Panel layer for ePaper)

void Driver_SSD1680::writePixel(uint16_t /*color*/) {
    // ePaper writes are done via pushNewColors / pushOldColors.
}

void Driver_SSD1680::writePixels(const uint16_t* /*data*/, size_t /*len*/) {
    // ePaper writes are done via pushNewColors / pushOldColors.
}

void Driver_SSD1680::writeFill(uint16_t /*color*/, size_t /*len*/) {
    // ePaper writes are done via pushNewColors / pushOldColors.
}

// IDriver :: power management

void Driver_SSD1680::sleep() {
    // Converted from EPD_SLEEP() macro
    _bus->writeCommand(SSD1680_SLPIN);  // 0x10
    _bus->writeData(0x01);
    delay(100);
}

void Driver_SSD1680::wake() {
    // Converted from EPD_WAKEUP() macro (which is EPD_INIT())
    // Re-run the full init sequence
    reset();
    if (lastOperationError() != DriverOperationError::None) return;
    if (applyWaveformProfile(EPaperWaveformMode::Full, _busy_pin, false)) {
        setRotation(_rotation);
        return;
    }
    if (lastOperationError() != DriverOperationError::None) return;

    _bus->writeCommand(SSD1680_PTLIN);  // 0x3C
    _bus->writeData(0x05);

    _bus->writeCommand(SSD1680_DRVOUT);  // 0x01
    _bus->writeData((_init_height - 1) & 0xFF);
    _bus->writeData(((_init_height - 1) >> 8) & 0xFF);
    _bus->writeData(0x00);

    _bus->writeCommand(SSD1680_DTM);  // 0x11
    _bus->writeData(0x03);

    _bus->writeCommand(SSD1680_SETX);  // 0x44
    _bus->writeData(0x00);
    _bus->writeData(((_init_width + 7U) / 8U) - 1U);

    _bus->writeCommand(SSD1680_SETY);  // 0x45
    _bus->writeData(0x00);
    _bus->writeData(0x00);
    _bus->writeData((_init_height - 1) & 0xFF);
    _bus->writeData(((_init_height - 1) >> 8) & 0xFF);

    _bus->writeCommand(SSD1680_TEMP);  // 0x18
    _bus->writeData(0x80);

    _bus->writeCommand(SSD1680_RAMXCNT);  // 0x4E
    _bus->writeData(0x00);

    _bus->writeCommand(SSD1680_RAMYCNT);  // 0x4F
    _bus->writeData(0x00);
    _bus->writeData(0x00);

    checkBusy();

    setRotation(_rotation);
}

void Driver_SSD1680::wakePartial() {
    // Converted from EPD_WAKEUP_PARTIAL() macro
    wake();
    initPartial();
}

void Driver_SSD1680::wakeGray() {
    // Converted from EPD_WAKEUP_GRAY() macro (which is EPD_WAKEUP())
    wake();
}

// ePaper-specific :: update methods

void Driver_SSD1680::update() {
    // Converted from EPD_UPDATE() macro
    _bus->writeCommand(SSD1680_DISPCTRL);  // 0x22
    _bus->writeData(0xF7);
    _bus->writeCommand(SSD1680_MASTER);    // 0x20
    checkBusy();
}

void Driver_SSD1680::updateFast() {
    // Converted from EPD_UPDATE_FAST() macro
    _bus->writeCommand(SSD1680_DISPCTRL);  // 0x22
    _bus->writeData(0xC7);
    _bus->writeCommand(SSD1680_MASTER);    // 0x20
    checkBusy();
}

void Driver_SSD1680::updateGray() {
    // Converted from EPD_UPDATE_GRAY() macro (which is EPD_UPDATE_FAST())
    updateFast();
}

void Driver_SSD1680::updatePartial() {
    // Converted from EPD_UPDATE_PARTIAL() macro
    _bus->writeCommand(SSD1680_DISPCTRL);  // 0x22
    _bus->writeData(0xFF);
    _bus->writeCommand(SSD1680_MASTER);    // 0x20
    // Keep the public operation synchronous so Panel can report completion
    // and BUSY timeout consistently with full/fast refreshes.
    checkBusy();
}

// ePaper-specific :: init sub-modes

void Driver_SSD1680::initPartial() {
    if (applyWaveformProfile(EPaperWaveformMode::Partial, _busy_pin, false)) return;
    if (lastOperationError() != DriverOperationError::None) return;
    // Converted from EPD_INIT_PARTIAL() macro
    _bus->writeCommand(SSD1680_TEMP);   // 0x18
    _bus->writeData(0x80);
    _bus->writeCommand(SSD1680_PTLIN);  // 0x3C
    _bus->writeData(0x80);
}

void Driver_SSD1680::initGray() {
    if (applyWaveformProfile(EPaperWaveformMode::Gray, _busy_pin, false)) return;
    if (lastOperationError() != DriverOperationError::None) return;
    // Converted from EPD_INIT_GRAY() macro
    _bus->writeCommand(SSD1680_TEMP);       // 0x18
    _bus->writeData(0x80);

    _bus->writeCommand(SSD1680_DISPCTRL);   // 0x22
    _bus->writeData(0xB1);
    _bus->writeCommand(SSD1680_MASTER);     // 0x20
    checkBusy();

    _bus->writeCommand(SSD1680_LUTOPT);     // 0x1A
    _bus->writeData(0x5A);
    _bus->writeData(0x00);

    _bus->writeCommand(SSD1680_DISPCTRL);   // 0x22
    _bus->writeData(0x91);
    _bus->writeCommand(SSD1680_MASTER);     // 0x20
    checkBusy();
}

// ePaper-specific :: push color data

void Driver_SSD1680::pushNewColors(uint16_t w, uint16_t h,
                                    const uint8_t* colors) {
    // Converted from EPD_PUSH_NEW_COLORS(w, h, colors) macro
    _bus->writeCommand(SSD1680_RAMBW);  // 0x24
    size_t n = (size_t)w * h / 8;
    _bus->writeData(colors, n);
}

void Driver_SSD1680::pushOldColors(uint16_t w, uint16_t h,
                                    const uint8_t* colors) {
    // Converted from EPD_PUSH_OLD_COLORS(w, h, colors) macro
    _bus->writeCommand(SSD1680_RAMRED);  // 0x26
    size_t n = (size_t)w * h / 8;
    _bus->writeData(colors, n);
}

void Driver_SSD1680::pushNewColorsFlip(uint16_t w, uint16_t h,
                                        const uint8_t* colors) {
    // Converted from EPD_PUSH_NEW_COLORS_FLIP(w, h, colors) macro
    // For each row, reverses the byte order and flips the bits within
    // each byte. This corrects for panels where the column wiring is
    // reversed relative to the logical pixel order.

    _bus->writeCommand(SSD1680_RAMBW);  // 0x24

    uint16_t bytes_per_row = w / 8;

    for (uint16_t row = 0; row < h; row++) {
        uint16_t start = row * bytes_per_row;
        for (uint16_t col = 0; col < bytes_per_row; col++) {
            uint8_t b = colors[start + (bytes_per_row - 1 - col)];

            // Reverse the bits within the byte:
            // 1. Swap nibbles
            b = ((b & 0xF0) >> 4) | ((b & 0x0F) << 4);
            // 2. Swap adjacent pairs of bits
            b = ((b & 0xCC) >> 2) | ((b & 0x33) << 2);
            // 3. Swap every other bit
            b = ((b & 0xAA) >> 1) | ((b & 0x55) << 1);

            _bus->writeData(b);
        }
    }
}

void Driver_SSD1680::pushOldColorsFlip(uint16_t w, uint16_t h,
                                        const uint8_t* colors) {
    // Converted from EPD_PUSH_OLD_COLORS_FLIP(w, h, colors) macro
    // Same row-reverse + bit-reverse logic as pushNewColorsFlip,
    // but writes to the Red RAM (0x26) instead.

    _bus->writeCommand(SSD1680_RAMRED);  // 0x26

    uint16_t bytes_per_row = w / 8;

    for (uint16_t row = 0; row < h; row++) {
        uint16_t start = row * bytes_per_row;
        for (uint16_t col = 0; col < bytes_per_row; col++) {
            uint8_t b = colors[start + (bytes_per_row - 1 - col)];

            b = ((b & 0xF0) >> 4) | ((b & 0x0F) << 4);
            b = ((b & 0xCC) >> 2) | ((b & 0x33) << 2);
            b = ((b & 0xAA) >> 1) | ((b & 0x55) << 1);

            _bus->writeData(b);
        }
    }
}

// ePaper-specific :: 4-level grayscale push

void Driver_SSD1680::pushNewGrayColors(uint16_t w, uint16_t h,
                                        const uint8_t* colors) {
    // Converted from EPD_PUSH_NEW_GRAY_COLORS(w, h, colors) macro
    // The SSD1680 achieves 4-level grayscale by using both the B/W RAM (0x24)
    // and Red RAM (0x26) planes. Each pixel is 2 bits in the source buffer:
    //   00 = white  (B/W=0, Red=0)
    //   01 = gray1  (B/W=1, Red=0)
    //   10 = gray2  (B/W=0, Red=1)
    //   11 = black  (B/W=1, Red=1)
    // The input colors[] buffer packs 2 pixels per byte (2 bits each,
    // stored in bits [5:4] and [1:0]). Four input bytes = 8 pixels = 1 output
    // byte per plane. Total input size = 4 * (w * h / 8) = w * h / 2 bytes.

    // Initialize for grayscale operation
    initGray();

    uint32_t total_pixels = (uint32_t)w * h;
    uint32_t total_pixels_div8 = total_pixels / 8;

    uint8_t temp1, temp2, temp3;

    // ---- Plane 1: B/W RAM (0x24) ----
    _bus->writeCommand(SSD1680_RAMBW);  // 0x24

    for (uint32_t i = 0; i < total_pixels_div8; i++) {
        // Read 4 input bytes = 8 pixels
        uint8_t c0 = colors[i * 4 + 0];
        uint8_t c1 = colors[i * 4 + 1];
        uint8_t c2 = colors[i * 4 + 2];
        uint8_t c3 = colors[i * 4 + 3];

        // Extract 8 pixels (2 bits each from bits [5:4] and [1:0])
        uint8_t p0 = (c0 >> 4) & 0x03;
        uint8_t p1 = c0 & 0x03;
        uint8_t p2 = (c1 >> 4) & 0x03;
        uint8_t p3 = c1 & 0x03;
        uint8_t p4 = (c2 >> 4) & 0x03;
        uint8_t p5 = c2 & 0x03;
        uint8_t p6 = (c3 >> 4) & 0x03;
        uint8_t p7 = c3 & 0x03;

        // Pack into 2-bit-per-pixel format (4 pixels per byte)
        uint8_t packed0 = (p0 << 6) | (p1 << 4) | (p2 << 2) | p3;
        uint8_t packed1 = (p4 << 6) | (p5 << 4) | (p6 << 2) | p7;

        // Convert to B/W plane: bit 0 of each 2-bit value -> output bit
        //   11 (0xC0) -> 1,  10 (0x80) -> 0,  01 (0x40) -> 1,  00 -> 0
        temp3 = 0;
        for (uint8_t j = 0; j < 2; j++) {
            temp1 = (j == 0) ? packed0 : packed1;
            for (uint8_t k = 0; k < 4; k++) {
                temp2 = temp1 & 0xC0;
                if (temp2 == 0xC0)
                    temp3 |= 0x01;
                else if (temp2 == 0x00)
                    temp3 |= 0x00;
                else if (temp2 == 0x40)
                    temp3 |= 0x01;
                else  // 0x80
                    temp3 |= 0x00;

                if ((j == 0 && k <= 3) || (j == 1 && k <= 2)) {
                    temp3 <<= 1;
                    temp1 <<= 2;
                }
            }
        }
        _bus->writeData(~temp3);
    }

    // ---- Plane 2: Red RAM (0x26) ----
    _bus->writeCommand(SSD1680_RAMRED);  // 0x26

    for (uint32_t i = 0; i < total_pixels_div8; i++) {
        uint8_t c0 = colors[i * 4 + 0];
        uint8_t c1 = colors[i * 4 + 1];
        uint8_t c2 = colors[i * 4 + 2];
        uint8_t c3 = colors[i * 4 + 3];

        uint8_t p0 = (c0 >> 4) & 0x03;
        uint8_t p1 = c0 & 0x03;
        uint8_t p2 = (c1 >> 4) & 0x03;
        uint8_t p3 = c1 & 0x03;
        uint8_t p4 = (c2 >> 4) & 0x03;
        uint8_t p5 = c2 & 0x03;
        uint8_t p6 = (c3 >> 4) & 0x03;
        uint8_t p7 = c3 & 0x03;

        uint8_t packed0 = (p0 << 6) | (p1 << 4) | (p2 << 2) | p3;
        uint8_t packed1 = (p4 << 6) | (p5 << 4) | (p6 << 2) | p7;

        // Convert to Red plane: bit 1 of each 2-bit value -> output bit
        //   11 (0xC0) -> 1,  10 (0x80) -> 1,  01 (0x40) -> 0,  00 -> 0
        temp3 = 0;
        for (uint8_t j = 0; j < 2; j++) {
            temp1 = (j == 0) ? packed0 : packed1;
            for (uint8_t k = 0; k < 4; k++) {
                temp2 = temp1 & 0xC0;
                if (temp2 == 0xC0)
                    temp3 |= 0x01;
                else if (temp2 == 0x00)
                    temp3 |= 0x00;
                else if (temp2 == 0x40)
                    temp3 |= 0x00;
                else  // 0x80
                    temp3 |= 0x01;

                if ((j == 0 && k <= 3) || (j == 1 && k <= 2)) {
                    temp3 <<= 1;
                    temp1 <<= 2;
                }
            }
        }
        _bus->writeData(~temp3);
    }
}

void Driver_SSD1680::pushNewGrayColorsFlip(uint16_t w, uint16_t h,
                                            const uint8_t* colors) {
    // Converted from EPD_PUSH_NEW_GRAY_COLORS_FLIP(w, h, colors) macro
    // The original macro simply delegates to the non-flip version.
    pushNewGrayColors(w, h, colors);
}
