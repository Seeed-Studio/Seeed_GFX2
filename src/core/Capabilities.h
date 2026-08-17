#ifndef SEEED_GFX_CAPABILITIES_H
#define SEEED_GFX_CAPABILITIES_H

#include <stdint.h>
#include <stddef.h>

enum class PixelFormat : uint8_t {
    Unknown = 0,
    Mono1,
    Gray4,
    Indexed4,
    RGB565,
    RGB888,
};

enum class DisplayTechnology : uint8_t {
    Unknown = 0,
    TFT,
    OLED,
    EInk,
};

struct BusCapabilities {
    bool readable = false;
    /** Legacy alias: true only for queued asynchronous pixel transfers. */
    bool dma = false;
    /** Public pixel buffers can be submitted and remain owned until dmaBusy clears. */
    bool asyncPixelTransfer = false;
    /** The transport uses a hardware DMA engine internally. */
    bool hardwareTransfer = false;
    /** A display peripheral continuously scans a framebuffer (for example RGB/DPI). */
    bool continuousScanout = false;
    bool parallel = false;
    bool secondaryChipSelect = false;
    size_t maxTransferBytes = 0;
};

enum class DmaTransferResult : uint8_t {
    Queued = 0,
    SynchronousFallback,
    Unsupported,
    InvalidArgument,
    ClippedOut,
    SubmitFailed,
};

struct DisplayCapabilities {
    DisplayTechnology technology = DisplayTechnology::Unknown;
    PixelFormat nativeFormat = PixelFormat::Unknown;
    bool readback = false;
    bool partialRefresh = false;
    bool fastRefresh = false;
    bool temperatureCompensation = false;
    bool deepSleep = false;
    bool backlight = false;
    uint16_t partialXAlignment = 1;
    uint16_t partialYAlignment = 1;
};

#endif // SEEED_GFX_CAPABILITIES_H
