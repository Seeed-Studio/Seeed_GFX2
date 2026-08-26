/**
 * @file   Driver_SSD2677.cpp
 * @brief  SSD2677 ePaper display driver implementation
 *
 * Aligned with the reTerminal Sticky product firmware
 * (seeed_epaper/driver/ssd2677.c): monochrome 1bpp framebuffer expanded to
 * the controller's 2bpp data format (black=0x03, white=0x00 under
 * PSR {0x2F,0x0E}), RES = 800x680 (680 gate lines scanned, 480 visible),
 * temperature-selected waveform latched before every refresh, and power
 * on/off handled inside the refresh sequence instead of at init.
 * Gray4 follows the firmware's gray path: two-bucket gray4 waveform latch
 * (<=15 C -> 0xF1, otherwise 0xFA) and the 4bpp framebuffer mapped to the
 * controller's 2bpp gray pixel codes; init and the 0x04/0x12/0x02 drive
 * phase are shared with the monochrome refresh.
 */

#include "Driver_SSD2677.h"
#include "../../core/Gpio.h"

Driver_SSD2677::Driver_SSD2677(uint16_t w, uint16_t h, int8_t busyPin)
    : _init_width(w), _init_height(h), _busyPin(busyPin),
      _refreshPowered(false) {
    _width = w; _height = h;
}

void Driver_SSD2677::busyWait() {
    if (_busyPin < 0) return;
    (void)waitForReadyPin(_busyPin, true);
}

// Firmware ssd2677_read_temperature(): command 0x40, wait ready, read one
// byte. Two situations get the 25 °C fallback (the firmware's 21–30 °C
// bucket — full-refresh waveform 0xEE, Gray4 waveform 0xFA):
//   - the bus has no MISO at all (bus !readable, e.g. EE04), so the read
//     cannot happen;
//   - the read comes back 0x00 or >127. Field units exist whose panel SDO
//     is not wired to the Sticky's shared MISO: the bus IS readable, but
//     every read returns 0x00 (probe and temperature alike). Returning raw
//     0 here made Gray4 latch the <=15 °C waveform at room temperature and
//     collapse the intermediate grays to black/white. >127 is outside any
//     sane panel operating range. A genuinely frozen panel at exactly 0 °C
//     is indistinguishable from a dead line; the warm default is the safer
//     trade for a device that otherwise displays nothing wrong.
uint8_t Driver_SSD2677::readPanelTemperature() {
    if (_bus->capabilities().readable) {
        _bus->writeCommand(0x40);
        busyWait();
        _bus->beginRead();
        const uint8_t temp = _bus->readData();
        _bus->endRead();
        if (temp != 0x00 && temp <= 127) return temp;
    }
    return 25;
}

// Firmware ssd2677_latch_full_refresh_waveform(): pick the waveform bucket
// from the panel temperature, then latch it before streaming image data.
void Driver_SSD2677::latchFullWaveform() {
    const uint8_t temp = readPanelTemperature();
    uint8_t waveform;
    if (temp <= 10)      waveform = 0xE8;
    else if (temp <= 20) waveform = 0xEB;
    else if (temp <= 30) waveform = 0xEE;
    else                 waveform = 0xF1;
    _bus->writeCommand(0xE0);
    _bus->writeData(0x12);
    _bus->writeCommand(0xE6);
    _bus->writeData(waveform);
    _bus->writeCommand(0xA5);
    busyWait();
    delay(10);
}

// Firmware ssd2677_gray4_waveform_for_temperature +
// ssd2677_latch_gray4_waveform: Gray4 selects between only two buckets
// (<=15 C -> 0xF1, otherwise -> 0xFA), then latches with the same
// 0xE0/0xE6/0xA5 sequence and trailing 10 ms settle as the monochrome
// latch. readPanelTemperature() already filters dead/invalid reads to
// 25 C, so the >127 branch is only defense in depth.
void Driver_SSD2677::latchGray4Waveform() {
    const uint8_t temp = readPanelTemperature();
    uint8_t waveform;
    if (temp <= 15)       waveform = 0xF1;
    else if (temp <= 127) waveform = 0xFA;
    else                  waveform = 0xF1;
    _bus->writeCommand(0xE0);
    _bus->writeData(0x12);
    _bus->writeCommand(0xE6);
    _bus->writeData(waveform);
    _bus->writeCommand(0xA5);
    busyWait();
    delay(10);
}

// Firmware gray4_nibble_to_ssd2677_pixel: indexes 0-3 pass through
// unchanged. NOTE: gray4 pixel codes are the opposite polarity of the
// monochrome path — nibble 0 (black) -> 0x00, nibble 3 (white) -> 0x03.
// Indexes 4-15 (outside the 4-level set) fold back by their upper pair,
// ported verbatim from the firmware switch.
static inline uint8_t ssd2677Gray4NibbleToPixel(uint8_t nibble) {
    if (nibble <= 3) return nibble;
    switch ((nibble >> 2) & 3) {
        case 0:  return 0x00;
        case 1:  return 0x02;
        case 2:  return 0x01;
        default: return 0x03;
    }
}

