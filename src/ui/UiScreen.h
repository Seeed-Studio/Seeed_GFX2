#ifndef SEEED_UI_SCREEN_H
#define SEEED_UI_SCREEN_H

#include <stdint.h>
#include "UiEvent.h"
#include "UiStatus.h"

class UiWidget;

class UiScreen {
public:
    virtual ~UiScreen() = default;
    virtual UiStatus onCreate() { return UiStatus::Ok; }
    virtual void onEnter(const void*) {}
    virtual void onPause() {}
    virtual void onResume() {}
    virtual void onExit() {}
    virtual void update(uint32_t) {}
    virtual bool onEvent(UiEvent&) { return false; }
    virtual UiWidget& root() = 0;

    UiStatus ensureCreated() {
        if (_created) return UiStatus::Ok;
        const UiStatus status = onCreate();
        if (uiOk(status)) _created = true;
        return status;
    }

private:
    bool _created = false;
};

#endif
