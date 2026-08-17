#ifndef SEEED_UI_OVERLAY_MANAGER_H
#define SEEED_UI_OVERLAY_MANAGER_H

#include "UiFocusManager.h"
#include "UiStatus.h"

class UiWidget;
class IUiInvalidationSink;

class UiOverlayManager {
public:
    UiOverlayManager(UiFocusManager& focus, IUiInvalidationSink& invalidation)
        : _focus(focus), _invalidation(invalidation) {}
    UiStatus showModal(UiWidget& widget);
    UiStatus dismiss();
    UiWidget* modal() const { return _modal; }
    bool active() const { return _modal != nullptr; }

private:
    UiFocusManager& _focus;
    IUiInvalidationSink& _invalidation;
    UiWidget* _modal = nullptr;
    UiWidget* _savedFocus = nullptr;
};

#endif