// Firmware pack_mono_byte_to_2bpp: one 1bpp byte (bit=1 black, bit=0 white,
// MSB leftmost) becomes two output bytes of 2-bit pairs — black -> 0x03,
// white -> 0x00. out[0] carries pixels 0-3, out[1] pixels 4-7.
static inline void ssd2677ExpandMonoByte(uint8_t mono, uint8_t& o0,
                                         uint8_t& o1) {
    o0 = 0;
    o1 = 0;
    for (uint8_t bit = 0; bit < 8; bit++) {
        const uint8_t pair = ((mono >> (7 - bit)) & 1) ? 0x03 : 0x00;
        if (bit < 4) o0 |= static_cast<uint8_t>(pair << (6 - bit * 2));
        else         o1 |= static_cast<uint8_t>(pair << (14 - bit * 2));
    }
}

static inline uint8_t ssd2677ReverseBits8(uint8_t b) {
    b = static_cast<uint8_t>(((b & 0xF0) >> 4) | ((b & 0x0F) << 4));
    b = static_cast<uint8_t>(((b & 0xCC) >> 2) | ((b & 0x33) << 2));
    b = static_cast<uint8_t>(((b & 0xAA) >> 1) | ((b & 0x55) << 1));
    return b;
}

bool Driver_SSD2677::init(IBus& bus) {
    _bus = &bus;
    _refreshPowered = false;
    if (_busyPin >= 0) pinMode(_busyPin, INPUT);
    hardwareReset(20, 50);
    busyWait();
    if (lastOperationError() != DriverOperationError::None) return false;
    if (applyWaveformProfile(EPaperWaveformMode::Full, _busyPin, true)) {
        // No setRotation(0) here: the firmware never rewrites PSR after
        // init, and touching PSR would drop the aligned {0x2F,0x0E}.
        _rotation = 0;
        _width = _init_width;
        _height = _init_height;
        return lastOperationError() == DriverOperationError::None;
    }
    if (lastOperationError() != DriverOperationError::None) return false;
    // Raw fallback: same registers/order as the firmware ssd2677_init_otp.
    // No PON (0x04) — power-on belongs to the refresh sequence.
    _bus->writeCommand(0x00); // PSR
    _bus->writeData(0x2F);
    _bus->writeData(0x0E);
    busyWait();
    _bus->writeCommand(0x06); // Booster
    _bus->writeData(0x0F);
    _bus->writeData(0x8B);
    _bus->writeData(0x93);
    _bus->writeData(0xC1);
    _bus->writeCommand(0xE7);
    _bus->writeData(0xC1);
    _bus->writeCommand(0x30); // PLL
    _bus->writeData(0x08);
    _bus->writeCommand(0x50); // CDI
    _bus->writeData(0x77);
    _bus->writeCommand(0x62); // Timing
    _bus->writeData(0x76);
    _bus->writeData(0x76);
    _bus->writeData(0x76);
    _bus->writeData(0x5A);
    _bus->writeData(0x9D);
    _bus->writeData(0x8A);
    _bus->writeData(0x76);
    _bus->writeData(0x62);
    _bus->writeCommand(0x61); // RES = 800x680 (680 gate lines scanned)
    _bus->writeData(0x03);
    _bus->writeData(0x20);
    _bus->writeData(0x02);
    _bus->writeData(0xA8);
    _bus->writeCommand(0xE0);
    _bus->writeData(0x10);
    _bus->writeCommand(0x65); // GSST
    _bus->writeData(0x00);
    _bus->writeData(0x00);
    _bus->writeData(0x00);
    _bus->writeData(0x00);
    _bus->writeCommand(0xE9);
    _bus->writeData(0x01);
    busyWait();
    _rotation = 0;
    _width = _init_width;
    _height = _init_height;
    return lastOperationError() == DriverOperationError::None;
}

