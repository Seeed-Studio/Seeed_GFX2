#ifndef SEEED_UI_IMAGE_DECODERS_H
#define SEEED_UI_IMAGE_DECODERS_H

#include "UiImageDecoder.h"

class Seeed_GFX;

class UiGfxImageSink : public IUiImageSink {
public:
    explicit UiGfxImageSink(Seeed_GFX& gfx) : _gfx(gfx) {}
    UiStatus writeRgb565(int16_t x, int16_t y, const uint16_t* pixels,
                         uint16_t count) override;
    UiStatus writeRgba8888(int16_t x, int16_t y, const uint8_t* pixels,
                           uint16_t count) override;
private:
    Seeed_GFX& _gfx;
};

class UiImageDecoderRegistry {
public:
    UiStatus add(IUiImageDecoder& decoder);
    IUiImageDecoder* find(UiImageFormat format, IUiDataSource& source) const;
private:
    static constexpr size_t Capacity = 6;
    IUiImageDecoder* _decoders[Capacity] = {};
    size_t _count = 0;
};

class BmpDecoder : public IUiImageDecoder {
public:
    UiImageFormat format() const override { return UiImageFormat::BMP; }
    bool probe(IUiDataSource& source) override;
    UiStatus readInfo(IUiDataSource& source, UiImageInfo& information) override;
    UiStatus begin(IUiDataSource& source, const UiPoint& destination,
                   void* workBuffer, size_t workBytes) override;
    UiStatus step(IUiImageSink& sink, uint16_t maxRows) override;
    bool finished() const override { return _source && _row >= _height; }
    void cancel() override;
    size_t requiredWorkBytes() const override { return _requiredWorkBytes; }

private:
    UiStatus parse(IUiDataSource& source);
    static uint16_t read16(const uint8_t* data);
    static uint32_t read32(const uint8_t* data);

    IUiDataSource* _source = nullptr;
    uint8_t* _work = nullptr;
    size_t _workBytes = 0;
    UiPoint _destination;
    uint32_t _pixelOffset = 0;
    uint32_t _rowStride = 0;
    size_t _requiredWorkBytes = 0;
    uint16_t _width = 0;
    uint16_t _height = 0;
    uint16_t _row = 0;
    uint8_t _bitsPerPixel = 0;
    bool _topDown = false;
    uint32_t _redMask = 0, _greenMask = 0, _blueMask = 0, _alphaMask = 0;
    uint16_t _palette[256] = {};
    uint16_t _paletteSize = 0;
};

class QoiDecoder : public IUiImageDecoder {
public:
    UiImageFormat format() const override { return UiImageFormat::QOI; }
    bool probe(IUiDataSource& source) override;
    UiStatus readInfo(IUiDataSource& source, UiImageInfo& information) override;
    size_t requiredWorkBytes() const override {
        return static_cast<size_t>(_width) * 4U;
    }
    UiStatus begin(IUiDataSource& source, const UiPoint& destination,
                   void* workBuffer, size_t workBytes) override;
    UiStatus step(IUiImageSink& sink, uint16_t maxRows) override;
    bool finished() const override { return _source && _pixel >= _pixelCount; }
    void cancel() override;

private:
    struct Pixel { uint8_t r, g, b, a; };
    static uint32_t readBig32(const uint8_t* data);
    static uint8_t hash(const Pixel& pixel);
    bool readByte(uint8_t& value);
    bool decodePixel(Pixel& pixel);

    IUiDataSource* _source = nullptr;
    uint8_t* _rowBuffer = nullptr;
    UiPoint _destination;
    uint32_t _offset = 14;
    uint32_t _pixel = 0;
    uint32_t _pixelCount = 0;
    uint16_t _width = 0, _height = 0;
    uint8_t _channels = 0;
    uint8_t _run = 0;
    Pixel _current = {0, 0, 0, 255};
    Pixel _index[64] = {};
};

#endif
