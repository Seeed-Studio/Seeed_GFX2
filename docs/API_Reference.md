# Seeed_GFX 2.1 API Reference

> 2.1 introduces an owned `DisplayInstance`, a separate product catalog,
> structured initialization errors, and capability queries. APIs explicitly
> marked experimental in the source are not part of the stable contract.

## Seeed_GFX Class

The main graphics class. Inherits from Arduino's `Print` class.

### Constructors

| Constructor | Description |
|---|---|
| `Seeed_GFX()` | Create an empty instance (use `setPanel()` or `begin()` later) |
| `Seeed_GFX(IPanel& panel)` | Create with an existing panel |
| `Seeed_GFX(Seeed_Product::Product)` | Store a product selection; hardware is initialized later by `begin()` |

### Initialization

| Method | Description |
|---|---|
| `begin()` | Initialize the current panel |
| `begin<BoardType, PanelConfig>()` | Initialize with predefined board + panel config |
| `begin(Product)` | Initialize with a product enum |
| `setPanel(IPanel&)` | Set the display panel |
| `lastResult()` | Return the most recent structured initialization result |
| `capabilities()` | Return display technology, format and optional capabilities |
| `attachTouch(ITouch&, IBus&)` | Initialize and attach a caller-owned touch controller |

### Drawing

| Method | Description |
|---|---|
| `drawPixel(x, y, color)` | Draw a single pixel |
| `drawLine(x1, y1, x2, y2, color)` | Draw a line |
| `drawFastHLine(x, y, w, color)` | Draw a horizontal line |
| `drawFastVLine(x, y, h, color)` | Draw a vertical line |
| `fillRect(x, y, w, h, color)` | Draw a filled rectangle |
| `fillScreen(color)` | Fill the entire screen |
| `drawRect(x, y, w, h, color)` | Draw a rectangle outline |
| `drawRoundRect(x, y, w, h, r, color)` | Draw a rounded rectangle |
| `fillRoundRect(x, y, w, h, r, color)` | Draw a filled rounded rectangle |
| `drawCircle(x, y, r, color)` | Draw a circle outline |
| `fillCircle(x, y, r, color)` | Draw a filled circle |
| `drawTriangle(x1,y1, x2,y2, x3,y3, color)` | Draw a triangle outline |
| `fillTriangle(x1,y1, x2,y2, x3,y3, color)` | Draw a filled triangle |
| `drawEllipse(x, y, rx, ry, color)` | Draw an ellipse outline |
| `fillEllipse(x, y, rx, ry, color)` | Draw a filled ellipse |

### Text

| Method | Description |
|---|---|
| `drawString(str, x, y)` | Draw a string |
| `drawString(str, x, y, font)` | Draw a string with specified font |
| `drawNumber(n, x, y)` | Draw an integer |
| `drawFloat(f, dec, x, y)` | Draw a float |
| `setCursor(x, y)` | Set text cursor position |
| `setTextColor(color)` | Set text foreground color |
| `setTextColor(fg, bg)` | Set text foreground and background colors |
| `setTextSize(size)` | Set text size multiplier |
| `setTextFont(font)` | Set font number (1-8) |
| `setTextDatum(datum)` | Set text alignment datum |
| `setTextWrap(wrapX, wrapY)` | Set text wrapping |
| `textWidth(str)` | Get text width in pixels |
| `fontHeight()` | Get font height in pixels |

### Image

| Method | Description |
|---|---|
| `pushImage(x, y, w, h, data)` | Push a 16-bit image to the display |
| `pushColor(color)` | Push a single pixel color |
| `pushBlock(color, len)` | Push a block of the same color |
| `setSwapBytes(swap)` | Set byte order swapping for pushImage |
| `drawBitmap(x, y, bitmap, w, h, color)` | Draw a monochrome bitmap |

### Settings

| Method | Description |
|---|---|
| `setRotation(r)` | Set display rotation (0-3) |
| `getRotation()` | Get current rotation |
| `invertDisplay(invert)` | Invert display colors |
| `width()` | Get display width |
| `height()` | Get display height |
| `setViewport(x, y, w, h)` | Set the clipping region |
| `resetViewport()` | Reset viewport to full screen |

### ePaper refresh

| Method | Description |
|---|---|
| `refresh()` | Full refresh and return a structured result |
| `refreshFast()` | Fast full refresh when `capabilities().fastRefresh` is true |
| `refreshPartial(x, y, w, h)` | Partial refresh when supported |
| `update()` / `updateFast()` / `updatePartial(...)` | Compatibility wrappers that discard the result |

`GfxError::BusyTimeout` means the controller did not reach its ready BUSY
level before the bounded timeout. `GfxError::CommunicationFailed` reports a
transport error supplied by the active bus backend.

### Color Utilities

| Method | Description |
|---|---|
| `color565(r, g, b)` | Convert 8-bit RGB to 16-bit RGB565 |
| `color8to16(c)` | Convert 8-bit color to 16-bit |
| `color16to8(c)` | Convert 16-bit color to 8-bit |
| `alphaBlend(alpha, fg, bg)` | Alpha blend two colors |

