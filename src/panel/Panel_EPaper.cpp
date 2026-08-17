/**
 * @file   Panel_EPaper.cpp
 * @brief  ePaper panel implementation for Seeed_GFX v2.0
 *
 * Manages a standalone frame buffer for ePaper displays.
 * All drawing operations write to the frame buffer; the update()
 * method sends the buffer to the physical display.
 *
 * Frame buffer format:
 *   1bpp: each byte = 8 horizontal pixels, MSB = leftmost, 0 = white, 1 = black
 *   4bpp: each byte = 2 horizontal pixels, high nibble = left, indexed color
 */

#include "Panel_EPaper.h"

#if defined(ESP32)
#include "esp32-hal-psram.h"
#endif

namespace {
void* allocateEPaperMemory(size_t bytes) {
#if defined(ESP32)
    if (psramFound()) {
        void* memory = ps_malloc(bytes);
        if (memory) return memory;
    }
#endif
    return malloc(bytes);
}
}

// Constructor / Destructor

Panel_EPaper::Panel_EPaper(IDriver& driver, IBus& bus, IBoard* board)
    : _driver(driver)
    , _bus(bus)
    , _board(board)
    , _frameBuffer(nullptr)
    , _oldFrameBuffer(nullptr)
    , _rotation(0)
    , _grayLevel(0)
    , _bpp(1)
    , _colorMode(COLOR_MODE_NONE)
    , _sleeping(true)
    , _initialized(false)
    , _horizontalMirror(board ? board->panelHorizontalMirror() : false)
    , _verticalMirror(false)
    , _displayHorizontalMirror(
          board ? board->panelDisplayHorizontalMirror() : false)
    , _displayVerticalMirror(false)
    , _mirrorOverride(false)
    , _temp(16.0f)
    , _lastResult()
    , _init_width(0)
    , _init_height(0)
    , _visible_width(0)
    , _visible_height(0)
    , _addr_x0(0), _addr_y0(0), _addr_x1(0), _addr_y1(0)
    , _addr_x(0), _addr_y(0)
{
}

Panel_EPaper::~Panel_EPaper() {
    (void)end();
    freeFrameBuffer();
}

// IPanel interface: Initialization

bool Panel_EPaper::begin() {
    if (_initialized) {
        _lastResult = GfxResult::success();
        return true;
    }
    // Initialize the board (GPIOs, power, BUSY pin)
    if (_board) {
        if (!_board->begin()) {
            _lastResult = GfxResult(GfxError::BoardInitFailed,
                                    "ePaper board initialization failed");
            return false;
        }
        // Pass board pins to driver for hardware reset + busy wait
        _driver.setBusyPin(_board->busyPin());
        _driver.setResetPin(_board->pinRST());
    }

    // Initialize the bus (SPI, I2C, etc.)
    if (!_bus.begin()) {
        if (_board) _board->powerOff();
        _lastResult = GfxResult(GfxError::BusInitFailed,
                                "ePaper bus initialization failed");
        return false;
    }

    // Initialize the driver IC. BUSY timeout is sticky even though init()
    // keeps its legacy bool signature.
    _driver.clearOperationError();
    const bool driverInitialized = _driver.init(_bus);
    const GfxResult driverInitResult = driverOperationResult();
    if (!driverInitialized || !driverInitResult.ok()) {
        _bus.end();
        if (_board) _board->powerOff();
        _lastResult = driverInitResult.ok()
            ? GfxResult(GfxError::DriverInitFailed,
                        "ePaper driver initialization failed")
            : driverInitResult;
        return false;
    }

    // Store original dimensions (before any rotation swap)
    _init_width  = _driver.width();
    _init_height = _driver.height();
    _rotation    = _driver.rotation();
    if (_init_width == 0 || _init_height == 0) {
        _bus.end();
        if (_board) _board->powerOff();
        _lastResult = GfxResult(GfxError::PanelInitFailed,
                                "ePaper driver reported invalid dimensions");
        return false;
    }

    if (_visible_width == 0) _visible_width = _init_width;
    if (_visible_height == 0) _visible_height = _init_height;
    if (_visible_width > _init_width || _visible_height > _init_height) {
        _bus.end();
        if (_board) _board->powerOff();
        _lastResult = GfxResult(
            GfxError::InvalidArgument,
            "ePaper visible area exceeds controller storage geometry");
        return false;
    }

    // Allocate frame buffer at 1bpp
    _bpp = _driver.colorDepth();
    if (_bpp < 1) _bpp = 1;
    if (_bpp != 1 && _bpp != 4) _bpp = 1;

    if (!allocateFrameBuffer()) {
        _bus.end();
        if (_board) _board->powerOff();
        _lastResult = GfxResult(GfxError::AllocationFailed,
                                "ePaper frame-buffer allocation failed");
        return false;
    }

    // Both supported formats encode white as zero in this panel buffer.
    memset(_frameBuffer, 0x00, frameBufferSize());

    _sleeping = false;  // Display is awake after init (no sleep called yet)
    _initialized = true;
    _lastResult = GfxResult::success();
    return true;
}

GfxResult Panel_EPaper::end() {
    if (!_initialized) {
        _lastResult = GfxResult::success();
        return _lastResult;
    }
    sleep();
    const GfxResult sleepResult = _lastResult;
    _bus.end();
    if (_board) _board->powerOff();
    _initialized = false;
    _lastResult = sleepResult;
    return _lastResult;
}

GfxResult Panel_EPaper::configure(PanelMode mode) {
    if ((mode == PanelMode::Gray4 && !_driver.supportsGrayRefresh(4)) ||
        (mode == PanelMode::Gray16 && !_driver.supportsGrayRefresh(16))) {
        _lastResult = GfxResult(GfxError::NotSupported,
                                "requested grayscale mode is not supported by this driver");
        return _lastResult;
    }
    if ((mode == PanelMode::Colorful && !_driver.supportsColorfulEPaper()) ||
        (mode == PanelMode::BWRY && !_driver.supportsBWRYEPaper())) {
        _lastResult = GfxResult(GfxError::NotSupported,
                                "requested color mode is not supported by this driver");
        return _lastResult;
    }

    bool configured = false;
    switch (mode) {
        case PanelMode::Native:   configured = deinitGrayMode(); break;
        case PanelMode::Gray4:    configured = initGrayMode(4); break;
        case PanelMode::Gray16:   configured = initGrayMode(16); break;
        case PanelMode::Colorful: configured = initColorfulMode(); break;
        case PanelMode::BWRY:     configured = initBWRYMode(); break;
    }
    _lastResult = configured
        ? GfxResult::success()
        : GfxResult(GfxError::AllocationFailed,
                    "ePaper mode frame-buffer allocation failed");
    return _lastResult;
}

