#ifndef SEEED_UI_EVENT_H
#define SEEED_UI_EVENT_H

#include <stdint.h>

enum class UiEventType : uint8_t {
    Action = 0,
    PointerDown,
    PointerMove,
    PointerUp,
    PointerCancel,
    Scroll,
    FocusGained,
    FocusLost,
    Tick
};

enum class UiAction : uint8_t {
    None = 0,
    NavigateUp,
    NavigateDown,
    NavigateLeft,
    NavigateRight,
    Activate,
    Back,
    Home,
    Menu,
    PagePrevious,
    PageNext,
    Increment,
    Decrement
};

enum class UiActionPhase : uint8_t {
    Pressed = 0,
    Released,
    LongPressed,
    Repeat
};

struct UiEvent {
    UiEventType type = UiEventType::Tick;
    UiAction action = UiAction::None;
    UiActionPhase phase = UiActionPhase::Pressed;
    int16_t x = 0;
    int16_t y = 0;
    int16_t delta = 0;
    uint8_t pointerId = 0;
    uint8_t sourceId = 0;
    uint32_t timestampMs = 0;
    bool handled = false;
};

#endif
