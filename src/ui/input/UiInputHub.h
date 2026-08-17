#ifndef SEEED_UI_INPUT_HUB_H
#define SEEED_UI_INPUT_HUB_H

#include <stddef.h>
#include "../UiConfig.h"
#include "../UiStatus.h"
#include "IUiInputSource.h"
#include "UiActionMap.h"
#include "UiEventQueue.h"

enum class UiInputMode : uint8_t { Keys = 0, Touch, Encoder };

class UiInputHub : public IUiRawEventSink {
public:
    UiStatus add(IUiInputSource& source);
    UiStatus begin();
    void scan(uint32_t nowMs);
    bool poll(UiEvent& event) { return _queue.pop(event); }
    bool pushRaw(const UiRawEvent& event) override;

    UiActionMap& actionMap() { return _actionMap; }
    const UiActionMap& actionMap() const { return _actionMap; }
    UiInputMode mode() const { return _mode; }
    uint32_t overflowCount() const { return _queue.overflowCount(); }

private:
    IUiInputSource* _sources[SEEED_UI_MAX_INPUT_SOURCES] = {};
    size_t _sourceCount = 0;
    UiActionMap _actionMap;
    UiEventQueue<> _queue;
    UiInputMode _mode = UiInputMode::Keys;
};

#endif