GfxResult Panel_EPaper::configureVisibleArea(uint16_t visibleWidth,
                                             uint16_t visibleHeight) {
    if (_initialized) {
        _lastResult = GfxResult(
            GfxError::InvalidArgument,
            "ePaper visible area must be configured before begin");
        return _lastResult;
    }
    if (visibleWidth == 0 || visibleHeight == 0) {
        _lastResult = GfxResult(GfxError::InvalidArgument,
                                "ePaper visible area must be non-zero");
        return _lastResult;
    }
    _visible_width = visibleWidth;
    _visible_height = visibleHeight;
    _lastResult = GfxResult::success();
    return _lastResult;
}

// IPanel interface: Dimensions

uint16_t Panel_EPaper::width() const {
    return (_rotation & 1) ? _visible_height : _visible_width;
}

uint16_t Panel_EPaper::height() const {
    return (_rotation & 1) ? _visible_width : _visible_height;
}

uint8_t Panel_EPaper::colorDepth() const {
    return _bpp;
}

// IPanel interface: Rotation

void Panel_EPaper::setRotation(uint8_t r) {
    _rotation = r % 4;
    // The frame buffer owns coordinate rotation. Keep the controller in its
    // canonical orientation so rotation is not applied twice.
    _driver.setRotation(0);
}

uint8_t Panel_EPaper::rotation() const {
    return _rotation;
}

// IPanel interface: Display control

void Panel_EPaper::invertDisplay(bool /*i*/) {
    // ePaper does not support inversion in hardware.
    // Inversion can be achieved by inverting the frame buffer.
}

void Panel_EPaper::setBacklight(uint8_t /*brightness*/) {
    // ePaper has no backlight
}

uint8_t Panel_EPaper::backlight() const {
    return 0;
}

IDriver& Panel_EPaper::driver() {
    return _driver;
}

bool Panel_EPaper::selectWaveformProfile(const char* id) {
    const bool selected = _driver.selectWaveformProfile(id);
    if (!selected) {
        _lastResult = GfxResult(GfxError::NotSupported,
                                "unknown or incompatible ePaper waveform profile");
    }
    return selected;
}

const EPaperWaveformProfile* Panel_EPaper::waveformProfile() const {
    return _driver.waveformProfile();
}

EPaperWaveformResult Panel_EPaper::waveformProfileResult() const {
    return _driver.waveformProfileResult();
}

DisplayCapabilities Panel_EPaper::capabilities() const {
    DisplayCapabilities caps;
    caps.technology = DisplayTechnology::EInk;
    caps.nativeFormat = (_bpp == 1) ? PixelFormat::Mono1
        : ((_colorMode == COLOR_MODE_NONE) ? PixelFormat::Gray4 : PixelFormat::Indexed4);
    caps.readback = true; // Reads are served from the owned frame buffer.
    caps.partialRefresh = _driver.supportsPartialRefresh();
    caps.fastRefresh = _driver.supportsFastRefresh();
    caps.temperatureCompensation = _driver.supportsTemperatureCompensation();
    caps.deepSleep = _driver.supportsDeepSleep();
    const uint16_t driverAlignment = _driver.partialXAlignment();
    caps.partialXAlignment = driverAlignment
        ? driverAlignment
        : ((_bpp == 1) ? 8 : 2);
    caps.partialYAlignment = 1;
    return caps;
}

// IPanel interface: Address window

void Panel_EPaper::setAddrWindow(uint16_t x0, uint16_t y0,
                                  uint16_t x1, uint16_t y1) {
    _addr_x0 = x0;
    _addr_y0 = y0;
    _addr_x1 = x1;
    _addr_y1 = y1;
    _addr_x  = x0;
    _addr_y  = y0;
}

// IPanel interface: Pixel writing (to frame buffer)

void Panel_EPaper::writePixel(uint16_t color) {
    if (!_frameBuffer) return;
    if (_addr_x > _addr_x1 || _addr_y > _addr_y1) return;

    uint16_t physicalX = 0;
    uint16_t physicalY = 0;
    if (!mapToPhysical(_addr_x, _addr_y, physicalX, physicalY)) {
        // Advance and skip
        _addr_x++;
        if (_addr_x > _addr_x1) {
            _addr_x = _addr_x0;
            _addr_y++;
        }
        return;
    }

    if (_bpp == 1) {
        // 1bpp: 8 pixels per byte, MSB = leftmost
        // Panel buffer: 0 = white, 1 = black
        size_t byteIdx = static_cast<size_t>(physicalY) * frameStride() + (physicalX >> 3);
        uint8_t bit = 0x80 >> (physicalX & 7);
        if (color) {
            _frameBuffer[byteIdx] &= ~bit;  // white (0)
        } else {
            _frameBuffer[byteIdx] |= bit;   // black (1)
        }
    } else if (_bpp == 4) {
        // 4bpp: 2 pixels per byte, high nibble = left pixel
        size_t byteIdx = static_cast<size_t>(physicalY) * frameStride() + (physicalX >> 1);
        uint8_t  grayVal;
        if (_colorMode == COLOR_MODE_COLORFUL || _colorMode == COLOR_MODE_BWRY) {
            // Map RGB565 to the 6-color or 4-color palette
            grayVal = mapColorToPalette((uint16_t)color);
        } else {
            // Grayscale: use lower 4 bits directly
            grayVal = (uint8_t)(color & 0x0F);
        }
        if (physicalX & 1) {
            // Right pixel: low nibble
            _frameBuffer[byteIdx] = (_frameBuffer[byteIdx] & 0xF0) | (grayVal & 0x0F);
        } else {
            // Left pixel: high nibble
            _frameBuffer[byteIdx] = (_frameBuffer[byteIdx] & 0x0F) | ((grayVal & 0x0F) << 4);
        }
    }

    // Advance cursor
    _addr_x++;
    if (_addr_x > _addr_x1) {
        _addr_x = _addr_x0;
        _addr_y++;
    }
}

void Panel_EPaper::writePixels(const uint16_t* colors, size_t len) {
    if (!_frameBuffer || !colors) return;
    for (size_t i = 0; i < len; i++) {
        writePixel(colors[i]);
    }
}

void Panel_EPaper::writeFill(uint16_t color, size_t len) {
    if (!_frameBuffer) return;
    for (size_t i = 0; i < len; i++) {
        writePixel(color);
    }
}

