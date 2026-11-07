#include "Driver_CO5300.h"
#include "../../bus/Bus_ESP32QSPI.h"
#include <Arduino.h>

#if SEEED_GFX_HAS_ESP32S3_LCD
#include "esp_heap_caps.h"
#endif

namespace {
// Transfer chunk, in pixels. 480x250 rows = 240 KB per CS pulse: a full
// 480x480 frame is exactly TWO pulses (~12 ms each), comfortably inside the
// 16.7 ms panel scan — the panel drops neither, so one-shot fills paint clean
// with no second pass. This is the v1 default (proven on hardware).
//   The former whole-frame 480x480 (460 KB) single transfer reported ESP_OK
// but never painted; tiny chunked transfers (e.g. 2048 px, many CS pulses
// per frame) only ever updated a thin strip at the top of the panel.
//   The 50-row chunk (48 KB, the LVGL demo's own buffer height) needs the
// dual-lattice second pass below to repaint dropped slices — enable that with
// CO5300_ENABLE_DUAL_PASS=1 and CO5300_CHUNK_ROWS 50 to trade the 240 KB
// buffer for a double write at 48 KB.
#ifndef CO5300_CHUNK_ROWS
#define CO5300_CHUNK_ROWS 250
#endif
// Compile-time switch for the half-chunk-shifted second pass (see
// kDualPassMultiSliceFills). v1 ships OFF: at 250 rows the single pass is
// already clean, and this pass would only double large-fill time. Set to 1
// when running the cheap 48 KB / 50-row path.
#ifndef CO5300_ENABLE_DUAL_PASS
#define CO5300_ENABLE_DUAL_PASS 1
#endif
// RGB565 big-endian swap (LVGL's RGB565_SWAPPED + SPD2010 parity). Toggle for
// endianness A/B: WHITE 0xFFFF and BLACK 0x0000 are the ONLY colors invariant
// under this swap, so a wrong line feeds back exactly as "white/black clean,
// every real colour garbled or off by one".
#ifndef CO5300_ENABLE_BYTE_SWAP
#define CO5300_ENABLE_BYTE_SWAP 1
#endif
constexpr uint16_t kSwapColor(uint16_t color) {
#if CO5300_ENABLE_BYTE_SWAP
    return static_cast<uint16_t>((color << 8) | (color >> 8));
#else
    return color;
#endif
}
constexpr size_t kTransferChunkPixels = 480 * CO5300_CHUNK_ROWS;

// Multi-slice fills drop some slices at a FIXED lattice (the panel drops a
// pulse depending on its window's starting row, not its timing — inter-pulse
// spacing 0/3/15 ms did not move the bands; a different starting row did). One
// pass over the frame that exactly repeats the lattice drops the same rows and
// GRAM keeps its boot-green forever. The LVGL demo never sees this because it
// repaints every ~16 ms at a drifting lattice. Implemented here as the demo's
// "second chance" done cheaply: writeFill()/writePixels() rewrite the window
// a second time with the slice lattice shifted by half a slice, so every row
// is written by two different-phase pulses (GapProbe T3 proved this paints
// fully at a 48 KB chunk = CO5300_CHUNK_ROWS 50).
constexpr bool kDualPassMultiSliceFills = CO5300_ENABLE_DUAL_PASS != 0;
// MADCTL orientation subgroup cycle (MV/MX/MY only — no color-order bits).
// Index = quarter-turn: a panel whose factory MADCTL is kMadctlOrientations[i]
// lands on kMadctlOrientations[(i + rotation) % 4] at rotation n.  The
// 2.16" square ships at 0xA0 (index 3 -> {0xA0,0x00,0x60,0xC0}); the round
// panels ship at 0x00 (index 0 -> {0x00,0x60,0xC0,0xA0}).
constexpr uint8_t kMadctlOrientations[4] = {0x00, 0x60, 0xC0, 0xA0};

// Per-glass profile.  The three CO5300 glasses share one controller, QSPI
// framing, power path and reset; only resolution, factory MADCTL, the source
// GRAM gap and the init-command set differ (mirrors the ESP-IDF component
// library's amoled_console_panel_profile_t):
//   2.16" square  480x480  MADCTL 0xA0  gap ( 0, 0)  init "simple"
//   2.05" round   368x448  MADCTL 0x00  gap (16, 0)  init "extended"
//   1.75" round   466x466  MADCTL 0x00  gap ( 6, 0)  init "simple"
struct Co5300PanelProfile {
    uint16_t gapX;
    uint16_t gapY;
    uint8_t  madctlFactory;
    bool     extendedInit;
};

constexpr Co5300PanelProfile panelProfileFor(uint16_t w, uint16_t h) {
    if (w == 368 && h == 448) return {16, 0, 0x00, true};   // 2.05" round
    if (w == 466 && h == 466) return {6, 0, 0x00, false};   // 1.75" round
    return {0, 0, 0xA0, false};                             // 2.16" square (default)
}
}

