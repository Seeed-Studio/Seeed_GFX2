#if !defined(ARDUINO_ARCH_NRF52)
/* UI layer skipped on nRF52 (Seeed core uses -std=gnu++11; UI needs C++14/17
   aggregate features). Core graphics + ePaper do not depend on the UI. */
#include "UiRenderScheduler.h"
#include "../../Seeed_GFX.h"
#include "../theme/UiTheme.h"
#include "../widget/UiWidget.h"

UiRenderScheduler::UiRenderScheduler(Seeed_GFX& gfx)
    : _gfx(gfx), _canvas(gfx), _dirty() {}

void UiRenderScheduler::begin() {
    UiRect screen;
    screen.w = _gfx.width(); screen.h = _gfx.height();
    _dirty.setScreen(screen);
    _dirty.invalidateAll();
}

UiRect UiRenderScheduler::alignedPartial(const UiRect& rect, uint16_t ax,
                                         uint16_t ay) const {
    if (!ax) ax = 1;
    if (!ay) ay = 1;
    int32_t x0 = (rect.x / ax) * ax;
    int32_t y0 = (rect.y / ay) * ay;
    int32_t x1 = ((rect.right() + ax - 1) / ax) * ax;
    int32_t y1 = ((rect.bottom() + ay - 1) / ay) * ay;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > _gfx.width()) x1 = _gfx.width();
    if (y1 > _gfx.height()) y1 = _gfx.height();
    UiRect result;
    result.x = uiClamp16(x0); result.y = uiClamp16(y0);
    result.w = uiClamp16(x1 - x0); result.h = uiClamp16(y1 - y0);
    return result;
}

UiStatus UiRenderScheduler::render(UiWidget& root, const UiTheme& theme,
                                   UiWidget* overlay) {
    if (!_gfx.hasPanel()) return UiStatus::NotInitialized;
    if (!_dirty.count()) return UiStatus::Ok;
    const DisplayCapabilities caps = _gfx.capabilities();

    UiRect submitted[SEEED_UI_MAX_DIRTY_RECTS] = {};
    const size_t submittedCount = _dirty.count();
    // Keep every dirty rectangle in one display transaction. Framebuffer
    // transports can therefore publish the complete UI update atomically.
    _gfx.startWrite();
    for (size_t i = 0; i < submittedCount; ++i) {
        submitted[i] = _dirty.at(i);
        _gfx.setViewport(submitted[i].x, submitted[i].y,
                         submitted[i].w, submitted[i].h, false);
        root.render(_canvas, theme);
        if (overlay && overlay->visible()) overlay->render(_canvas, theme);
    }
    _gfx.resetViewport();
    _gfx.endWrite();

    if (caps.technology == DisplayTechnology::EInk) {
        if (caps.partialRefresh) {
            UiRect combined;
            for (size_t i = 0; i < submittedCount; ++i)
                combined = combined.united(submitted[i]);
            combined = alignedPartial(combined, caps.partialXAlignment,
                                      caps.partialYAlignment);
            const uint32_t screenArea = static_cast<uint32_t>(_gfx.width()) * _gfx.height();
            const uint32_t dirtyArea = static_cast<uint32_t>(combined.w) * combined.h;
            const bool areaRequestsFull = screenArea &&
                dirtyArea * 100U >= screenArea * _einkPolicy.fullRefreshAreaPercent;
            const bool countRequestsFull = _einkPolicy.fullRefreshAfterPartials &&
                _partialRefreshCount >= _einkPolicy.fullRefreshAfterPartials;
            GfxResult result;
            if (areaRequestsFull || countRequestsFull) {
                result = _gfx.refresh();
                if (result.ok()) _partialRefreshCount = 0;
            } else if (!combined.empty()) {
                result = _gfx.refreshPartial(combined.x, combined.y,
                                             combined.w, combined.h);
                if (result.ok() && _partialRefreshCount < 255) ++_partialRefreshCount;
            }
            if (!result.ok()) return result.error == GfxError::BusyTimeout
                ? UiStatus::Busy : UiStatus::IoError;
        } else {
            const GfxResult result = _gfx.refresh();
            if (!result.ok()) return result.error == GfxError::BusyTimeout
                ? UiStatus::Busy : UiStatus::IoError;
        }
    }
    _dirty.clear();
    ++_frameCount;
    return UiStatus::Ok;
}

#endif // !ARDUINO_ARCH_NRF52 (UI layer)