bool Panel_EPaper::pushImage4BPP(int32_t x, int32_t y,
                                 int32_t w, int32_t h,
                                 const uint8_t* data,
                                 bool dataInProgmem) {
    if (!_frameBuffer || _bpp != 4 || !data || w <= 0 || h <= 0) return false;

    const size_t sourceStride = static_cast<size_t>(w + 1) / 2U;
    for (int32_t sourceY = 0; sourceY < h; ++sourceY) {
        const int32_t destinationY = y + sourceY;
        if (destinationY < 0 || destinationY >= height()) continue;

        for (int32_t sourceX = 0; sourceX < w; ++sourceX) {
            const int32_t destinationX = x + sourceX;
            if (destinationX < 0 || destinationX >= width()) continue;

            const uint8_t* source =
                data + static_cast<size_t>(sourceY) * sourceStride +
                static_cast<size_t>(sourceX >> 1);
            const uint8_t packed =
                dataInProgmem ? pgm_read_byte(source) : *source;
            const uint8_t index = (sourceX & 1)
                ? static_cast<uint8_t>(packed & 0x0F)
                : static_cast<uint8_t>((packed >> 4) & 0x0F);

            uint16_t physicalX = 0;
            uint16_t physicalY = 0;
            if (!mapToPhysical(destinationX, destinationY,
                               physicalX, physicalY)) {
                continue;
            }

            const size_t byteIdx =
                static_cast<size_t>(physicalY) * frameStride() +
                (physicalX >> 1);
            if (physicalX & 1U) {
                _frameBuffer[byteIdx] =
                    static_cast<uint8_t>((_frameBuffer[byteIdx] & 0xF0U) |
                                         index);
            } else {
                _frameBuffer[byteIdx] =
                    static_cast<uint8_t>((_frameBuffer[byteIdx] & 0x0FU) |
                                         (index << 4));
            }
        }
    }
    return true;
}

bool Panel_EPaper::pushImage4BPPRotatedCW(int32_t x, int32_t y,
                                          int32_t w, int32_t h,
                                          const uint8_t* data,
                                          bool dataInProgmem) {
    if (!_frameBuffer || _bpp != 4 || !data || w <= 0 || h <= 0) return false;

    const size_t sourceStride = static_cast<size_t>(w + 1) / 2U;
    for (int32_t sourceY = 0; sourceY < h; ++sourceY) {
        for (int32_t sourceX = 0; sourceX < w; ++sourceX) {
            const uint8_t* source =
                data + static_cast<size_t>(sourceY) * sourceStride +
                static_cast<size_t>(sourceX >> 1);
            const uint8_t packed =
                dataInProgmem ? pgm_read_byte(source) : *source;
            const uint8_t index = (sourceX & 1)
                ? static_cast<uint8_t>(packed & 0x0F)
                : static_cast<uint8_t>((packed >> 4) & 0x0F);

            const int32_t destinationX = x + h - 1 - sourceY;
            const int32_t destinationY = y + sourceX;
            uint16_t physicalX = 0;
            uint16_t physicalY = 0;
            if (!mapToPhysical(destinationX, destinationY,
                               physicalX, physicalY)) {
                continue;
            }

            const size_t byteIdx =
                static_cast<size_t>(physicalY) * frameStride() +
                (physicalX >> 1);
            if (physicalX & 1U) {
                _frameBuffer[byteIdx] =
                    static_cast<uint8_t>((_frameBuffer[byteIdx] & 0xF0U) |
                                         index);
            } else {
                _frameBuffer[byteIdx] =
                    static_cast<uint8_t>((_frameBuffer[byteIdx] & 0x0FU) |
                                         (index << 4));
            }
        }
    }
    return true;
}


uint16_t Panel_EPaper::readPixel(uint16_t x, uint16_t y) {
    if (!_frameBuffer) return 0;
    uint16_t physicalX = 0;
    uint16_t physicalY = 0;
    if (!mapToPhysical(x, y, physicalX, physicalY)) return 0;

    if (_bpp == 1) {
        size_t byteIdx = static_cast<size_t>(physicalY) * frameStride() + (physicalX >> 3);
        uint8_t bit = 0x80 >> (physicalX & 7);
        return (_frameBuffer[byteIdx] & bit) ? 0x0000 : 0xFFFF;  // 1=black, 0=white
    } else if (_bpp == 4) {
        size_t byteIdx = static_cast<size_t>(physicalY) * frameStride() + (physicalX >> 1);
        const uint8_t value = (physicalX & 1)
            ? (_frameBuffer[byteIdx] & 0x0F)
            : ((_frameBuffer[byteIdx] >> 4) & 0x0F);
        return decode4BitColor(value);
    }
    return 0;
}

// IPanel interface: Power management

void Panel_EPaper::sleep() {
    if (_sleeping) {
        _lastResult = GfxResult::success();
        return;
    }
    _driver.clearOperationError();
    ePaperSleep();
    _sleeping = true;
    _lastResult = driverOperationResult();
}

void Panel_EPaper::wake() {
    if (!_initialized) {
        _lastResult = GfxResult(GfxError::NotInitialized,
                                "ePaper panel is not initialized");
        return;
    }
    if (!_sleeping) {
        _lastResult = GfxResult::success();
        return;
    }
    _driver.clearOperationError();
    ePaperWakeUp();
    _sleeping = false;
    _lastResult = driverOperationResult();
}

// ePaper-specific: Full update

void Panel_EPaper::update() {
    _lastResult = refreshFull(false);
}

void Panel_EPaper::updateFast() {
    _lastResult = refreshFull(true);
}

GfxResult Panel_EPaper::refresh() {
    _lastResult = refreshFull(false);
    return _lastResult;
}

GfxResult Panel_EPaper::refreshFast() {
    _lastResult = refreshFull(true);
    return _lastResult;
}