### Smooth fonts

`Seeed_GFX` and `Seeed_Sprite` support VLW anti-aliased fonts through the
`SmoothFont` renderer. Load a PROGMEM font array with `loadFont(data)`, draw
through `print()`, `drawString()` or `drawGlyph()`, then call `unloadFont()`.
`isFontLoaded()` and `smoothFontLoaded()` query the same loaded state;
`showFont(pageDelay)` previews all glyphs on a `Seeed_GFX` display.

When the selected Arduino platform provides `FS.h`, the overload
`loadFont(path, fs::FS&)` loads a `.vlw` file from an explicit filesystem.

### IT8951 TCON access

IT8951-specific operations remain on `Driver_IT8951`, not on every display.
Use `driverAs<Driver_IT8951>()` before invoking its `tconXXX` methods:

```cpp
if (auto* tcon = gfx.driverAs<Driver_IT8951>()) {
    tcon->setTconTemp(25);
    tcon->tconStandby();
}
```

### Smooth graphics

`drawSmoothArc`, `drawSmoothCircle`, `fillSmoothCircle`,
`drawSmoothRoundRect`, `fillSmoothRoundRect`, `drawSpot`, `drawWideLine`, and
`drawWedgeLine` use software anti-aliasing. They are more CPU-intensive than
basic primitives, particularly on high-resolution displays.

The same smooth primitives are available on `Seeed_Sprite`. They reuse the
main renderer through an internal Panel adapter, so the Sprite remains a
RAM-backed surface rather than inheriting the hardware facade.

### Images

`drawImage()` incrementally decodes built-in BMP and QOI data. Sources may be
`UiMemorySource`, `UiCallbackSource`, or an application-defined
`IUiDataSource`. Pass an application `IUiImageDecoder` to integrate an optional
JPEG, PNG, or GIF library without making it a mandatory dependency.

`drawImageRaw()` supports explicit-width RGB565, Mono1, Indexed4, and Indexed8
assets. BMP supports palette depths 1/4/8, RGB555, RGB565 BITFIELDS, RGB888,
and 32-bit pixels. QOI alpha is blended against the destination through the
active Panel.

### Asynchronous transfers

`submitImageAsync()` and `submitPixelsAsync()` return `DmaTransferResult`.
`Queued` means the source remains owned by the transfer until `dmaWait()`;
`SynchronousFallback` means drawing already finished synchronously.
`busCapabilities()` separates queued pixel transfers, internal hardware DMA,
and continuous RGB framebuffer scanout.

## Optional UI input

Include `Seeed_UI.h` to use the retained-mode UI layer. Physical inputs are
independent sources registered with `UiInputHub`; `UiActionMap` then translates
their physical codes into navigation actions such as `NavigateUp`, `Activate`,
and `Back`.

| Input class | Use case |
|---|---|
| `ButtonInput` | Generic GPIO buttons on any Arduino-compatible board |
| `WioTerminalInput` | Wio Terminal five-way switch and A/B/C preset |
| `TouchInput` / `DirectTouchInput` | Attached or explicitly supplied touch controller |
| `EncoderInput` | Rotary encoder delta and optional push switch |
| `SenseCAPWatcherInput` | Watcher GPIO41/42 wheel plus active-low PCA9535 P03 push switch |

`ButtonInputConfig::add(pin, code, activeLevel, pinModeValue)` adds a key. The
last two arguments default to `LOW` and `INPUT_PULLUP`. Configuration is copied
into `ButtonInput`, and its fixed capacity defaults to eight keys through
`SEEED_UI_MAX_BUTTONS_PER_SOURCE`; no heap allocation is performed.

```cpp
ButtonInputConfig config;
config.add(D0, 1);                         // active-low, internal pull-up
config.add(D1, 2, HIGH, INPUT);            // active-high, external bias
ButtonInput buttons(config);

input.add(buttons);
input.actionMap().add({1, UiAction::NavigateUp, true, false});
input.actionMap().add({2, UiAction::Activate, false, true});
```

Input sources and their configurations must be constructed before
`UiInputHub::begin()`. Multiple sources, such as buttons plus touch, may be
registered in one hub. The current UI implementation requires C++14 or newer
and is excluded on `ARDUINO_ARCH_NRF52`; the graphics API remains available.

For SenseCAP Watcher, construct `SenseCAPWatcherInput` from the product-mode
`Seeed_GFX` object after selecting `SENSECAP_WATCHER`. Rotation produces Scroll
events; `installSenseCAPWatcherDefaultActionMap()` maps the knob push switch to
`UiAction::Activate`. The switch has 25 ms debounce by default and emits one
LongPressed phase after a two-second hold. The lower-level
`Board_SenseCAP_Watcher` methods
`readKnobDelta()` and `readKnobButton()` are also available for sketches that
do not use the retained UI layer.

## Core Interfaces

### IBus (Bus Interface)

