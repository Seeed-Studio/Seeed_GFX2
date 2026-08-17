#if !defined(ARDUINO_ARCH_NRF52)
/* UI layer skipped on nRF52 (Seeed core uses -std=gnu++11; UI needs C++14/17
   aggregate features). Core graphics + ePaper do not depend on the UI. */
#include "UiImageDecoders.h"

namespace {
uint8_t expandMaskedChannel(uint32_t pixel, uint32_t mask) {
    if (!mask) return 0;
    uint8_t shift = 0;
    while ((mask & 1U) == 0U) { mask >>= 1; ++shift; }
    const uint32_t value = (pixel >> shift) & mask;
    return static_cast<uint8_t>((value * 255U + mask / 2U) / mask);
}
}

UiStatus UiImageDecoderRegistry::add(IUiImageDecoder& decoder) {
    if (_count >= Capacity) return UiStatus::CapacityExceeded;
    _decoders[_count++] = &decoder;
    return UiStatus::Ok;
}

IUiImageDecoder* UiImageDecoderRegistry::find(UiImageFormat format,
                                               IUiDataSource& source) const {
    for (size_t i = 0; i < _count; ++i) {
        if (format != UiImageFormat::Auto && _decoders[i]->format() != format) continue;
        if (_decoders[i]->probe(source)) return _decoders[i];
    }
    return nullptr;
}

uint16_t BmpDecoder::read16(const uint8_t* data) {
    return static_cast<uint16_t>(data[0] | (static_cast<uint16_t>(data[1]) << 8));
}
uint32_t BmpDecoder::read32(const uint8_t* data) {
    return static_cast<uint32_t>(data[0]) |
           (static_cast<uint32_t>(data[1]) << 8) |
           (static_cast<uint32_t>(data[2]) << 16) |
           (static_cast<uint32_t>(data[3]) << 24);
}

bool BmpDecoder::probe(IUiDataSource& source) {
    uint8_t signature[2] = {};
    return source.readAt(0, signature, sizeof(signature)) == sizeof(signature) &&
           signature[0] == 'B' && signature[1] == 'M';
}

UiStatus BmpDecoder::parse(IUiDataSource& source) {
    uint8_t header[54] = {};
    if (source.readAt(0, header, sizeof(header)) != sizeof(header)) return UiStatus::DataError;
    if (header[0] != 'B' || header[1] != 'M') return UiStatus::DataError;
    const uint32_t dibSize = read32(header + 14);
    const int32_t width = static_cast<int32_t>(read32(header + 18));
    const int32_t signedHeight = static_cast<int32_t>(read32(header + 22));
    const uint16_t planes = read16(header + 26);
    const uint16_t bpp = read16(header + 28);
    const uint32_t compression = read32(header + 30);
    const uint32_t pixelOffset = read32(header + 10);
    if (dibSize < 40 || width <= 0 || signedHeight == 0 || planes != 1 ||
        (bpp != 1 && bpp != 4 && bpp != 8 && bpp != 16 &&
         bpp != 24 && bpp != 32) ||
        (compression != 0 && compression != 3) ||
        (compression == 3 && bpp != 16 && bpp != 32) ||
        width > 32767 || signedHeight == INT32_MIN) return UiStatus::Unsupported;
    const uint32_t height = signedHeight < 0
        ? static_cast<uint32_t>(-signedHeight) : static_cast<uint32_t>(signedHeight);
    if (height > 32767) return UiStatus::Unsupported;
    const uint64_t rowBits = static_cast<uint64_t>(width) * bpp;
    const uint64_t stride = ((rowBits + 31U) / 32U) * 4U;
    const uint64_t end = static_cast<uint64_t>(pixelOffset) + stride * height;
    if (stride > 0xFFFFFFFFULL || end > source.size()) return UiStatus::DataError;

    _paletteSize = 0;
    _redMask = _greenMask = _blueMask = _alphaMask = 0;
    if (bpp <= 8) {
        uint32_t colorCount = read32(header + 46);
        if (!colorCount) colorCount = 1UL << bpp;
        if (colorCount > 256) return UiStatus::Unsupported;
        const uint64_t paletteOffset = 14ULL + dibSize;
        const uint64_t paletteEnd = paletteOffset + colorCount * 4ULL;
        if (paletteEnd > pixelOffset || paletteEnd > source.size())
            return UiStatus::DataError;
        uint8_t entry[4];
        for (uint32_t i = 0; i < colorCount; ++i) {
            if (source.readAt(static_cast<uint32_t>(paletteOffset + i * 4U),
                              entry, sizeof(entry)) != sizeof(entry))
                return UiStatus::IoError;
            _palette[i] = static_cast<uint16_t>(((entry[2] & 0xF8U) << 8) |
                                                ((entry[1] & 0xFCU) << 3) |
                                                (entry[0] >> 3));
        }
        _paletteSize = static_cast<uint16_t>(colorCount);
    } else if (bpp == 16 || bpp == 32) {
        if (compression == 0) {
            _redMask = bpp == 16 ? 0x00007C00UL : 0x00FF0000UL;
            _greenMask = bpp == 16 ? 0x000003E0UL : 0x0000FF00UL;
            _blueMask = bpp == 16 ? 0x0000001FUL : 0x000000FFUL;
            _alphaMask = bpp == 32 ? 0xFF000000UL : 0;
        } else {
            uint8_t masks[16] = {};
            const uint32_t maskOffset = dibSize >= 52 ? 54U
                                                       : static_cast<uint32_t>(14U + dibSize);
            const size_t maskBytes = (dibSize >= 56 || pixelOffset >= maskOffset + 16U)
                ? sizeof(masks) : 12U;
            if (source.readAt(maskOffset, masks, maskBytes) != maskBytes)
                return UiStatus::DataError;
            _redMask = read32(masks); _greenMask = read32(masks + 4);
            _blueMask = read32(masks + 8);
            if (maskBytes == 16) _alphaMask = read32(masks + 12);
            if (!_redMask || !_greenMask || !_blueMask) return UiStatus::DataError;
        }
    }
    _width = static_cast<uint16_t>(width);
    _height = static_cast<uint16_t>(height);
    _topDown = signedHeight < 0;
    _bitsPerPixel = static_cast<uint8_t>(bpp);
    _pixelOffset = pixelOffset;
    _rowStride = static_cast<uint32_t>(stride);
    const uint64_t outputOffset = (stride + 1ULL) & ~1ULL;
    const uint64_t required = outputOffset + static_cast<uint64_t>(_width) * 2ULL;
    if (required > SIZE_MAX) return UiStatus::Unsupported;
    _requiredWorkBytes = static_cast<size_t>(required);
    return UiStatus::Ok;
}

