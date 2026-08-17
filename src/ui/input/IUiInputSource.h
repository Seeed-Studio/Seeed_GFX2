#ifndef SEEED_UI_INPUT_SOURCE_H
#define SEEED_UI_INPUT_SOURCE_H

#include <stdint.h>
#include "../UiStatus.h"
#include "UiRawEvent.h"

class IUiInputSource {
public:
    virtual ~IUiInputSource() = default;
    virtual UiStatus begin() = 0;
    virtual void scan(uint32_t nowMs, IUiRawEventSink& sink) = 0;
};

#endif