GfxResult Panel_EPaper::refreshFull(bool fast) {
    if (!_initialized || !_frameBuffer) {
        return GfxResult(GfxError::NotInitialized,
                         "ePaper panel is not initialized");
    }
    if (fast && (_bpp != 1 || !_driver.supportsFastRefresh())) {
        return GfxResult(GfxError::NotSupported,
                         "fast refresh is not supported in this mode");
    }

    _driver.clearOperationError();

    const bool grayMode = (_bpp == 4 && _colorMode == COLOR_MODE_NONE && _grayLevel != 0);

    // Some ePaper controllers store byte-aligned rows that are wider than the
    // visible glass. Keep those controller-only columns white even if a caller
    // modified the raw frame buffer directly.
    clearStoragePadding(_frameBuffer);

    // Mode-specific wake-up is required because gray waveforms use different
    // LUT/register initialization from normal full refreshes.
    if (fast) {
        _driver.wakeFast();
        ePaperSetTemp(_temp);
    } else if (grayMode) {
        _driver.wakeGray();
        ePaperSetTemp(_temp);
    } else {
        ePaperWakeUp();
    }
    _sleeping = false;

    const GfxResult wakeResult = driverOperationResult();
    if (!wakeResult.ok()) {
        return wakeResult.error == GfxError::BusyTimeout
            ? GfxResult(GfxError::BusyTimeout,
                        "ePaper controller BUSY wait timed out during wake-up")
            : wakeResult;
    }

    _driver.setAddrWindow(0, 0, _init_width - 1, _init_height - 1);

    // Vertical mirror (native-Y flip): reverse the row order of the current
    // frame into a temp buffer. A 90-degree rotation swaps native-X <->
    // displayed-Y and native-Y <-> displayed-X, so for landscape-rotated
    // (rot1/rot3) panels a board-level *horizontal* mirror must be countered
    // with a *native-Y* flip (setHorizontalMirror flips native-X = displayed-Y
    // on a rotated panel, which would yield 180 deg, not a fix). _oldFrameBuffer
    // is kept in display orientation across refreshes (the snapshot below
    // stores the reversed frame), so only the new frame is reversed here.
    // Composes with the horizontal flip applied next: V-only = native-Y flip,
    // V+H = 180 deg. The partial-refresh path applies the same transform to
    // its native window and row stream. No-op when it is false.
    uint8_t* vfb = nullptr;
    const uint8_t* newFB = _frameBuffer;
    const bool mirrorVertical = verticalMirror();
    const bool mirrorHorizontal = horizontalMirror();
    if (mirrorVertical) {
        const size_t stride = frameStride();
        const size_t total = frameBufferSize();
        vfb = static_cast<uint8_t*>(allocateEPaperMemory(total));
        if (!vfb) {
            ePaperSleep();
            _sleeping = true;
            return GfxResult(GfxError::AllocationFailed,
                             "ePaper vertical-mirror allocation failed");
        }
        for (uint16_t r = 0; r < _init_height; ++r)
            memcpy(vfb + static_cast<size_t>(_init_height - 1 - r) * stride,
                   _frameBuffer + static_cast<size_t>(r) * stride, stride);
        newFB = vfb;
    }

    if (_bpp == 1) {
        // Monochrome mode: push old + new data
        size_t bufSize = frameBufferSize();

        // UC8179 compares old (DTM1) and new (DTM2) data to determine
        // which pixels need to transition. We must push the PREVIOUS
        // frame content as old data, and the CURRENT frame buffer as new data.
        if (!_oldFrameBuffer) {
            // First update: old frame is all white (0x00)
            _oldFrameBuffer = static_cast<uint8_t*>(allocateEPaperMemory(bufSize));
            if (!_oldFrameBuffer) {
                free(vfb);
                ePaperSleep();
                _sleeping = true;
                return GfxResult(GfxError::AllocationFailed,
                                 "ePaper previous-frame allocation failed");
            }
            memset(_oldFrameBuffer, 0x00, bufSize);
        }

        if (mirrorHorizontal) {
            if (!pushOldColorsFlip(_oldFrameBuffer,
                                   static_cast<uint16_t>(frameStride()), _init_height) ||
                !pushNewColorsFlip(newFB,
                                   static_cast<uint16_t>(frameStride()), _init_height)) {
                free(vfb);
                ePaperSleep();
                _sleeping = true;
                return GfxResult(GfxError::AllocationFailed,
                                 "ePaper mirrored transfer allocation failed");
            }
        } else {
            pushOldColors(_oldFrameBuffer, bufSize);
            pushNewColors(newFB, bufSize);
        }

        if (fast) ePaperUpdateFast();
        else ePaperUpdate();

        // Advance the previous-frame snapshot only when the physical refresh
        // completed. A timeout must retain the last known displayed frame.
        // With _verticalMirror the displayed frame is the reversed buffer, so
        // store newFB to keep the next differential diff consistent.
        if (driverOperationResult().ok()) {
            memcpy(_oldFrameBuffer, newFB, bufSize);
        }
    } else if (_bpp == 4) {
        size_t bufSize = frameBufferSize();

        if (grayMode && mirrorHorizontal) {
            if (!pushGrayColorsFlip(newFB, static_cast<uint16_t>(frameStride()),
                                    _init_height)) {
                free(vfb);
                ePaperSleep();
                _sleeping = true;
                return GfxResult(GfxError::AllocationFailed,
                                 "ePaper mirrored gray transfer allocation failed");
            }
        } else if (grayMode) {
            pushGrayColors(newFB, bufSize);
        } else if (mirrorHorizontal) {
            if (!pushNewColorsFlip(newFB, static_cast<uint16_t>(frameStride()),
                                   _init_height)) {
                free(vfb);
                ePaperSleep();
                _sleeping = true;
                return GfxResult(GfxError::AllocationFailed,
                                 "ePaper mirrored color transfer allocation failed");
            }
        } else {
            pushNewColors(newFB, bufSize);
        }

        if (grayMode) _driver.updateGray();
        else ePaperUpdate();
    }

    free(vfb);

    // Never send POF/DSLP while a timed-out refresh may still be running.
    // Doing so can freeze a multi-stage Spectra refresh in its yellow
    // intermediate phase and hides which operation actually failed.
    const GfxResult updateResult = driverOperationResult();
    if (!updateResult.ok()) {
        _sleeping = false;
        return updateResult.error == GfxError::BusyTimeout
            ? GfxResult(GfxError::BusyTimeout,
                        "ePaper controller BUSY wait timed out during refresh")
            : updateResult;
    }

    // Put display to sleep only after the physical refresh completed.
    ePaperSleep();
    const GfxResult sleepResult = driverOperationResult();
    _sleeping = sleepResult.ok();
    return sleepResult.error == GfxError::BusyTimeout
        ? GfxResult(GfxError::BusyTimeout,
                    "ePaper controller BUSY wait timed out during sleep")
        : sleepResult;
}

// ePaper-specific: Partial update

