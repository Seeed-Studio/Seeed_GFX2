# GUI Widgets

These examples are migrated from the original Seeed_GFX `GUI Widgets`
folder. They use the native widgets in `src/widgets/ClassicWidgets.*`; the
external `TFT_eWidget` library and the copied `Free_Fonts.h` files are not
required.

The sketches normally detect the product automatically:

```cpp
Seeed_Product::detectIntegratedDisplayProduct()
```

- Wio Terminal uses its Arduino board macro.
- SenseCAP Watcher is identified by its `0x21` control expander.
- SenseCAP Indicator is identified by its `0x20`/`0x39` expander; touch
  address `0x48` selects GX and `0x38` selects DX.

On ESP32-S3 the detector explicitly stops and rebinds `Wire` when moving from
the Watcher GPIO 47/48 probe to the Indicator GPIO 39/40 probe. ESP32 Arduino
does not change pins when `Wire.begin()` is called again on an active bus, so
this restart is required for reliable automatic Indicator detection.

No product selection is normally required. Detection stops with a Serial
error instead of initializing the wrong panel when the result is ambiguous.
For diagnostics or a custom board package, an explicit override can still be
placed below the `#include <Seeed_GFX.h>` line:

```cpp
#define SEEED_GUI_WIDGET_PRODUCT Seeed_Product::SENSECAP_WATCHER
```

Supported values are:

- `Seeed_Product::SENSECAP_INDICATOR_GX` or `_DX`: touch-enabled 480x480 demo.
- `Seeed_Product::SENSECAP_WATCHER`: touch-enabled 412x412 demo.
- `Seeed_Product::WIO_TERMINAL_PRODUCT`: graphs and meters run normally;
  buttons and sliders enter an automatic demonstration because Wio Terminal
  has no touch panel.

Buttons and sliders call the common `display.getTouch()` API. Product-specific
touch rotation is supplied by the product catalog, so the sketches must not
add their own coordinate inversion.

The immediate-mode widgets here are intended for direct drawing sketches. For
multi-page applications, focus navigation, dialogs and dirty-region rendering,
use the retained-mode layer from `<Seeed_UI.h>` instead.
