#if !defined(ARDUINO_ARCH_NRF52)
/* UI layer skipped on nRF52 (Seeed core uses -std=gnu++11; UI needs C++14/17
   aggregate features). Core graphics + ePaper do not depend on the UI. */
#include "UiCanvas.h"
#include "../../Seeed_GFX.h"

bool UiCanvas::save() {
    if (_depth >= SEEED_UI_CANVAS_STACK_DEPTH) return false;
    State& state = _states[_depth++];
    state.viewportX = _gfx.getViewportX();
    state.viewportY = _gfx.getViewportY();
    state.viewportW = _gfx.getViewportWidth();
    state.viewportH = _gfx.getViewportHeight();
    state.viewportDatum = _gfx.getViewportDatum();
    state.originX = _gfx.getOriginX(); state.originY = _gfx.getOriginY();
    state.textColor = _gfx.textcolor; state.textBackground = _gfx.textbgcolor;
    state.textSize = _gfx.textsize; state.textFont = _gfx.textfont;
    state.textDatum = _gfx.textdatum;
    return true;
}

bool UiCanvas::restore() {
    if (!_depth) return false;
    const State& state = _states[--_depth];
    _gfx.setViewport(state.viewportX, state.viewportY,
                     state.viewportW, state.viewportH, state.viewportDatum);
    _gfx.setOrigin(state.originX, state.originY);
    _gfx.setTextColor(static_cast<uint16_t>(state.textColor),
                      static_cast<uint16_t>(state.textBackground));
    _gfx.setTextSize(state.textSize); _gfx.setTextFont(state.textFont);
    _gfx.setTextDatum(state.textDatum);
    return true;
}

void UiCanvas::clipRect(const UiRect& rect) {
    UiRect current;
    current.x = uiClamp16(_gfx.getViewportX());
    current.y = uiClamp16(_gfx.getViewportY());
    current.w = uiClamp16(_gfx.getViewportWidth());
    current.h = uiClamp16(_gfx.getViewportHeight());
    const UiRect clipped = current.intersection(rect);
    if (clipped.empty()) _gfx.setViewport(0, 0, 0, 0, false);
    else _gfx.setViewport(clipped.x, clipped.y, clipped.w, clipped.h, false);
}

void UiCanvas::resetClip() { _gfx.resetViewport(); }

void UiCanvas::fillRect(const UiRect& rect, uint16_t color) {
    _gfx.fillRect(rect.x, rect.y, rect.w, rect.h, color);
}

void UiCanvas::drawRect(const UiRect& rect, uint16_t color, int16_t thickness) {
    for (int16_t i = 0; i < thickness && rect.w > 2 * i && rect.h > 2 * i; ++i)
        _gfx.drawRect(rect.x + i, rect.y + i, rect.w - 2 * i, rect.h - 2 * i, color);
}

void UiCanvas::drawText(int16_t x, int16_t y, const char* text, uint16_t color,
                        uint8_t size) {
    if (!text) return;
    _gfx.setTextDatum(TL_DATUM);
    _gfx.setTextSize(size ? size : 1);
    _gfx.setTextColor(color);
    _gfx.drawString(String(text), x, y);
}

void UiCanvas::drawImage565(const UiRect& destination, const uint16_t* pixels) {
    if (!pixels || destination.empty()) return;
    _gfx.pushImage(destination.x, destination.y, destination.w, destination.h, pixels);
}

#endif // !ARDUINO_ARCH_NRF52 (UI layer)