UiStatus BmpDecoder::readInfo(IUiDataSource& source, UiImageInfo& information) {
    const UiStatus status = parse(source);
    if (!uiOk(status)) return status;
    information.width = _width; information.height = _height;
    information.format = UiImageFormat::BMP;
    information.hasAlpha = false;
    return UiStatus::Ok;
}

UiStatus BmpDecoder::begin(IUiDataSource& source, const UiPoint& destination,
                           void* workBuffer, size_t workBytes) {
    const UiStatus status = parse(source);
    if (!uiOk(status)) return status;
    if (!workBuffer || workBytes < _requiredWorkBytes ||
        (reinterpret_cast<uintptr_t>(workBuffer) & 1U)) return UiStatus::InvalidArgument;
    _source = &source; _destination = destination;
    _work = static_cast<uint8_t*>(workBuffer); _workBytes = workBytes; _row = 0;
    return UiStatus::Ok;
}

UiStatus BmpDecoder::step(IUiImageSink& sink, uint16_t maxRows) {
    if (!_source || !_work) return UiStatus::NotInitialized;
    if (!maxRows) return finished() ? UiStatus::Ok : UiStatus::Busy;
    uint16_t completed = 0;
    while (_row < _height && completed < maxRows) {
        const uint32_t fileRow = _topDown ? _row : (_height - 1U - _row);
        const uint32_t offset = _pixelOffset + fileRow * _rowStride;
        if (_source->readAt(offset, _work, _rowStride) != _rowStride)
            return UiStatus::IoError;
        uint16_t* output = reinterpret_cast<uint16_t*>(
            _work + ((_rowStride + 1U) & ~1U));
        if (_bitsPerPixel == 24) {
            for (uint16_t x = 0; x < _width; ++x) {
                const uint8_t b = _work[x * 3U];
                const uint8_t g = _work[x * 3U + 1U];
                const uint8_t r = _work[x * 3U + 2U];
                output[x] = static_cast<uint16_t>(((r & 0xF8U) << 8) |
                                                   ((g & 0xFCU) << 3) | (b >> 3));
            }
        } else if (_bitsPerPixel == 1 || _bitsPerPixel == 4 ||
                   _bitsPerPixel == 8) {
            for (uint16_t x = 0; x < _width; ++x) {
                uint8_t index;
                if (_bitsPerPixel == 1)
                    index = static_cast<uint8_t>((_work[x >> 3] >>
                                                  (7U - (x & 7U))) & 1U);
                else if (_bitsPerPixel == 4)
                    index = static_cast<uint8_t>((x & 1U)
                        ? (_work[x >> 1] & 0x0FU) : (_work[x >> 1] >> 4));
                else index = _work[x];
                if (index >= _paletteSize) return UiStatus::DataError;
                output[x] = _palette[index];
            }
        } else if (_bitsPerPixel == 16 && _redMask == 0x00007C00UL &&
                   _greenMask == 0x000003E0UL && _blueMask == 0x0000001FUL) {
            for (uint16_t x = 0; x < _width; ++x) {
                const uint16_t rgb555 = read16(_work + x * 2U);
                const uint16_t r = (rgb555 >> 10) & 0x1FU;
                const uint16_t g = (rgb555 >> 5) & 0x1FU;
                const uint16_t b = rgb555 & 0x1FU;
                output[x] = static_cast<uint16_t>((r << 11) | ((g << 1) << 5) | b);
            }
        } else {
            const uint8_t bytesPerPixel = static_cast<uint8_t>(_bitsPerPixel / 8U);
            for (uint16_t x = 0; x < _width; ++x) {
                const uint8_t* sourcePixel = _work + static_cast<size_t>(x) * bytesPerPixel;
                const uint32_t packed = bytesPerPixel == 2
                    ? read16(sourcePixel) : read32(sourcePixel);
                const uint8_t r = expandMaskedChannel(packed, _redMask);
                const uint8_t g = expandMaskedChannel(packed, _greenMask);
                const uint8_t b = expandMaskedChannel(packed, _blueMask);
                output[x] = static_cast<uint16_t>(((r & 0xF8U) << 8) |
                                                   ((g & 0xFCU) << 3) | (b >> 3));
            }
        }
        const UiStatus writeStatus = sink.writeRgb565(_destination.x,
            uiClamp16(static_cast<int32_t>(_destination.y) + _row), output, _width);
        if (!uiOk(writeStatus)) return writeStatus;
        ++_row; ++completed;
    }
    return finished() ? UiStatus::Ok : UiStatus::Busy;
}

