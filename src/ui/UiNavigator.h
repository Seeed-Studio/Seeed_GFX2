#ifndef SEEED_UI_NAVIGATOR_H
#define SEEED_UI_NAVIGATOR_H

#include <stddef.h>
#include "UiConfig.h"
#include "UiFocusManager.h"
#include "UiScreen.h"

struct UiRouteEntry {
    UiScreen* screen;
    UiId savedFocusId;
    uint16_t savedScrollIndex;
    UiRouteEntry(UiScreen* routeScreen = nullptr,
                 UiId focusId = UI_ID_NONE, uint16_t scrollIndex = 0)
        : screen(routeScreen), savedFocusId(focusId),
          savedScrollIndex(scrollIndex) {}
};

class UiNavigator {
public:
    explicit UiNavigator(UiFocusManager& focus) : _focus(focus) {}
    UiStatus push(UiScreen& screen, const void* params = nullptr);
    UiStatus pop();
    UiStatus replace(UiScreen& screen, const void* params = nullptr);
    UiStatus popToRoot();
    UiScreen* current() const { return _depth ? _stack[_depth - 1].screen : nullptr; }
    size_t depth() const { return _depth; }
    uint32_t generation() const { return _generation; }

private:
    void saveFocus();
    void restoreFocus();

    UiFocusManager& _focus;
    UiRouteEntry _stack[SEEED_UI_NAV_STACK_DEPTH] = {};
    size_t _depth = 0;
    uint32_t _generation = 0;
};

#endif
