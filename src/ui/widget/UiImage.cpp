#if !defined(ARDUINO_ARCH_NRF52)
/* UI layer skipped on nRF52 (Seeed core uses -std=gnu++11; UI needs C++14/17
   aggregate features). Core graphics + ePaper do not depend on the UI. */
#include "UiImage.h"
#include "../render/UiCanvas.h"
#include "../theme/UiTheme.h"

void UiImage::render(UiCanvas& canvas, const UiTheme& theme) {
    if (!visible()) return;
    if (!_pixels || !_width || !_height) {
        canvas.fillRect(bounds(), theme.colors.surface);
        canvas.drawRect(bounds(), theme.colors.error, 1);
        return;
    }
    UiRect imageRect = {bounds().x, bounds().y,
                        static_cast<int16_t>(_width), static_cast<int16_t>(_height)};
    // Direct drawing is intentionally 1:1. The active viewport clips the original
    // row stride correctly; decoders and future scalers feed RGB565 here.
    canvas.drawImage565(imageRect, _pixels);
}

#endif // !ARDUINO_ARCH_NRF52 (UI layer)