void BmpDecoder::cancel() {
    _source = nullptr; _work = nullptr; _workBytes = 0; _row = 0;
}

uint32_t QoiDecoder::readBig32(const uint8_t* data) {
    return (static_cast<uint32_t>(data[0]) << 24) |
           (static_cast<uint32_t>(data[1]) << 16) |
           (static_cast<uint32_t>(data[2]) << 8) | data[3];
}

uint8_t QoiDecoder::hash(const Pixel& pixel) {
    return static_cast<uint8_t>((pixel.r * 3U + pixel.g * 5U +
                                 pixel.b * 7U + pixel.a * 11U) & 63U);
}

bool QoiDecoder::probe(IUiDataSource& source) {
    uint8_t magic[4] = {};
    return source.readAt(0, magic, sizeof(magic)) == sizeof(magic) &&
           magic[0] == 'q' && magic[1] == 'o' &&
           magic[2] == 'i' && magic[3] == 'f';
}

UiStatus QoiDecoder::readInfo(IUiDataSource& source,
                              UiImageInfo& information) {
    uint8_t header[14] = {};
    if (source.readAt(0, header, sizeof(header)) != sizeof(header))
        return UiStatus::DataError;
    if (header[0] != 'q' || header[1] != 'o' ||
        header[2] != 'i' || header[3] != 'f') return UiStatus::DataError;
    const uint32_t width = readBig32(header + 4);
    const uint32_t height = readBig32(header + 8);
    if (!width || !height || width > 32767U || height > 32767U ||
        (header[12] != 3 && header[12] != 4) || header[13] > 1 ||
        static_cast<uint64_t>(width) * height > 0xFFFFFFFFULL ||
        source.size() < 22U) return UiStatus::Unsupported;
    _width = static_cast<uint16_t>(width);
    _height = static_cast<uint16_t>(height);
    _channels = header[12];
    _pixelCount = width * height;
    information.width = _width; information.height = _height;
    information.format = UiImageFormat::QOI;
    information.hasAlpha = _channels == 4;
    return UiStatus::Ok;
}

