#include "Driver_RGB565.h"
#include "../../bus/Bus_ESP32RGB.h"

Driver_RGB565::Driver_RGB565(uint16_t width, uint16_t height,
                             const char* driverName)
    : _width(width), _height(height), _name(driverName), _rgbBus(nullptr),
      _windowX0(0), _windowY0(0), _windowX1(width ? width - 1 : 0),
      _windowY1(height ? height - 1 : 0), _cursorX(0), _cursorY(0),
      _inverted(false) {
    IDriver::_width = width;
    IDriver::_height = height;
}

bool Driver_RGB565::init(IBus& bus) {
    _bus = &bus;
    // ProductCatalog constructs this driver only with Bus_ESP32RGB. Avoid
    // RTTI because the ESP32 Arduino core is built with -fno-rtti.
    _rgbBus = static_cast<Bus_ESP32RGB*>(&bus);
    if (!_rgbBus->framebuffer()) {
        setOperationError(DriverOperationError::CommunicationFailed);
        return false;
    }
    setAddrWindow(0, 0, width() - 1, height() - 1);
    return true;
}

void Driver_RGB565::setRotation(uint8_t rotation) {
    _rotation = rotation & 3U;
    setAddrWindow(0, 0, width() - 1, height() - 1);
}

void Driver_RGB565::invertDisplay(bool invert) {
    if (invert == _inverted) return;
    if (!_rgbBus || !_rgbBus->framebuffer()) {
        _inverted = invert;
        return;
    }

    // RGB panels have no command channel once scanning starts. Invert the
    // complete draw framebuffer so the already-visible image changes
    // immediately, then retain the flag so future logical colors are stored
    // in the same inverted representation. Nested write transactions are
    // supported by Bus_ESP32RGB.
    _rgbBus->beginWrite();
    uint16_t* frame = _rgbBus->framebuffer();
    const size_t pixelCount = static_cast<size_t>(_width) * _height;
    for (size_t i = 0; i < pixelCount; ++i) {
        frame[i] = static_cast<uint16_t>(~frame[i]);
    }
    if (!_rgbBus->flushFramebuffer(frame, pixelCount * sizeof(uint16_t))) {
        setOperationError(DriverOperationError::CommunicationFailed);
    }
    _rgbBus->endWrite();
    _inverted = invert;
}

void Driver_RGB565::displayOn() {
    if (_rgbBus && !_rgbBus->setDisplayEnabled(true)) {
        setOperationError(DriverOperationError::CommunicationFailed);
    }
}

void Driver_RGB565::displayOff() {
    if (_rgbBus && !_rgbBus->setDisplayEnabled(false)) {
        setOperationError(DriverOperationError::CommunicationFailed);
    }
}

void Driver_RGB565::setAddrWindow(uint16_t xs, uint16_t ys,
                                  uint16_t xe, uint16_t ye) {
    if (xs >= width() || ys >= height() || xe < xs || ye < ys) {
        _windowX0 = _windowX1 = _cursorX = 0;
        _windowY0 = _windowY1 = _cursorY = 0;
        return;
    }
    _windowX0 = xs;
    _windowY0 = ys;
    _windowX1 = xe < width() ? xe : static_cast<uint16_t>(width() - 1);
    _windowY1 = ye < height() ? ye : static_cast<uint16_t>(height() - 1);
    _cursorX = _windowX0;
    _cursorY = _windowY0;
}

bool Driver_RGB565::mapPoint(uint16_t x, uint16_t y, uint16_t& nativeX,
                             uint16_t& nativeY) const {
    if (x >= width() || y >= height()) return false;
    switch (_rotation) {
        case 0:
            nativeX = x;
            nativeY = y;
            break;
        case 1:
            nativeX = static_cast<uint16_t>(_width - 1U - y);
            nativeY = x;
            break;
        case 2:
            nativeX = static_cast<uint16_t>(_width - 1U - x);
            nativeY = static_cast<uint16_t>(_height - 1U - y);
            break;
        default:
            nativeX = y;
            nativeY = static_cast<uint16_t>(_height - 1U - x);
            break;
    }
    return nativeX < _width && nativeY < _height;
}

void Driver_RGB565::advanceCursor() {
    if (_cursorX < _windowX1) {
        ++_cursorX;
    } else {
        _cursorX = _windowX0;
        if (_cursorY < _windowY1) ++_cursorY;
        else _cursorY = _windowY0;
    }
}

void Driver_RGB565::writePixel(uint16_t color) {
    if (!_rgbBus) return;
    uint16_t nx = 0;
    uint16_t ny = 0;
    if (mapPoint(_cursorX, _cursorY, nx, ny)) {
        uint16_t* pixel = _rgbBus->framebuffer() +
            static_cast<size_t>(ny) * _width + nx;
        *pixel = outputColor(color);
        if (!_rgbBus->flushFramebuffer(pixel, sizeof(*pixel))) {
            setOperationError(DriverOperationError::CommunicationFailed);
        }
    }
    advanceCursor();
}

void Driver_RGB565::writePixels(const uint16_t* data, size_t len) {
    if (!data || !_rgbBus) return;
    size_t first = static_cast<size_t>(-1);
    size_t last = 0;
    for (size_t i = 0; i < len; ++i) {
        uint16_t nx = 0;
        uint16_t ny = 0;
        if (mapPoint(_cursorX, _cursorY, nx, ny)) {
            const size_t index = static_cast<size_t>(ny) * _width + nx;
            _rgbBus->framebuffer()[index] = outputColor(data[i]);
            if (index < first) first = index;
            if (index > last) last = index;
        }
        advanceCursor();
    }
    if (first != static_cast<size_t>(-1) &&
        !_rgbBus->flushFramebuffer(
            _rgbBus->framebuffer() + first,
            (last - first + 1U) * sizeof(uint16_t))) {
        setOperationError(DriverOperationError::CommunicationFailed);
    }
}

void Driver_RGB565::writeFill(uint16_t color, size_t len) {
    if (!_rgbBus) return;
    const uint16_t output = outputColor(color);
    size_t first = static_cast<size_t>(-1);
    size_t last = 0;
    for (size_t i = 0; i < len; ++i) {
        uint16_t nx = 0;
        uint16_t ny = 0;
        if (mapPoint(_cursorX, _cursorY, nx, ny)) {
            const size_t index = static_cast<size_t>(ny) * _width + nx;
            _rgbBus->framebuffer()[index] = output;
            if (index < first) first = index;
            if (index > last) last = index;
        }
        advanceCursor();
    }
    if (first != static_cast<size_t>(-1) &&
        !_rgbBus->flushFramebuffer(
            _rgbBus->framebuffer() + first,
            (last - first + 1U) * sizeof(uint16_t))) {
        setOperationError(DriverOperationError::CommunicationFailed);
    }
}

uint16_t Driver_RGB565::readPixel(uint16_t x, uint16_t y) {
    if (!_rgbBus || !_rgbBus->framebuffer()) return 0;
    uint16_t nx = 0;
    uint16_t ny = 0;
    if (!mapPoint(x, y, nx, ny)) return 0;
    const uint16_t color =
        _rgbBus->framebuffer()[static_cast<size_t>(ny) * _width + nx];
    return outputColor(color);
}