Driver_CO5300::Driver_CO5300(uint16_t width, uint16_t height, uint8_t rgbOrder)
    : _width(width), _height(height), _qspi(nullptr), _transferBuffer(nullptr),
      _transferCapacityPixels(0), _winX0(0), _winY0(0), _winX1(0), _winY1(0),
      _interPulseSpacingUs(0), _pulseNumber(0), _interPulseGapUs(0),
      _pendingX0(0), _pendingX1(0), _pendingY0(0), _pendingRows(0),
      _gapX(0), _gapY(0), _extendedInit(false) {
    IDriver::_width = width;
    IDriver::_height = height;

    // The glass is identified by the configured resolution (the only per-panel
    // data the SFINAE driver factory forwards besides rgbOrder), and its gap /
    // MADCTL / init set follow from that.
    const Co5300PanelProfile p = panelProfileFor(width, height);
    _gapX = p.gapX;
    _gapY = p.gapY;
    _extendedInit = p.extendedInit;

    // Build the MADCTL rotation table from the panel's factory orientation so
    // rotation 0 reproduces board-native framing on every glass.  Any BGR
    // color-order bit rides along unchanged.
    int baseIdx = 0;
    for (int i = 0; i < 4; ++i) {
        if (kMadctlOrientations[i] == p.madctlFactory) { baseIdx = i; break; }
    }
    for (uint8_t i = 0; i < 4; ++i) {
        _madctlTable[i] = static_cast<uint8_t>(
            kMadctlOrientations[(baseIdx + i) % 4] | (rgbOrder & 0x08U));
    }
}

Driver_CO5300::~Driver_CO5300() {
    if (_qspi) (void)_qspi->waitForTransfer();
#if SEEED_GFX_HAS_ESP32S3_LCD
    if (_transferBuffer) heap_caps_free(_transferBuffer);
#endif
}