void Driver_SSD2677::setRotation(uint8_t m) {
    // Logical rotation only: the firmware keeps PSR fixed for all
    // orientations, so no register is rewritten here.
    _rotation = m % 4;
    if (_rotation & 1) {
        _width = _init_height;
        _height = _init_width;
    } else {
        _width = _init_width;
        _height = _init_height;
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
    // Firmware: power off first (if still powered), then deep sleep.
    if (_refreshPowered) {
        _bus->writeCommand(0x02);
        _bus->writeData(0x00);
        busyWait();
        _refreshPowered = false;
    }
    _bus->writeCommand(0x07);
    _bus->writeData(0xA5);
}

void Driver_SSD2677::wake() {
    init(*_bus);
}

void Driver_SSD2677::update() {
    // Firmware refresh: power on (if not already), drive display (0x12),
    // then power off (0x02) after every full refresh. (Only partial mode
    // skips the power-off in the firmware; partial is not implemented.)
    if (!_refreshPowered) {
        _bus->writeCommand(0x04);
        busyWait();
        _refreshPowered = true;
    }
    _bus->writeCommand(0x12);
    _bus->writeData(0x00);
    busyWait();
    _bus->writeCommand(0x02);
    _bus->writeData(0x00);
    busyWait();
    _refreshPowered = false;
}

void Driver_SSD2677::pushColors(const uint8_t* colors, uint16_t w,
                                uint16_t h) {
    if (!colors) return;
    latchFullWaveform();
    const uint16_t bytes_per_row = w / 8;
    const size_t total = static_cast<size_t>(bytes_per_row) * h;
    _bus->writeCommand(0x10); // DTM1: single-plane full refresh
    uint8_t out[512];
    size_t out_len = 0;
    for (size_t i = 0; i < total; i++) {
        uint8_t o0, o1;
        ssd2677ExpandMonoByte(colors[i], o0, o1);
        out[out_len++] = o0;
        out[out_len++] = o1;
        if (out_len == sizeof(out)) {
            _bus->writeData(out, out_len);
            out_len = 0;
        }
    }
    if (out_len) _bus->writeData(out, out_len);
}

void Driver_SSD2677::pushColorsFlip(const uint8_t* colors, uint16_t w,
                                    uint16_t h) {
    if (!colors) return;
    latchFullWaveform();
    const uint16_t bytes_per_row = w / 8;
    _bus->writeCommand(0x10);
    uint8_t out[512];
    size_t out_len = 0;
    for (uint16_t row = 0; row < h; row++) {
        const uint8_t* in_row =
            colors + static_cast<size_t>(bytes_per_row) * row;
        for (uint16_t i = bytes_per_row; i-- > 0;) {
            uint8_t o0, o1;
            ssd2677ExpandMonoByte(ssd2677ReverseBits8(in_row[i]), o0, o1);
            out[out_len++] = o0;
            out[out_len++] = o1;
            if (out_len == sizeof(out)) {
                _bus->writeData(out, out_len);
                out_len = 0;
            }
        }
    }
    if (out_len) _bus->writeData(out, out_len);
}

void Driver_SSD2677::pushOldColors(const uint8_t*, uint16_t, uint16_t) {}
void Driver_SSD2677::pushOldColorsFlip(const uint8_t*, uint16_t, uint16_t) {}

// Firmware update_ssd2677_gray4_region + ssd2677_write_full_framebuffer_gray4:
// unpack the 4bpp framebuffer (high nibble = leftmost pixel), map each index
// through ssd2677Gray4NibbleToPixel, and pack the pairs MSB-first (four
// pixels per byte, shift = 6 - (pos&3)*2) into DTM1 (0x10). Each row is
// packed right-to-left — the firmware mirrors during packing, and
// Driver_SSD1677::pushGrayColors applies the same per-row reversal to the
// shared framebuffer. Sticky mixes both controllers behind one board config
// whose mirror flag flips the 4bpp buffer once before this call; the
// SSD1677 child reverses again inside its driver (net: no flip), so the
// SSD2677 child must reverse too or its gray frame lands mirrored relative
// to its SSD1677 sibling.
void Driver_SSD2677::pushGrayColors(const uint8_t* colors, uint16_t w,
                                    uint16_t h) {
    if (!colors || w < 4) return;
    latchGray4Waveform();
    const uint16_t src_stride = w / 2; // 4bpp: two indexes per byte
    _bus->writeCommand(0x10);          // DTM1: single-plane gray data
    uint8_t out[512];
    size_t out_len = 0;
    for (uint16_t y = 0; y < h; ++y) {
        const uint8_t* row = colors + static_cast<size_t>(src_stride) * y;
        for (uint16_t x = w; x-- > 0;) {
            const uint16_t pos = w - 1 - x; // output pixel index, 0..w-1
            const uint8_t packed = row[x >> 1];
            const uint8_t nibble = ((x & 1) == 0)
                                       ? static_cast<uint8_t>(packed >> 4)
                                       : static_cast<uint8_t>(packed & 0x0F);
            const uint8_t pair = ssd2677Gray4NibbleToPixel(nibble);
            if ((pos & 3) == 0) out[out_len++] = 0;
            out[out_len - 1] |=
                static_cast<uint8_t>(pair << (6 - (pos & 3) * 2));
            // Flush only when the current byte is COMPLETE (pos&3==3).
            // out_len reaches the buffer size at a byte START (pos&3==0),
            // when only pixel 0 of that byte has been written; flushing
            // there would transmit the byte with pixels 1-3 left at 0b00
            // (= black in gray codes) and strand the next three pixels on
            // out[-1]. Field symptom was ~561 fixed black speckles per
            // 800x480 frame (one per 512-byte chunk: 187 chunks x 3 px).
            if ((pos & 3) == 3 && out_len == sizeof(out)) {
                _bus->writeData(out, out_len);
                out_len = 0;
            }
        }
    }
    if (out_len) _bus->writeData(out, out_len);
}
