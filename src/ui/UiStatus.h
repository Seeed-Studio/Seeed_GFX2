#ifndef SEEED_UI_STATUS_H
#define SEEED_UI_STATUS_H

#include <stdint.h>

enum class UiStatus : uint8_t {
    Ok = 0,
    InvalidArgument,
    NotInitialized,
    CapacityExceeded,
    QueueOverflow,
    Unsupported,
    DataError,
    IoError,
    Busy
};

inline bool uiOk(UiStatus status) { return status == UiStatus::Ok; }

#endif
