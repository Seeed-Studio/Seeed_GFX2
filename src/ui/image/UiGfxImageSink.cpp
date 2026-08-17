#if !defined(ARDUINO_ARCH_NRF52)
/* UI layer skipped on nRF52 (Seeed core uses -std=gnu++11; UI needs C++14/17
   aggregate features). Core graphics + ePaper do not depend on the UI. */
#include "UiImageDecoders.h"
#include "../../Seeed_GFX.h"

UiStatus UiGfxImageSink::writeRgb565(int16_t x, int16_t y,
                                     const uint16_t* pixels, uint16_t count) {
    if (!pixels || !count) return UiStatus::InvalidArgument;
    _gfx.pushImage(x, y, count, 1, pixels);
    return UiStatus::Ok;
}

UiStatus UiGfxImageSink::writeRgba8888(int16_t x, int16_t y,
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
            const uint16_t foreground = _gfx.color565(rgba[0], rgba[1], rgba[2]);
            converted[i] = rgba[3] == 255U ? foreground
                : _gfx.alphaBlend(rgba[3], foreground,
                    _gfx.readPixel(static_cast<int32_t>(x) + offset + i, y));
        }
        _gfx.pushImage(static_cast<int32_t>(x) + offset, y, chunk, 1, converted);
        offset = static_cast<uint16_t>(offset + chunk);
    }
    return UiStatus::Ok;
}

#endif // !ARDUINO_ARCH_NRF52 (UI layer)
