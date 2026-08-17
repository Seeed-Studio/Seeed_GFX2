#ifndef SEEED_GFX_RESULT_H
#define SEEED_GFX_RESULT_H

#include <stdint.h>

enum class GfxError : uint8_t {
    Ok = 0,
    InvalidArgument,
    ProductNotFound,
    AllocationFailed,
    BoardInitFailed,
    BusInitFailed,
    DriverInitFailed,
    PanelInitFailed,
    TouchInitFailed,
    NotInitialized,
    NotSupported,
    BusyTimeout,
    CommunicationFailed,
};

struct GfxResult {
    GfxError error;
    const char* message;

    constexpr GfxResult(GfxError value = GfxError::Ok, const char* detail = "ok")
        : error(value), message(detail) {}

    constexpr bool ok() const { return error == GfxError::Ok; }
    constexpr explicit operator bool() const { return ok(); }

    static constexpr GfxResult success() { return GfxResult(); }
};

#endif // SEEED_GFX_RESULT_H