bool Driver_CO5300::runInitSequence() {
    // Mirrors the vendor init lists for the three CO5300 glasses (the same
    // sequences the ESP-IDF component library ships as lcd_init_cmds).  The
    // 2.05" round part adds the source/gate-map block and wakes (SLPOUT) just
    // before DISPON; the 2.16"/1.75" parts use the bare "simple" sequence with
    // SLPOUT first.  Hardware reset is already done by the board layer.
    Serial.println("===begin init lcd===");

    if (!_extendedInit) {
        if (!_qspi->writeCommandData(0x11, nullptr, 0)) return false; // SLPOUT
        delay(600);
    }

    const uint8_t page20[1] = {0x20};
    if (!_qspi->writeCommandData(0xFE, page20, 1)) return false; // page 0x20
    const uint8_t v19[1] = {0x10};
    if (!_qspi->writeCommandData(0x19, v19, 1)) return false;
    const uint8_t v1C[1] = {0xA0};
    if (!_qspi->writeCommandData(0x1C, v1C, 1)) return false;

    if (_extendedInit) {
        // 2.05" round: partial-power / source-gate map (vendor demo).
        const uint8_t f4[1] = {0x5A};
        if (!_qspi->writeCommandData(0xF4, f4, 1)) return false;
        const uint8_t f5[1] = {0x59};
        if (!_qspi->writeCommandData(0xF5, f5, 1)) return false;
        const uint8_t page59[1] = {0x59};
        if (!_qspi->writeCommandData(0xFE, page59, 1)) return false; // page 0x59
        const uint8_t v4d[1] = {0x7F};
        if (!_qspi->writeCommandData(0x4D, v4d, 1)) return false;
        const uint8_t v4e[1] = {0x80};
        if (!_qspi->writeCommandData(0x4E, v4e, 1)) return false;
        const uint8_t page40[1] = {0x40};
        if (!_qspi->writeCommandData(0xFE, page40, 1)) return false; // page 0x40
        const uint8_t v54[1] = {0xAF};
        if (!_qspi->writeCommandData(0x54, v54, 1)) return false;
    }

    const uint8_t page00[1] = {0x00};
    if (!_qspi->writeCommandData(0xFE, page00, 1)) return false; // page 0x00
    const uint8_t c4[1] = {0x80};
    if (!_qspi->writeCommandData(0xC4, c4, 1)) return false;

    const uint8_t colmod = 0x55; // RGB565
    if (!_qspi->writeCommandData(0x3A, &colmod, 1)) return false;
    const uint8_t teLine = 0x00; // TE on
    if (!_qspi->writeCommandData(0x35, &teLine, 1)) return false;
    const uint8_t ctrlDisplay = 0x20; // brightness control on, via 0x51
    if (!_qspi->writeCommandData(0x53, &ctrlDisplay, 1)) return false;
    const uint8_t brightness = 0xFF; // max
    if (!_qspi->writeCommandData(0x51, &brightness, 1)) return false;
    const uint8_t bs0 = 0xFF;
    if (!_qspi->writeCommandData(0x63, &bs0, 1)) return false;

    // Full-span window, logical 0..(w-1)/0..(h-1), derived from the configured
    // panel size so the same driver serves every CO5300 part (2.16" 480x480
    // -> 0x01DF, 1.75" 466x466 -> 0x01D1, 2.05" 368x448 -> 0x16F/0x1BF).  The
    // source-column GRAM gap is applied per-draw in setAddrWindow, not here.
    const uint16_t winW = static_cast<uint16_t>(_width - 1);
    const uint8_t caset[4] = {0x00, 0x00, static_cast<uint8_t>(winW >> 8),
                              static_cast<uint8_t>(winW & 0xFF)};
    if (!_qspi->writeCommandData(0x2A, caset, 4)) return false;
    const uint16_t winH = static_cast<uint16_t>(_height - 1);
    const uint8_t raset[4] = {0x00, 0x00, static_cast<uint8_t>(winH >> 8),
                              static_cast<uint8_t>(winH & 0xFF)};
    if (!_qspi->writeCommandData(0x2B, raset, 4)) return false;

    // MADCTL is written by setRotation(0) at the end of begin<>, so the panel
    // lands on its factory orientation (0xA0 square / 0x00 round).

    if (_extendedInit) {
        if (!_qspi->writeCommandData(0x11, nullptr, 0)) return false; // SLPOUT
        delay(600);
    }
    if (!_qspi->writeCommandData(0x29, nullptr, 0)) return false; // DISPON
    delay(600);
    return true;
}

