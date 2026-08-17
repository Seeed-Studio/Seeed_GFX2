#ifndef SEEED_UI_EVENT_QUEUE_H
#define SEEED_UI_EVENT_QUEUE_H

#include <stddef.h>
#include "../UiConfig.h"
#include "../UiEvent.h"

template <size_t Capacity = SEEED_UI_EVENT_QUEUE_CAPACITY>
class UiEventQueue {
public:
    static_assert(Capacity > 1, "UI event queue capacity must be greater than one");

    bool push(const UiEvent& event) {
        if (_count == Capacity) {
            if (coalesce(event)) return true;
            ++_overflowCount;
            return false;
        }
        if (_count && coalesce(event)) return true;
        _events[_tail] = event;
        _tail = (_tail + 1U) % Capacity;
        ++_count;
        return true;
    }

    bool pop(UiEvent& event) {
        if (!_count) return false;
        event = _events[_head];
        _head = (_head + 1U) % Capacity;
        --_count;
        return true;
    }

    void clear() { _head = _tail = _count = 0; }
    size_t size() const { return _count; }
    size_t capacity() const { return Capacity; }
    uint32_t overflowCount() const { return _overflowCount; }

private:
    bool coalesce(const UiEvent& event) {
        if (!_count) return false;
        const size_t last = (_tail + Capacity - 1U) % Capacity;
        UiEvent& prior = _events[last];
        if (event.type == UiEventType::PointerMove &&
            prior.type == UiEventType::PointerMove &&
            event.pointerId == prior.pointerId &&
            event.sourceId == prior.sourceId) {
            prior = event;
            return true;
        }
        if (event.type == UiEventType::Scroll && prior.type == UiEventType::Scroll &&
            event.sourceId == prior.sourceId) {
            int32_t sum = static_cast<int32_t>(prior.delta) + event.delta;
            if (sum > 32767) sum = 32767;
            if (sum < -32768) sum = -32768;
            prior.delta = static_cast<int16_t>(sum);
            prior.timestampMs = event.timestampMs;
            return true;
        }
        return false;
    }

    UiEvent _events[Capacity] = {};
    size_t _head = 0;
    size_t _tail = 0;
    size_t _count = 0;
    uint32_t _overflowCount = 0;
};

#endif
