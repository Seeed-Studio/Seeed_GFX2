#ifndef SEEED_UI_APPLICATION_H
#define SEEED_UI_APPLICATION_H

#include "UiFocusManager.h"
#include "UiNavigator.h"
#include "UiOverlayManager.h"
#include "input/UiInputHub.h"
#include "render/UiRenderScheduler.h"
#include "theme/UiTheme.h"

class Seeed_GFX;

struct UiDiagnostics {
    uint32_t ticks = 0;
    uint32_t frames = 0;
    uint32_t inputOverflows = 0;
    UiStatus lastStatus = UiStatus::Ok;
};

class UiApplication {
public:
    UiApplication(Seeed_GFX& gfx, UiInputHub& input, UiTheme& theme);
    UiStatus begin(UiScreen& root);
    UiStatus tick(uint32_t nowMs);

    UiNavigator& navigator() { return _navigator; }
    UiFocusManager& focus() { return _focus; }
    UiOverlayManager& overlays() { return _overlays; }
    UiRenderScheduler& renderer() { return _renderer; }
    UiDiagnostics diagnostics() const { return _diagnostics; }

private:
    void bindCurrentScreen();
    bool dispatch(UiEvent& event);
    bool bubble(UiWidget* target, UiEvent& event);
    UiWidget* focusableAncestor(UiWidget* widget) const;
    UiWidget* dragAncestor(UiWidget* widget) const;

    Seeed_GFX& _gfx;
    UiInputHub& _input;
    UiTheme& _theme;
    UiFocusManager _focus;
    UiNavigator _navigator;
    UiRenderScheduler _renderer;
    UiOverlayManager _overlays;
    UiWidget* _pointerCapture = nullptr;
    UiWidget* _dragCandidate = nullptr;
    UiPoint _pointerDown;
    UiScreen* _boundScreen = nullptr;
    uint32_t _boundGeneration = 0;
    UiDiagnostics _diagnostics;
};

#endif