```cpp
class IBus {
    virtual bool begin() = 0;
    virtual void end() = 0;
    virtual void beginWrite() = 0;
    virtual void endWrite() = 0;
    virtual void writeCommand(uint8_t cmd) = 0;
    virtual void writeData(uint8_t data) = 0;
    virtual void writeData16(uint16_t data);
    virtual void writePixels(const uint16_t* data, size_t len);
    virtual void writeRepeat(uint16_t pixel, size_t len);
    virtual uint8_t readData() = 0;
    virtual void setFrequency(uint32_t freq) = 0;
    virtual bool supportsDMA() const;
};
```

### IDriver (Driver Interface)

```cpp
class IDriver {
    virtual bool init(IBus& bus) = 0;
    virtual void setRotation(uint8_t rotation) = 0;
    virtual void setAddrWindow(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye) = 0;
    virtual void writePixel(uint16_t color) = 0;
    virtual void writePixels(const uint16_t* data, size_t len) = 0;
    virtual void writeFill(uint16_t color, size_t len) = 0;
    virtual bool supportsAsyncPixelTransfer() const;
    virtual bool enableDMA(bool enable = true);
    virtual bool writePixelsDMA(const uint16_t* data, size_t len);
    virtual bool dmaBusy();
    virtual void sleep() = 0;
    virtual void wake() = 0;
};
```

### IPanel (Panel Interface)

```cpp
class IPanel {
    virtual bool begin() = 0;
    virtual uint16_t width() const = 0;
    virtual uint16_t height() const = 0;
    virtual void setRotation(uint8_t r) = 0;
    virtual void setBacklight(uint8_t brightness) = 0;
    virtual IDriver& driver() = 0;
    virtual DisplayCapabilities capabilities() const = 0;
};
```

## Runtime ownership

`DisplayInstance` owns an official product's Board, Bus, Driver, and Panel.
It destroys them in dependency order: Panel, Driver, Bus, then Board. Product
creation is implemented in `runtime/ProductCatalog`, not in `Seeed_GFX.cpp`.

When `setPanel()` or the `IPanel&` constructor is used, ownership stays with
the caller and every attached object must outlive `Seeed_GFX`.

## Errors and capabilities

`GfxResult` contains a `GfxError` and a diagnostic message. Optional display
operations should be selected using `DisplayCapabilities` rather than assuming
that every panel supports readback, partial refresh, fast refresh, temperature
compensation, deep sleep, or backlight.

### IBoard (Board Interface)

```cpp
class IBoard {
    virtual bool begin() = 0;
    virtual int8_t pinCS() const = 0;
    virtual int8_t pinDC() const = 0;
    virtual int8_t pinRST() const = 0;
    virtual int8_t pinMOSI() const = 0;
    virtual int8_t pinMISO() const = 0;
    virtual int8_t pinSCLK() const = 0;
    virtual int8_t pinBacklight() const = 0;
    virtual IBus* createBus() = 0;
};
```

Most SPI boards are aliases of `ConfiguredSpiBoard<Config>`. Shared behavior
initializes reset, backlight, busy, and enable pins and creates a `Bus_SPI`
using the configured write/read frequencies, SPI mode, and host selection.
Board configuration data is separated from panel configuration data:

```cpp
#include <board/boards/XIAO_ESP32C5.h>

using MyBoard = Board_XIAO_ESP32C5;
```

MCU definitions and LCD product wiring are also separated:

- `board/configs/XIAO_Board_Configs.h` and `board/boards/XIAO_*.h` describe
  XIAO MCU boards.
- `board/configs/XIAO_LCD_Board_Configs.h` and
  `board/boards/XIAO_LCD_Board.h` describe LCD modules and their wiring,
  including 0.96, 1.14, standalone 1.47, 1.47 touch, 1.69 and ILI9341.

```cpp
#include <board/boards/XIAO_LCD_Board.h>

using MyLcdBoard = Board_Seeed_1inch69_LCD;
```

`board/boards/XIAO_Boards.h` is an optional umbrella header. Including an
individual board header is preferred when minimizing compile dependencies.

## Product catalog

```cpp
namespace Seeed_Product {
    enum Product {
        XIAO_ROUND_DISPLAY,    // 1.28" Round, GC9A01
        XIAO_LCD_1INCH47,      // standalone 1.47" LCD, ST7789V3
        XIAO_LCD_1INCH47_TOUCH,// XIAO 1.47" Touch, JD9853A + AXS5106L
        XIAO_ILI9341_240x320,
        XIAO_EPAPER_1INCH54,
        XIAO_EPAPER_2INCH13,
        XIAO_EPAPER_2INCH9,
        XIAO_EPAPER_4INCH2,
        XIAO_EPAPER_4INCH26,
        XIAO_EPAPER_5INCH83,
        XIAO_EPAPER_7INCH5,
        XIAO_EPAPER_10INCH3,
        WIO_TERMINAL,          // 2.4" TFT, ILI9341
        CUSTOM,
    };
}
```

Each official entry has a stable string ID and geometry in
`ProductDescriptor`. `CUSTOM` intentionally has no factory; use template or
external-stack initialization for custom hardware.