void Panel_EPaper::updatePartial(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    if (!_initialized || !_frameBuffer) {
        _lastResult = GfxResult(GfxError::NotInitialized,
                                "ePaper panel is not initialized");
        return;
    }
    if (w == 0 || h == 0) {
        _lastResult = GfxResult(GfxError::InvalidArgument,
                                "partial refresh area is empty");
        return;
    }
    if (_bpp != 1 || !_driver.supportsPartialRefresh()) {
        _lastResult = GfxResult(GfxError::NotSupported,
                                "partial refresh is not supported in this mode");
        return;
    }

    _driver.clearOperationError();
    clearStoragePadding(_frameBuffer);

    int32_t bx = (int32_t)x;
    int32_t by = (int32_t)y;
    int32_t bw = (int32_t)w;
    int32_t bh = (int32_t)h;

    // Map the rotated coordinate space back to the frame buffer.
    switch (_rotation & 3) {
    case 1:
        bx = (int32_t)_visible_width - y - h;
        by = x;
        bw = (int32_t)h;
        bh = (int32_t)w;
        break;
    case 2:
        bx = (int32_t)_visible_width - x - w;
        by = (int32_t)_visible_height - y - h;
        break;
    case 3:
        bx = y;
        by = (int32_t)_visible_height - x - w;
        bw = (int32_t)h;
        bh = (int32_t)w;
        break;
    default:
        break;
    }

    // Clip to frame buffer bounds
    if (bx < 0) { bw += bx; bx = 0; }
    if (by < 0) { bh += by; by = 0; }
    if ((bx + bw) > (int32_t)_visible_width)
        bw = (int32_t)_visible_width - bx;
    if ((by + bh) > (int32_t)_visible_height)
        bh = (int32_t)_visible_height - by;
    if (bw < 1 || bh < 1) {
        _lastResult = GfxResult(GfxError::InvalidArgument,
                                "partial refresh area is outside the panel");
        return;
    }

    // Align x to 8-pixel boundary (ePaper hardware requirement)
    const uint16_t align_px = capabilities().partialXAlignment;
    uint16_t x0 = ((uint16_t)bx) & ~(align_px - 1);
    uint16_t x1 = ((uint16_t)(bx + bw + (align_px - 1))) & ~(align_px - 1);
    if (x1 > _init_width) x1 = _init_width;
    uint16_t w_aligned = x1 - x0;
    uint16_t yy = (uint16_t)by;
    uint16_t hh = (uint16_t)bh;

    // Calculate row stride in bytes
    size_t stride = frameStride();
    uint16_t win_bytes_per_row = static_cast<uint16_t>((w_aligned + 7U) / 8U);
    const bool mirrorVertical = verticalMirror();
    const bool mirrorHorizontal = horizontalMirror();

    // Extract the relevant portion of the frame buffer. A native-Y mirror
    // reverses both the physical window and the row stream. The frame buffer
    // itself remains in logical/native order so drawing and readback do not
    // depend on the selected Breakout mounting correction.
    size_t win_size = (size_t)win_bytes_per_row * hh;
    uint8_t* winbuf = static_cast<uint8_t*>(allocateEPaperMemory(win_size));
    if (!winbuf) {
        _lastResult = GfxResult(GfxError::AllocationFailed,
                                "partial refresh transfer allocation failed");
        return;
    }

    for (uint16_t row = 0; row < hh; row++) {
        const uint16_t sourceRow = mirrorVertical
            ? static_cast<uint16_t>(yy + hh - 1U - row)
            : static_cast<uint16_t>(yy + row);
        memcpy(winbuf + (uint32_t)row * win_bytes_per_row,
               _frameBuffer + static_cast<size_t>(sourceRow) * stride +
                   (x0 >> 3),
               win_bytes_per_row);
    }

    // Partial mode has its own LUT/register setup; enter it even if the panel
    // was manually woken in full-refresh mode beforehand.
    _driver.wakePartial();
    ePaperSetTemp(_temp);
    _sleeping = false;

    // Set the physical window and push new data. Full-frame vertical mirroring
    // reverses all controller-storage rows, so use _init_height here rather
    // than the public visible height.
    const uint16_t windowY = mirrorVertical
        ? static_cast<uint16_t>(_init_height - yy - hh)
        : yy;
    if (mirrorHorizontal) {
        uint16_t x_end = x0 + w_aligned - 1;
        uint16_t mx0 = (_visible_width - 1) - x_end;
        uint16_t mx1 = (_visible_width - 1) - x0;
        _driver.setAddrWindow(mx0, windowY, mx1, windowY + hh - 1);
        if (!pushNewColorsFlip(winbuf, win_bytes_per_row, hh)) {
            free(winbuf);
            ePaperSleep();
            _sleeping = true;
            _lastResult = GfxResult(GfxError::AllocationFailed,
                                    "partial mirrored transfer allocation failed");
            return;
        }
    } else {
        _driver.setAddrWindow(x0, windowY,
                              x0 + w_aligned - 1, windowY + hh - 1);
        pushNewColors(winbuf, win_size);
    }

    // Trigger partial update
    ePaperUpdatePartial();

    const GfxResult updateResult = driverOperationResult();

    // Keep the previous-frame snapshot coherent only after a successful
    // physical refresh. _oldFrameBuffer stores the row orientation that was
    // sent during the preceding full refresh, so mirrored partial rows belong
    // at their transformed physical Y positions as well.
    if (updateResult.ok() && _oldFrameBuffer) {
        for (uint16_t row = 0; row < hh; ++row) {
            memcpy(_oldFrameBuffer +
                       static_cast<size_t>(windowY + row) * stride + (x0 >> 3),
                   winbuf + static_cast<size_t>(row) * win_bytes_per_row,
                   win_bytes_per_row);
        }
    }

    free(winbuf);

    // Put display back to sleep
    ePaperSleep();
    _sleeping = true;
    _lastResult = driverOperationResult();
}

GfxResult Panel_EPaper::refreshPartial(uint16_t x, uint16_t y,
                                       uint16_t w, uint16_t h) {
    if (!_initialized || !_frameBuffer) {
        _lastResult = GfxResult(GfxError::NotInitialized,
                                "ePaper panel is not initialized");
        return _lastResult;
    }
    if (w == 0 || h == 0) {
        _lastResult = GfxResult(GfxError::InvalidArgument,
                                "partial refresh area is empty");
        return _lastResult;
    }
    if (_bpp != 1 || !_driver.supportsPartialRefresh()) {
        _lastResult = GfxResult(GfxError::NotSupported,
                                "partial refresh is not supported in this mode");
        return _lastResult;
    }
    updatePartial(x, y, w, h);
    return _lastResult;
}

// Driver synchronization

void Panel_EPaper::waitBusy() {
    // All concrete driver operations are synchronous. BUSY polarity and the
    // bounded wait belong to the driver, so there is nothing to poll here.
}

// Frame buffer access

uint8_t* Panel_EPaper::frameBuffer() {
    return _frameBuffer;
}

