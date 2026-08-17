#ifndef SEEED_UI_RENDER_SCHEDULER_H
#define SEEED_UI_RENDER_SCHEDULER_H

#include "UiCanvas.h"
#include "UiDirtyRegion.h"
#include "../UiStatus.h"

class Seeed_GFX;
class UiWidget;
struct UiTheme;

struct UiEInkPolicy {
    uint8_t fullRefreshAreaPercent = 60;
    uint8_t fullRefreshAfterPartials = 20;
};

class UiRenderScheduler : public IUiInvalidationSink {
public:
    explicit UiRenderScheduler(Seeed_GFX& gfx);
    void begin();
    void invalidateRect(const UiRect& rect) override { _dirty.add(rect); }
    void invalidateAll() { _dirty.invalidateAll(); }
    bool hasDirty() const { return _dirty.count() != 0; }
    void setEInkPolicy(const UiEInkPolicy& policy) { _einkPolicy = policy; }
    UiStatus render(UiWidget& root, const UiTheme& theme,
                    UiWidget* overlay = nullptr);
    uint32_t frameCount() const { return _frameCount; }

private:
    UiRect alignedPartial(const UiRect& rect, uint16_t ax, uint16_t ay) const;

    Seeed_GFX& _gfx;
    UiCanvas _canvas;
    UiDirtyRegion _dirty;
    UiEInkPolicy _einkPolicy;
    uint8_t _partialRefreshCount = 0;
    uint32_t _frameCount = 0;
};

#endif