bool Driver_CO5300::init(IBus& bus) {
    _bus = &bus;
    // The ProductCatalog wires this driver only with Bus_ESP32QSPI. Avoid
    // RTTI because the ESP32 Arduino core is built with -fno-rtti.
    _qspi = static_cast<Bus_ESP32QSPI*>(&bus);
#if SEEED_GFX_HAS_ESP32S3_LCD
    // One CO5300_CHUNK_ROWS-row chunk per CS pulse (250 rows = 240 KB by
    // default: a full frame in two pulses, neither colliding with the panel
    // scan). If that allocation fails, fall back to a 4 KiB working buffer so
    // the driver still runs — but expect the small-chunk multi-pulse path to
    // be flaky.
    _transferBuffer = static_cast<uint16_t*>(
        heap_caps_malloc(kTransferChunkPixels * sizeof(uint16_t),
                         MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
    if (_transferBuffer) {
        _transferCapacityPixels = kTransferChunkPixels;
    } else {
        constexpr size_t kFallbackPixels = 2048;
        _transferBuffer = static_cast<uint16_t*>(
            heap_caps_malloc(kFallbackPixels * sizeof(uint16_t),
                             MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
        if (!_transferBuffer) {
            setOperationError(DriverOperationError::CommunicationFailed);
            return false;
        }
        _transferCapacityPixels = kFallbackPixels;
    }
    Serial.printf("CO5300 transfer buffer: %d px (%d B per pulse, free DMA heap %d B)\n",
                  _transferCapacityPixels, _transferCapacityPixels * 2,
                  heap_caps_get_free_size(MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
#else
    return false;
#endif
    if (!runInitSequence()) {
        setOperationError(DriverOperationError::CommunicationFailed);
        return false;
    }
    setAddrWindow(0, 0, width() - 1, height() - 1);
    return true;
}

void Driver_CO5300::setRotation(uint8_t rotation) {
    _rotation = rotation & 3U;
    if (_qspi) {
        const uint8_t madctl = _madctlTable[_rotation];
        (void)_qspi->writeCommandData(0x36, &madctl, 1);
    }
    setAddrWindow(0, 0, width() - 1, height() - 1);
}

void Driver_CO5300::invertDisplay(bool invert) {
    if (_qspi) (void)_qspi->writeCommandData(invert ? 0x21 : 0x20, nullptr, 0);
}

void Driver_CO5300::displayOn() {
    if (_qspi) (void)_qspi->writeCommandData(0x29, nullptr, 0);
}

void Driver_CO5300::displayOff() {
    if (_qspi) (void)_qspi->writeCommandData(0x28, nullptr, 0);
}

void Driver_CO5300::setAddrWindow(uint16_t xs, uint16_t ys,
                                  uint16_t xe, uint16_t ye) {
    const uint16_t w = width();
    const uint16_t h = height();
    if (xs >= w) xs = static_cast<uint16_t>(w - 1);
    if (ys >= h) ys = static_cast<uint16_t>(h - 1);
    if (xe >= w) xe = static_cast<uint16_t>(w - 1);
    if (ye >= h) ye = static_cast<uint16_t>(h - 1);
    if (xs > xe) xs = xe;
    if (ys > ye) ys = ye;
    _winX0 = xs; _winY0 = ys; _winX1 = xe; _winY1 = ye;

    // The round glasses expose only a slice of the CO5300's full-width source
    // (2.05" -> CASET +16, 1.75" -> CASET +6, 2.16" -> +0).  Keep _win* in
    // LOGICAL coordinates for the slice math in writeFill/writePixels, and add
    // the GRAM gap here when composing the physical window bytes.
    const uint8_t columns[4] = {
        static_cast<uint8_t>((xs + _gapX) >> 8),
        static_cast<uint8_t>(xs + _gapX),
        static_cast<uint8_t>((xe + _gapX) >> 8),
        static_cast<uint8_t>(xe + _gapX)
    };
    const uint8_t rows[4] = {
        static_cast<uint8_t>((ys + _gapY) >> 8),
        static_cast<uint8_t>(ys + _gapY),
        static_cast<uint8_t>((ye + _gapY) >> 8),
        static_cast<uint8_t>(ye + _gapY)
    };
    if (_qspi) {
        (void)_qspi->writeCommandData(0x2A, columns, 4);
        (void)_qspi->writeCommandData(0x2B, rows, 4);
    }
}

bool Driver_CO5300::flushChunk(const uint16_t* data, size_t count,
                               bool fill, uint16_t fillColor) {
    // Constant inter-pulse gap first (probe knob: how much spacing a one-shot
    // fill needs to match the LVGL demo's cadence, which never fires
    // consecutive pulses back to back). Then the phase sweep.
    if (_interPulseGapUs) delayMicroseconds(_interPulseGapUs);
    if (_interPulseSpacingUs) {
        const uint32_t phase = _pulseNumber & 15U;
        ++_pulseNumber;
        if (phase) delayMicroseconds(_interPulseSpacingUs * phase);
    }
    // The DMA chunk may not be overwritten until the previous color transfer
    // completes: acquire the bus transfer buffer before filling.
    if (count == 0 || !_transferBuffer || count > _transferCapacityPixels ||
        !_qspi->acquireTransferBuffer()) {
        return false;
    }
    if (fill) {
        const uint16_t swapped = kSwapColor(fillColor);
        for (size_t i = 0; i < count; ++i) _transferBuffer[i] = swapped;
    } else {
        for (size_t i = 0; i < count; ++i) {
            const uint16_t color = data[i];
            _transferBuffer[i] = kSwapColor(color);
        }
    }
    if (!_qspi->writeColor(0x2C, _transferBuffer, count * sizeof(uint16_t))) {
        _qspi->releaseTransferBuffer();
        return false;
    }
    return true;
}

void Driver_CO5300::writePixel(uint16_t color) {
    writePixels(&color, 1);
}

// Send the staged odd-window rows (_pendingRows, byte-swapped, in
// _transferBuffer) as one pulse addressing their own stored window. The CO5300
// keeps RAMWR state only within a CS pulse, so this re-addresses with
// CASET/RASET exactly like every other pulse. _pendingRows is drained
// regardless of parity: in the normal pushImage flow it is even when called;
// a stranded odd row is sent as-is (equally dropped, no regression).
void Driver_CO5300::finalizeStaged() {
    if (_pendingRows == 0) return;
    if (!_qspi) { _pendingRows = 0; return; }
    (void)_qspi->waitForTransfer();
    if (!_qspi->acquireTransferBuffer()) return;   // keep _pendingRows: retry next call
    const size_t w = static_cast<size_t>(_pendingX1 - _pendingX0 + 1);
    setAddrWindow(_pendingX0, _pendingY0, _pendingX1,
                  static_cast<uint16_t>(_pendingY0 + _pendingRows - 1));
    if (!_qspi->writeColor(0x2C, _transferBuffer,
                           _pendingRows * w * sizeof(uint16_t))) {
        _qspi->releaseTransferBuffer();
        return;                                    // keep _pendingRows too
    }
    _pendingRows = 0;
}

void Driver_CO5300::writePixels(const uint16_t* data, size_t len) {
    if (!_qspi || !data || len == 0) return;
    const uint16_t w = static_cast<uint16_t>(_winX1 - _winX0 + 1);
    if (w == 0) return;
    const uint16_t h = static_cast<uint16_t>(_winY1 - _winY0 + 1);

    // Single pixel: the 1x1 window / 2 B burst does NOT land on this panel
    // (bench: size-1 text drawn pixel-by-pixel is invisible) - the CO5300
    // needs an even x even window (see writeFill).  Paint it as a 2x2 block
    // of the same color: 1-px overshoot right/down is the smallest mark this
    // panel will actually show.
    if (len == 1) {
        finalizeStaged();                           // keep draw order vs staged rows
        uint16_t x0 = _winX0, y0 = _winY0, x1 = _winX1, y1 = _winY1;
        if (static_cast<uint32_t>(x1) + 1U < width())  ++x1; else if (x0 > 0) --x0;
        if (static_cast<uint32_t>(y1) + 1U < height()) ++y1; else if (y0 > 0) --y0;
        setAddrWindow(x0, y0, x1, y1);
        const size_t px = static_cast<size_t>(x1 - x0 + 1) * (y1 - y0 + 1);
        if (!flushChunk(nullptr, px, true, data[0]))
            setOperationError(DriverOperationError::CommunicationFailed);
        return;
    }

    // Odd-height full-row write (pushImage row blits, odd sprites): the CO5300
    // discards color bursts aimed at odd-height windows (1-row = odd). Stage
    // the whole row(s) — byte-swapped — and once an EVEN count has assembled,
    // send them as ONE even-height-window pulse (2-row windows proven to land).
    const bool fullRowSpan = len == (size_t)w * h;
    if (fullRowSpan && (h & 1U)) {
        if (_pendingRows * w + len > _transferCapacityPixels) finalizeStaged();
        const bool contiguous =
            (_pendingRows == 0) ||
            (_winX0 == _pendingX0 && _winX1 == _pendingX1 &&
             (uint32_t)_winY0 == (uint32_t)_pendingY0 + _pendingRows);
        if (!contiguous) finalizeStaged();          // start a fresh run
        if (_pendingRows == 0) {
            _pendingX0 = _winX0; _pendingX1 = _winX1; _pendingY0 = _winY0;
        }
        (void)_qspi->waitForTransfer();             // buffer free before filling
        for (size_t r = 0; r < h; ++r) {
            const uint16_t* src = data + r * w;
            uint16_t* dst = _transferBuffer + _pendingRows * w;
            for (size_t i = 0; i < w; ++i)
                dst[i] = kSwapColor(src[i]);
            ++_pendingRows;
        }
        if ((_pendingRows & 1U) == 0) finalizeStaged();
        (void)_qspi->waitForTransfer();
        return;
    }

    // Even-height (or irregular) window: drain staged rows, then the proven
    // per-CS-pulse slice path. A fill larger than one pulse is sliced
    // per-CS-pulse (the CO5300 drops its RAMWR pointer across CS pulses, so
    // every pulse re-addresses its own slice). Wrap the whole window twice:
    // pass 0 on the natural lattice (0/50/100…), pass 1 on a lattice shifted
    // by half a chunk (25/75/125…) so every row is written by two
    // different-starting-row pulses (kDualPass, off by default).
    finalizeStaged();
    const size_t chunkRows = _transferCapacityPixels / w;
    const size_t passOffsetPx[2] = {0, (chunkRows / 2) * w};
    const bool dualPass =
        kDualPassMultiSliceFills && len > _transferCapacityPixels && chunkRows > 1;
    for (size_t pass = 0; pass < (dualPass ? 2U : 1U); ++pass) {
        size_t written = passOffsetPx[pass];
        while (written < len) {
            const size_t remaining = len - written;
            const size_t count = remaining < _transferCapacityPixels
                                     ? remaining : _transferCapacityPixels;
            // Strict ordering: the next slice's CASET/RASET must not be sent
            // before the previous color DMA has fully finished.
            (void)_qspi->waitForTransfer();
            const size_t r0 = written / w;
            const size_t r1 = (written + count - 1) / w;
            setAddrWindow(_winX0, static_cast<uint16_t>(_winY0 + r0),
                          _winX1, static_cast<uint16_t>(_winY0 + r1));
            if (!flushChunk(data + written, count, false, 0)) {
                setOperationError(DriverOperationError::CommunicationFailed);
                return;
            }
            written += count;
        }
    }
    (void)_qspi->waitForTransfer();
}

void Driver_CO5300::writeFill(uint16_t color, size_t len) {
    if (!_qspi || len == 0) return;
    uint16_t w = static_cast<uint16_t>(_winX1 - _winX0 + 1);
    if (w == 0) return;
    finalizeStaged();   // drain any staged odd-window rows first (draw order)

    // The CO5300 silently DROPS a color burst unless its CASET/RASET window
    // is EVEN IN BOTH DIMENSIONS (2 px = one 32-bit quad word - bench-proven
    // on this panel: 464x212 fillRect lands, 2x2 text blocks land, 464x1
    // HLine lands only after extending to 464x2, while 1x212 VLine, 1x18
    // circle spans and 1x1 pixels NEVER land - no vertical rect borders, no
    // size-1 text, no fillCircle).  pushImage's even-row assembler in
    // writePixels() only covers the height half of the rule.  A solid fill
    // cannot be staged against neighbours like blit rows, so round odd
    // window dimensions UP and paint the extra row/column with the same
    // color: worst case a 1-px overshoot past the right/bottom edge
    // (invisible inside solid shapes) instead of the shape never appearing.
    if (len % w == 0) {
        size_t rows = len / w;
        if (w & 1U) {                     // odd width -> widen to even
            if (static_cast<uint32_t>(_winX1) + 1U < width()) ++_winX1;
            else if (_winX0 > 0)                            --_winX0;
            w = static_cast<uint16_t>(_winX1 - _winX0 + 1);
        }
        if (rows & 1U) {                  // odd height -> heighten to even
            if (static_cast<uint32_t>(_winY0) + rows >= height() && _winY0 > 0) {
                --_winY0;                 // at panel bottom: extend upward
            }
            ++rows;                       // else: extend downward (fits, h even)
        }
        len = static_cast<size_t>(w) * rows;
    }

    const size_t chunkRows = _transferCapacityPixels / w;
    // Same geometry as writePixels(): wrap the window once on the natural
    // slice lattice, then again on a half-chunk-shifted lattice, so a pulse
    // the panel drops on pass 1 is repainted by a pass-2 pulse that straddles
    // the pass-1 window boundary (GapProbe T3 verified this full-blue at 48 KB).
    const size_t passOffsetPx[2] = {0, (chunkRows / 2) * w};
    const bool dualPass =
        kDualPassMultiSliceFills && len > _transferCapacityPixels && chunkRows > 1;
    for (size_t pass = 0; pass < (dualPass ? 2U : 1U); ++pass) {
        size_t written = passOffsetPx[pass];
        while (written < len) {
            const size_t remaining = len - written;
            const size_t count = remaining < _transferCapacityPixels
                                     ? remaining : _transferCapacityPixels;
            // Strict ordering: the next slice's CASET/RASET must not be sent
            // before the previous color DMA has fully finished.
            (void)_qspi->waitForTransfer();
            // See writePixels(): every CS pulse must carry its own
            // CASET/RASET slice, or only the first chunk's rows ever appear.
            const size_t r0 = written / w;
            const size_t r1 = (written + count - 1) / w;
            setAddrWindow(_winX0, static_cast<uint16_t>(_winY0 + r0),
                          _winX1, static_cast<uint16_t>(_winY0 + r1));
            if (!flushChunk(nullptr, count, true, color)) {
                setOperationError(DriverOperationError::CommunicationFailed);
                return;
            }
            written += count;
        }
    }
    (void)_qspi->waitForTransfer();
}

void Driver_CO5300::sleep() {
    if (_qspi) {
        (void)_qspi->waitForTransfer();
        (void)_qspi->writeCommandData(0x10, nullptr, 0); // SLPIN
        delay(120);
    }
}

void Driver_CO5300::wake() {
    if (_qspi) {
        (void)_qspi->writeCommandData(0x11, nullptr, 0); // SLPOUT
        delay(120);
    }
}

void Driver_CO5300::setBrightness(uint8_t value) {
    if (_qspi) (void)_qspi->writeCommandData(0x51, &value, 1);
}

void Driver_CO5300::setInterPulseSpacingUs(uint32_t baseUs) {
    _interPulseSpacingUs = baseUs;
}

void Driver_CO5300::setInterPulseGapUs(uint32_t gapUs) {
    _interPulseGapUs = gapUs;
}