void Panel_EPaper::drawBufferPixel(int32_t x, int32_t y,
                                    uint32_t color, uint8_t bpp) {
    if (!_frameBuffer || bpp != _bpp || (bpp != 1 && bpp != 4)) return;
    const uint8_t pixels = 8 / bpp;
    if (x < 0 || y < 0 || y >= height() || x + pixels > width()) return;
    const uint16_t savedX0 = _addr_x0, savedY0 = _addr_y0;
    const uint16_t savedX1 = _addr_x1, savedY1 = _addr_y1;
    const uint16_t savedX = _addr_x, savedY = _addr_y;
    setAddrWindow(static_cast<uint16_t>(x), static_cast<uint16_t>(y),
                  static_cast<uint16_t>(x + pixels - 1), static_cast<uint16_t>(y));
    for (uint8_t i = 0; i < pixels; ++i) {
        if (bpp == 1) {
            // Packed monochrome uses 1=black, while writePixel accepts RGB565.
            writePixel((color & (0x80U >> i)) ? 0x0000 : 0xFFFF);
        } else {
            const uint8_t nibble = (i == 0) ? ((color >> 4) & 0x0F) : (color & 0x0F);
            writePixel(_colorMode == COLOR_MODE_NONE ? nibble : decode4BitColor(nibble));
        }
    }
    _addr_x0 = savedX0; _addr_y0 = savedY0;
    _addr_x1 = savedX1; _addr_y1 = savedY1;
    _addr_x = savedX; _addr_y = savedY;
}

bool Panel_EPaper::mapToPhysical(int32_t x, int32_t y, uint16_t& physicalX,
                                 uint16_t& physicalY) const {
    if (x < 0 || y < 0 || x >= width() || y >= height()) return false;

    int32_t px = x;
    int32_t py = y;
    switch (_rotation & 3) {
        case 1:
            px = static_cast<int32_t>(_visible_width) - 1 - y;
            py = x;
            break;
        case 2:
            px = static_cast<int32_t>(_visible_width) - 1 - x;
            py = static_cast<int32_t>(_visible_height) - 1 - y;
            break;
        case 3:
            px = y;
            py = static_cast<int32_t>(_visible_height) - 1 - x;
            break;
        default:
            break;
    }
    if (px < 0 || py < 0 || px >= _init_width || py >= _init_height) return false;
    physicalX = static_cast<uint16_t>(px);
    physicalY = static_cast<uint16_t>(py);
    return true;
}

// Gray mode

bool Panel_EPaper::initGrayMode(uint8_t grayLevel) {
    if (grayLevel != 4 && grayLevel != 16) {
        return false;
    }
    if (!_driver.supportsGrayRefresh(grayLevel)) {
        return false;
    }
    if (grayLevel == _grayLevel && _bpp == 4) {
        return _frameBuffer != nullptr;
    }

    _grayLevel = grayLevel;
    _colorMode = COLOR_MODE_NONE;  // Gray mode is not color mode

    if (_bpp == 4 && _frameBuffer) {
        const uint8_t white = static_cast<uint8_t>(grayLevel - 1);
        memset(_frameBuffer, white | (white << 4), frameBufferSize());
        return true;
    }

    freeFrameBuffer();
    _bpp = 4;

    if (!allocateFrameBuffer()) {
        _bpp = 1;
        _grayLevel = 0;
        if (allocateFrameBuffer()) memset(_frameBuffer, 0x00, frameBufferSize());
        return false;
    }

    // Fill with white (grayLevel - 1 = lightest value)
    size_t bufSize = frameBufferSize();
    memset(_frameBuffer, (grayLevel - 1) | ((grayLevel - 1) << 4), bufSize);
    return true;
}

bool Panel_EPaper::deinitGrayMode() {
    if (_bpp == 1) return _frameBuffer != nullptr;

    // Free the old 4bpp buffer
    freeFrameBuffer();

    _grayLevel = 0;
    _bpp = 1;
    _colorMode = COLOR_MODE_NONE;

    if (!allocateFrameBuffer()) {
        return false;
    }

    // Monochrome buffer encodes white as zero.
    memset(_frameBuffer, 0x00, frameBufferSize());
    return true;
}

// Color mode (6-color / BWRY)

// 6-color palette: Black, White, Red, Yellow, Blue, Green
// 4-bit values per Color.h USE_COLORFULL_EPAPER definitions
static const uint8_t COLORFUL_PALETTE_4BIT[] = {
    0x0F, // Black  (TFT_EPD_BLACK)
    0x00, // White  (TFT_EPD_WHITE)
    0x06, // Red    (TFT_EPD_RED)
    0x0B, // Yellow (TFT_EPD_YELLOW)
    0x0D, // Blue   (TFT_EPD_BLUE)
    0x02, // Green  (TFT_EPD_GREEN)
};
static const uint16_t COLORFUL_PALETTE_RGB[] = {
    0x0000, // Black
    0xFFFF, // White
    0xF800, // Red
    0xFFE0, // Yellow
    0x001F, // Blue
    0x07E0, // Green
};
static const uint8_t COLORFUL_PALETTE_COUNT = 6;

// BWRY palette: Black, White, Red, Yellow
static const uint8_t BWRY_PALETTE_4BIT[] = {
    0x0F, // Black  (TFT_EPD_BLACK)
    0x00, // White  (TFT_EPD_WHITE)
    0x06, // Red    (TFT_EPD_RED)
    0x0B, // Yellow (TFT_EPD_YELLOW)
};
static const uint16_t BWRY_PALETTE_RGB[] = {
    0x0000, // Black
    0xFFFF, // White
    0xF800, // Red
    0xFFE0, // Yellow
};
static const uint8_t BWRY_PALETTE_COUNT = 4;

uint8_t Panel_EPaper::mapColorToPalette(uint16_t color) {
    const uint8_t* palette = (_colorMode == COLOR_MODE_BWRY) ? BWRY_PALETTE_4BIT : COLORFUL_PALETTE_4BIT;
    const uint16_t* rgbPalette = (_colorMode == COLOR_MODE_BWRY) ? BWRY_PALETTE_RGB : COLORFUL_PALETTE_RGB;
    uint8_t count = (_colorMode == COLOR_MODE_BWRY) ? BWRY_PALETTE_COUNT : COLORFUL_PALETTE_COUNT;

    // Find nearest color by Euclidean distance in RGB space
    int bestIdx = 0;
    uint32_t bestDist = 0xFFFFFFFF;

    uint8_t r1 = (color >> 11) & 0x1F;
    uint8_t g1 = (color >> 5) & 0x3F;
    uint8_t b1 = color & 0x1F;

    for (uint8_t i = 0; i < count; i++) {
        uint8_t r2 = (rgbPalette[i] >> 11) & 0x1F;
        uint8_t g2 = (rgbPalette[i] >> 5) & 0x3F;
        uint8_t b2 = rgbPalette[i] & 0x1F;

        int32_t dr = (int32_t)r1 - (int32_t)r2;
        int32_t dg = (int32_t)g1 - (int32_t)g2;
        int32_t db = (int32_t)b1 - (int32_t)b2;
        uint32_t dist = (uint32_t)(dr * dr + dg * dg + db * db);

        if (dist < bestDist) {
            bestDist = dist;
            bestIdx = i;
        }
    }

    return palette[bestIdx];
}