UiStatus QoiDecoder::begin(IUiDataSource& source, const UiPoint& destination,
                           void* workBuffer, size_t workBytes) {
    UiImageInfo information;
    const UiStatus status = readInfo(source, information);
    if (!uiOk(status)) return status;
    if (!workBuffer || workBytes < requiredWorkBytes() ||
        (reinterpret_cast<uintptr_t>(workBuffer) & 1U))
        return UiStatus::InvalidArgument;
    _source = &source; _rowBuffer = static_cast<uint8_t*>(workBuffer);
    _destination = destination; _offset = 14; _pixel = 0; _run = 0;
    _current = Pixel{0, 0, 0, 255};
    memset(_index, 0, sizeof(_index));
    return UiStatus::Ok;
}

bool QoiDecoder::readByte(uint8_t& value) {
    if (!_source || _offset >= _source->size() - 8U) return false;
    if (_source->readAt(_offset, &value, 1) != 1) return false;
    ++_offset;
    return true;
}

bool QoiDecoder::decodePixel(Pixel& pixel) {
    if (_run) { --_run; pixel = _current; return true; }
    uint8_t tag;
    if (!readByte(tag)) return false;
    if (tag == 0xFE) {
        if (!readByte(_current.r) || !readByte(_current.g) ||
            !readByte(_current.b)) return false;
    } else if (tag == 0xFF) {
        if (!readByte(_current.r) || !readByte(_current.g) ||
            !readByte(_current.b) || !readByte(_current.a)) return false;
    } else if ((tag & 0xC0U) == 0x00U) {
        _current = _index[tag & 0x3FU];
    } else if ((tag & 0xC0U) == 0x40U) {
        _current.r = static_cast<uint8_t>(_current.r + ((tag >> 4) & 3U) - 2U);
        _current.g = static_cast<uint8_t>(_current.g + ((tag >> 2) & 3U) - 2U);
        _current.b = static_cast<uint8_t>(_current.b + (tag & 3U) - 2U);
    } else if ((tag & 0xC0U) == 0x80U) {
        uint8_t second;
        if (!readByte(second)) return false;
        const int16_t dg = static_cast<int16_t>(tag & 0x3FU) - 32;
        _current.r = static_cast<uint8_t>(_current.r + dg +
                    static_cast<int16_t>((second >> 4) & 0x0FU) - 8);
        _current.g = static_cast<uint8_t>(_current.g + dg);
        _current.b = static_cast<uint8_t>(_current.b + dg +
                    static_cast<int16_t>(second & 0x0FU) - 8);
    } else {
        _run = static_cast<uint8_t>(tag & 0x3FU);
    }
    _index[hash(_current)] = _current;
    pixel = _current;
    return true;
}

UiStatus QoiDecoder::step(IUiImageSink& sink, uint16_t maxRows) {
    if (!_source || !_rowBuffer) return UiStatus::NotInitialized;
    if (!maxRows) return finished() ? UiStatus::Ok : UiStatus::Busy;
    uint16_t rows = 0;
    while (_pixel < _pixelCount && rows < maxRows) {
        const uint16_t row = static_cast<uint16_t>(_pixel / _width);
        for (uint16_t x = 0; x < _width; ++x) {
            Pixel pixel;
            if (!decodePixel(pixel)) return UiStatus::DataError;
            uint8_t* output = _rowBuffer + static_cast<size_t>(x) * 4U;
            output[0] = pixel.r; output[1] = pixel.g;
            output[2] = pixel.b; output[3] = pixel.a;
            ++_pixel;
        }
        const UiStatus status = sink.writeRgba8888(
            _destination.x, uiClamp16(static_cast<int32_t>(_destination.y) + row),
            _rowBuffer, _width);
        if (!uiOk(status)) return status;
        ++rows;
    }
    if (finished()) {
        static const uint8_t endMarker[8] = {0, 0, 0, 0, 0, 0, 0, 1};
        uint8_t actual[8] = {};
        if (_source->readAt(_offset, actual, sizeof(actual)) != sizeof(actual) ||
            memcmp(actual, endMarker, sizeof(actual)) != 0)
            return UiStatus::DataError;
        return UiStatus::Ok;
    }
    return UiStatus::Busy;
}

void QoiDecoder::cancel() {
    _source = nullptr; _rowBuffer = nullptr; _pixel = 0; _pixelCount = 0;
    _offset = 14; _run = 0;
}

#endif // !ARDUINO_ARCH_NRF52 (UI layer)
