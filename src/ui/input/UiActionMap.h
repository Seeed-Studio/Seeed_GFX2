#ifndef SEEED_UI_ACTION_MAP_H
#define SEEED_UI_ACTION_MAP_H

#include <stddef.h>
#include "../UiConfig.h"
#include "../UiEvent.h"
#include "../UiStatus.h"

struct UiActionBinding {
    uint16_t physicalCode;
    UiAction action;
    bool allowRepeat;
    bool allowLongPress;
    constexpr UiActionBinding(uint16_t code = 0,
                              UiAction mappedAction = UiAction::None,
                              bool repeat = false, bool longPress = false)
        : physicalCode(code), action(mappedAction), allowRepeat(repeat),
          allowLongPress(longPress) {}
};

class UiActionMap {
public:
    UiStatus add(const UiActionBinding& binding) {
        for (size_t i = 0; i < _count; ++i) {
            if (_bindings[i].physicalCode == binding.physicalCode) {
                _bindings[i] = binding;
                return UiStatus::Ok;
            }
        }
        if (_count >= SEEED_UI_MAX_ACTION_BINDINGS)
            return UiStatus::CapacityExceeded;
        _bindings[_count++] = binding;
        return UiStatus::Ok;
    }

    const UiActionBinding* find(uint16_t physicalCode) const {
        for (size_t i = 0; i < _count; ++i)
            if (_bindings[i].physicalCode == physicalCode) return &_bindings[i];
        return nullptr;
    }

    size_t size() const { return _count; }

private:
    UiActionBinding _bindings[SEEED_UI_MAX_ACTION_BINDINGS] = {};
    size_t _count = 0;
};

#endif