uint16_t Panel_EPaper::decode4BitColor(uint8_t value) const {
    value &= 0x0F;
    if (_colorMode != COLOR_MODE_NONE) {
        const uint8_t* palette = (_colorMode == COLOR_MODE_BWRY)
            ? BWRY_PALETTE_4BIT : COLORFUL_PALETTE_4BIT;
        const uint16_t* rgb = (_colorMode == COLOR_MODE_BWRY)
            ? BWRY_PALETTE_RGB : COLORFUL_PALETTE_RGB;
        const uint8_t count = (_colorMode == COLOR_MODE_BWRY)
            ? BWRY_PALETTE_COUNT : COLORFUL_PALETTE_COUNT;
        for (uint8_t i = 0; i < count; ++i)
            if (palette[i] == value) return rgb[i];
        return 0xFFFF;
    }
    const uint8_t maxValue = (_grayLevel == 4) ? 3 : 15;
    if (value > maxValue) value = maxValue;
    const uint8_t level = static_cast<uint8_t>((uint16_t)value * 255U / maxValue);
    return static_cast<uint16_t>(((level & 0xF8U) << 8) |
                                 ((level & 0xFCU) << 3) |
                                 (level >> 3));
}

bool Panel_EPaper::initColorfulMode() {
    if (_bpp == 4 && _colorMode == COLOR_MODE_COLORFUL)
        return _frameBuffer != nullptr;

    if (_bpp == 4 && _frameBuffer) {
        _grayLevel = 0;
        _colorMode = COLOR_MODE_COLORFUL;
        memset(_frameBuffer, 0x00, frameBufferSize());
        return true;
    }

    freeFrameBuffer();
    _bpp = 4;
    _grayLevel = 0;
    _colorMode = COLOR_MODE_COLORFUL;

    if (!allocateFrameBuffer()) {
        _bpp = 1;
        _colorMode = COLOR_MODE_NONE;
        if (allocateFrameBuffer()) memset(_frameBuffer, 0x00, frameBufferSize());
        return false;
    }

    // Fill with white (0x00 = TFT_EPD_WHITE)
    size_t bufSize = frameBufferSize();
    memset(_frameBuffer, 0x00, bufSize);
    return true;
}

bool Panel_EPaper::initBWRYMode() {
    if (_bpp == 4 && _colorMode == COLOR_MODE_BWRY)
        return _frameBuffer != nullptr;

    if (_bpp == 4 && _frameBuffer) {
        _grayLevel = 0;
        _colorMode = COLOR_MODE_BWRY;
        memset(_frameBuffer, 0x00, frameBufferSize());
        return true;
    }

    freeFrameBuffer();
    _bpp = 4;
    _grayLevel = 0;
    _colorMode = COLOR_MODE_BWRY;

    if (!allocateFrameBuffer()) {
        _bpp = 1;
        _colorMode = COLOR_MODE_NONE;
        if (allocateFrameBuffer()) memset(_frameBuffer, 0x00, frameBufferSize());
        return false;
    }

    // Fill with white (0x00 = TFT_EPD_WHITE)
    size_t bufSize = frameBufferSize();
    memset(_frameBuffer, 0x00, bufSize);
    return true;
}

// Temperature compensation

void Panel_EPaper::setTemp(float temp) {
    _temp = temp;
    if (!_sleeping) {
        ePaperSetTemp(_temp);
    }
}

float Panel_EPaper::getTemp() const {
    return _temp;
}

// Horizontal mirror

void Panel_EPaper::setHorizontalMirror(bool mirror) {
    _horizontalMirror = mirror;
    _mirrorOverride = true;
}

bool Panel_EPaper::horizontalMirror() const {
    if (_mirrorOverride) return _horizontalMirror;
    const bool oddRotation = (_rotation & 1U) != 0U;
    // Display-X maps to native-X on even rotations and native-Y on odd ones;
    // display-Y follows the opposite native axis.
    return _horizontalMirror ||
           (_displayHorizontalMirror && !oddRotation) ||
           (_displayVerticalMirror && oddRotation);
}

void Panel_EPaper::setVerticalMirror(bool mirror) {
    _verticalMirror = mirror;
    _mirrorOverride = true;
}

bool Panel_EPaper::verticalMirror() const {
    if (_mirrorOverride) return _verticalMirror;
    const bool oddRotation = (_rotation & 1U) != 0U;
    return _verticalMirror ||
           (_displayHorizontalMirror && oddRotation) ||
           (_displayVerticalMirror && !oddRotation);
}

void Panel_EPaper::setDisplayHorizontalMirror(bool mirror) {
    _displayHorizontalMirror = mirror;
}

void Panel_EPaper::setDisplayVerticalMirror(bool mirror) {
    _displayVerticalMirror = mirror;
}

// Internal: Frame buffer allocation

bool Panel_EPaper::allocateFrameBuffer() {
    freeFrameBuffer();

    size_t bufSize = frameBufferSize();
    if (bufSize == 0) return false;

    _frameBuffer = static_cast<uint8_t*>(allocateEPaperMemory(bufSize));
    return (_frameBuffer != nullptr);
}

size_t Panel_EPaper::frameStride() const {
    return (static_cast<size_t>(_init_width) * _bpp + 7U) / 8U;
}

size_t Panel_EPaper::frameBufferSize() const {
    return frameStride() * _init_height;
}

void Panel_EPaper::clearStoragePadding(uint8_t* buffer) {
    if (!buffer || _visible_width >= _init_width) return;

    const size_t stride = frameStride();
    if (_bpp == 1) {
        const size_t visibleBytes = (_visible_width + 7U) / 8U;
        const uint8_t visibleBits = static_cast<uint8_t>(_visible_width & 7U);
        for (uint16_t row = 0; row < _init_height; ++row) {
            uint8_t* rowData = buffer + static_cast<size_t>(row) * stride;
            if (visibleBits != 0 && visibleBytes != 0) {
                const uint8_t keepMask =
                    static_cast<uint8_t>(0xFFU << (8U - visibleBits));
                rowData[visibleBytes - 1U] &= keepMask;
            }
            if (visibleBytes < stride) {
                memset(rowData + visibleBytes, 0x00, stride - visibleBytes);
            }
        }
        return;
    }

    if (_bpp == 4) {
        const size_t visibleBytes = (_visible_width + 1U) / 2U;
        uint8_t whiteNibble = 0x00;
        if (_colorMode == COLOR_MODE_NONE && _grayLevel != 0) {
            whiteNibble = static_cast<uint8_t>(_grayLevel - 1U) & 0x0F;
        }
        const uint8_t whiteByte =
            static_cast<uint8_t>((whiteNibble << 4) | whiteNibble);
        for (uint16_t row = 0; row < _init_height; ++row) {
            uint8_t* rowData = buffer + static_cast<size_t>(row) * stride;
            if ((_visible_width & 1U) != 0 && visibleBytes != 0) {
                rowData[visibleBytes - 1U] =
                    static_cast<uint8_t>((rowData[visibleBytes - 1U] & 0xF0) |
                                         whiteNibble);
            }
            if (visibleBytes < stride) {
                memset(rowData + visibleBytes, whiteByte,
                       stride - visibleBytes);
            }
        }
    }
}

