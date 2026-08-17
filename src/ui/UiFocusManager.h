#ifndef SEEED_UI_FOCUS_MANAGER_H
#define SEEED_UI_FOCUS_MANAGER_H

#include "UiTypes.h"

class UiWidget;

class UiFocusManager {
public:
    UiWidget* focused() const { return _focused; }
    bool setFocus(UiWidget* widget);
    void clearFocus() { setFocus(nullptr); }
    bool focusFirst(UiWidget& root);
    bool moveNext(UiWidget& root, bool wrap = false);
    bool movePrevious(UiWidget& root, bool wrap = false);
    UiWidget* findById(UiWidget& root, UiId id) const;

private:
    UiWidget* _focused = nullptr;
};

#endif
