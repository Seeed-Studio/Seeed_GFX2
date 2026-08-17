#ifndef SEEED_UI_IMAGE_DECODER_H
#define SEEED_UI_IMAGE_DECODER_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "../UiStatus.h"
#include "../UiTypes.h"

enum class UiImageFormat : uint8_t {
    Auto = 0, RawRGB565, Mono1, Indexed4, Indexed8, BMP, JPEG, PNG, QOI
};

struct UiImageInfo {
    uint16_t width = 0;
    uint16_t height = 0;
    UiImageFormat format = UiImageFormat::Auto;
    bool hasAlpha = false;
};

class IUiDataSource {
public:
    virtual ~IUiDataSource() = default;
    virtual uint32_t size() const = 0;
    virtual size_t readAt(uint32_t offset, void* destination, size_t length) = 0;
};

class UiMemorySource : public IUiDataSource {
public:
    UiMemorySource(const void* data, uint32_t size)
        : _data(static_cast<const uint8_t*>(data)), _size(size) {}
    uint32_t size() const override { return _size; }
    size_t readAt(uint32_t offset, void* destination, size_t length) override {
        if (!destination || !_data || offset > _size || length > _size - offset) return 0;
        memcpy(destination, _data + offset, length);
        return length;
    }
private:
    const uint8_t* _data;
    uint32_t _size;
};

/** Random-access source adapter for files, flash partitions, network caches,
 *  or any other storage that can service a positional read callback. */
class UiCallbackSource : public IUiDataSource {
public:
    using ReadAt = size_t (*)(void* context, uint32_t offset,
                              void* destination, size_t length);
    UiCallbackSource(uint32_t sourceSize, ReadAt reader, void* context = nullptr)
        : _size(sourceSize), _reader(reader), _context(context) {}
    uint32_t size() const override { return _size; }
    size_t readAt(uint32_t offset, void* destination, size_t length) override {
        if (!_reader || !destination || offset > _size || length > _size - offset)
            return 0;
        return _reader(_context, offset, destination, length);
    }
private:
    uint32_t _size;
    ReadAt _reader;
    void* _context;
};

class IUiImageSink {
public:
    virtual ~IUiImageSink() = default;
    virtual UiStatus writeRgb565(int16_t x, int16_t y,
                                 const uint16_t* pixels,
                                 uint16_t count) = 0;
    /** RGBA8888 fallback composites over black. Display sinks can override
     *  this to alpha-blend against their actual destination pixels. */
    virtual UiStatus writeRgba8888(int16_t x, int16_t y,
                                   const uint8_t* pixels,
                                   uint16_t count) {
        if (!pixels || !count) return UiStatus::InvalidArgument;
        uint16_t converted[32];
        uint16_t offset = 0;
        while (offset < count) {
            const uint16_t chunk = static_cast<uint16_t>(
                count - offset > 32 ? 32 : count - offset);
            for (uint16_t i = 0; i < chunk; ++i) {
                const uint8_t* rgba = pixels + static_cast<size_t>(offset + i) * 4U;
                const uint16_t alpha = rgba[3];
                const uint8_t r = static_cast<uint8_t>((rgba[0] * alpha + 127U) / 255U);
                const uint8_t g = static_cast<uint8_t>((rgba[1] * alpha + 127U) / 255U);
                const uint8_t b = static_cast<uint8_t>((rgba[2] * alpha + 127U) / 255U);
                converted[i] = static_cast<uint16_t>(((r & 0xF8U) << 8) |
                                                      ((g & 0xFCU) << 3) | (b >> 3));
            }
            const UiStatus status = writeRgb565(
                static_cast<int16_t>(x + offset), y, converted, chunk);
            if (!uiOk(status)) return status;
            offset = static_cast<uint16_t>(offset + chunk);
        }
        return UiStatus::Ok;
    }
};

class IUiImageDecoder {
public:
    virtual ~IUiImageDecoder() = default;
    virtual UiImageFormat format() const = 0;
    virtual bool probe(IUiDataSource& source) = 0;
    virtual UiStatus readInfo(IUiDataSource& source, UiImageInfo& information) = 0;
    /** Work bytes required after the most recent successful readInfo(). */
    virtual size_t requiredWorkBytes() const = 0;
    virtual UiStatus begin(IUiDataSource& source, const UiPoint& destination,
                           void* workBuffer, size_t workBytes) = 0;
    virtual UiStatus step(IUiImageSink& sink, uint16_t maxRows) = 0;
    virtual bool finished() const = 0;
    virtual void cancel() = 0;
};

#endif