void Panel_EPaper::freeFrameBuffer() {
    if (_frameBuffer) {
        free(_frameBuffer);
        _frameBuffer = nullptr;
    }
    if (_oldFrameBuffer) {
        free(_oldFrameBuffer);
        _oldFrameBuffer = nullptr;
    }
}

// Internal: ePaper transfer helpers

void Panel_EPaper::pushOldColors(const uint8_t* data, size_t len) {
    _driver.pushOldColors(data, len);
}

void Panel_EPaper::pushNewColors(const uint8_t* data, size_t len) {
    _driver.pushNewColors(data, len);
}

void Panel_EPaper::pushGrayColors(const uint8_t* data, size_t len) {
    _driver.pushGrayColors(data, len);
}

bool Panel_EPaper::pushNewColorsFlip(const uint8_t* data,
                                     uint16_t bytesPerRow, uint16_t h) {
    // For horizontal flip: reverse bit order in each byte, reverse byte order per row
    if (!data || bytesPerRow == 0) return false;

    const size_t total = static_cast<size_t>(bytesPerRow) * h;
    uint8_t* flipped = static_cast<uint8_t*>(allocateEPaperMemory(total));
    if (!flipped) return false;

    for (uint16_t row = 0; row < h; row++) {
        const uint8_t* srcRow = data + (uint32_t)row * bytesPerRow;
        uint8_t* dstRow = flipped + static_cast<size_t>(row) * bytesPerRow;

        // Reverse byte order and bit order for this row
        for (uint16_t col = 0; col < bytesPerRow; col++) {
            uint8_t b = srcRow[bytesPerRow - 1 - col];
            if (_bpp == 4) {
                b = static_cast<uint8_t>((b << 4) | (b >> 4));
            } else {
                b = ((b & 0xF0) >> 4) | ((b & 0x0F) << 4);
                b = ((b & 0xCC) >> 2) | ((b & 0x33) << 2);
                b = ((b & 0xAA) >> 1) | ((b & 0x55) << 1);
            }
            dstRow[col] = b;
        }
    }

    _driver.pushNewColors(flipped, total);
    free(flipped);
    return true;
}

bool Panel_EPaper::pushOldColorsFlip(const uint8_t* data,
                                    uint16_t bytesPerRow, uint16_t h) {
    if (!data || bytesPerRow == 0) return false;
    const size_t total = static_cast<size_t>(bytesPerRow) * h;
    uint8_t* flipped = static_cast<uint8_t*>(allocateEPaperMemory(total));
    if (!flipped) return false;
    for (uint16_t row = 0; row < h; ++row) {
        const uint8_t* srcRow = data + static_cast<size_t>(row) * bytesPerRow;
        uint8_t* dstRow = flipped + static_cast<size_t>(row) * bytesPerRow;
        for (uint16_t col = 0; col < bytesPerRow; ++col) {
            uint8_t b = srcRow[bytesPerRow - 1 - col];
            b = ((b & 0xF0) >> 4) | ((b & 0x0F) << 4);
            b = ((b & 0xCC) >> 2) | ((b & 0x33) << 2);
            b = ((b & 0xAA) >> 1) | ((b & 0x55) << 1);
            dstRow[col] = b;
        }
    }
    _driver.pushOldColors(flipped, total);
    free(flipped);
    return true;
}

bool Panel_EPaper::pushGrayColorsFlip(const uint8_t* data,
                                     uint16_t bytesPerRow, uint16_t h) {
    if (!data || bytesPerRow == 0) return false;
    const size_t total = static_cast<size_t>(bytesPerRow) * h;
    uint8_t* flipped = static_cast<uint8_t*>(allocateEPaperMemory(total));
    if (!flipped) return false;
    for (uint16_t row = 0; row < h; ++row) {
        const uint8_t* srcRow = data + static_cast<size_t>(row) * bytesPerRow;
        uint8_t* dstRow = flipped + static_cast<size_t>(row) * bytesPerRow;
        for (uint16_t col = 0; col < bytesPerRow; ++col) {
            const uint8_t b = srcRow[bytesPerRow - 1 - col];
            dstRow[col] = static_cast<uint8_t>((b << 4) | (b >> 4));
        }
    }
    _driver.pushGrayColors(flipped, total);
    free(flipped);
    return true;
}

void Panel_EPaper::ePaperUpdate() {
    _driver.update();
}

void Panel_EPaper::ePaperUpdateFast() {
    _driver.updateFast();
}

void Panel_EPaper::ePaperUpdatePartial() {
    _driver.updatePartial();
}

void Panel_EPaper::ePaperWakeUp() {
    _driver.wake();
    // Set temperature compensation
    ePaperSetTemp(_temp);
}

void Panel_EPaper::ePaperSleep() {
    // Delegate to driver (UC8179 and ED2208 both power off, wait for BUSY,
    // then enter controller deep sleep with 0x07 + 0xA5).
    _driver.sleep();
    delay(100);
}

void Panel_EPaper::ePaperSetTemp(float temp) {
    if (_driver.supportsTemperatureCompensation()) {
        _driver.setTemperature(static_cast<int8_t>(temp));
    }
}

GfxResult Panel_EPaper::driverOperationResult() const {
    if (_bus.lastError() != 0) {
        return GfxResult(GfxError::CommunicationFailed,
                         "display bus communication failed");
    }
    switch (_driver.lastOperationError()) {
        case DriverOperationError::None:
            return GfxResult::success();
        case DriverOperationError::BusyTimeout:
            return GfxResult(GfxError::BusyTimeout,
                             "ePaper controller BUSY wait timed out");
        case DriverOperationError::CommunicationFailed:
            return GfxResult(GfxError::CommunicationFailed,
                             "display bus communication failed");
    }
    return GfxResult(GfxError::CommunicationFailed,
                     "unknown display driver failure");
}
