# Seeed_GFX2 examples

The example tree follows the Seeed product/board/panel matrix:

- `LCD Displays/` — TFT/LCD modules, grouped by **Display Series** (standalone
  SPI modules) or **Expansion Board Series** (boards like the Round Display for
  XIAO).
- `ePaper Displays/` — ePaper panels grouped by **Expansion Board Series**
  (`ESP32 Series` → EE02/EE03/EE04/EE05; `nRF52840 Series` → EN04/EN05) then by
  panel, plus `reTerminal E Series` (E1001–Sticky).
- `Wio Terminal/` — Wio Terminal onboard LCD.
- `Sense CAP watcher/` — SenseCAP Watcher onboard SPD2010 display.
- `LCD Displays/SenseCAP Indicator/` — SenseCAP Indicator GX/DX onboard RGB
  display.
- `Getting_Started/` — entry-point sketches (`Product_Selector` is a generic
  product picker; `Graphics_Basics` runs on the Round Display).
- `Graphics Features/GUI Widgets/` — hardware-neutral Button, Slider, Graph/Trace and
  Analogue Meter examples migrated from the original library. They use the
  native `src/widgets/ClassicWidgets.*` implementation and no longer require
  the external `TFT_eWidget` library.
- `Graphics Features/Smooth_Fonts/` and `Graphics Features/Sprite/` — default to the Wio Terminal
  built-in ILI9341. Upright landscape examples use rotation 3; the portrait
  `Font_Demo_1` intentionally retains the original 240x320 layout.
- `Graphics Features/Smooth_Graphics/` — defaults to the standalone 1.69 inch ST7789
  module and derives drawing bounds from the active display dimensions.
- `Graphics Features/TFT_Screen_Capture/` — targets Wio Terminal because its LCD_SPI
  connection includes MISO. Screen capture requires both controller RAM-read
  support and a physically connected readable bus; write-only LCD modules are
  not interchangeable for this example.

Folders with no sketch yet are placeholders matching the Seeed product matrix;
they will be filled in over time.

## Start here

- `Getting_Started/Product_Selector`: select a registered Seeed product with
  `Seeed_Product::Product`.
- `Getting_Started/Graphics_Basics`: basic drawing primitives on the Seeed
  Studio Round Display for XIAO.

## LCD product examples

| Seeed Wiki product name | Library selection | Example folder |
|---|---|---|
| Seeed Studio Round Display for XIAO | `XIAO_ROUND_DISPLAY` | `LCD Displays/Expansion Board Series/Round Display for XIAO` |
| 1.47 inch LCD SPI Display | `XIAO_LCD_1INCH47` | `LCD Displays/Display Series/1.47 inch LCD SPI Display` |
| XIAO 1.47 inch Touch Display | `XIAO_LCD_1INCH47_TOUCH` (pin template) | `LCD Displays/Display Series/1.47 inch Touch Display` |
| 1.69 inch LCD Display Module | `XIAO_LCD_1INCH69` | `LCD Displays/Display Series/1.69 inch LCD SPI Display` |
| Wio Terminal | `WIO_TERMINAL_PRODUCT` | `Wio Terminal` |
| SenseCAP Watcher | `SENSECAP_WATCHER` | `Sense CAP watcher` |
| SenseCAP Indicator GX | `SENSECAP_INDICATOR_GX` | `LCD Displays/SenseCAP Indicator` |
| SenseCAP Indicator DX | `SENSECAP_INDICATOR_DX` | `LCD Displays/SenseCAP Indicator` |

The Round Display folder also holds feature demos (`Color_Palette`, `Fonts`,
`Sprite`) that run on the same panel. `Touch_UI` is an interactive
XIAO ESP32-S3 example using the built-in CHSCX6X touch controller and the
optional retained-mode UI layer; it needs no third-party touch library.

The Wio Terminal folder contains two UI input styles. `UI_Menu` deliberately
uses only the five-way switch, while `UI_All_Buttons` demonstrates all eight
physical inputs: the five-way switch navigates and confirms, and the top
`A`/`B`/`C` keys provide Back, Quick Menu, and Home shortcuts.

The SenseCAP Watcher folder contains `MultiTouch`, which reads up to ten
SPD2010 touch points through the common `getTouchPoints()` API and prints each
point's controller ID, coordinates and strength. `Knob_Button` demonstrates the
GPIO41/42 rotary wheel and the active-low knob switch on PCA9535 P03. It uses
`SenseCAPWatcherInput` so wheel detents arrive as Scroll events and the push
switch arrives as the Activate action. `UI_Controls` registers both
`TouchInput` and `SenseCAPWatcherInput` in one `UiInputHub`: touch taps an
action directly, wheel rotation moves focus, and pressing the wheel activates
the focused action. `UI_Dashboard` is the complete multi-page example: its
welcome screen opens a four-item main menu, followed by a live system
dashboard, interactive counter, display settings and live device information.
The dashboard updates once per second and keeps all controls inside the useful
center of the Watcher's round 412 x 412 display.

The standalone 1.47 inch and 1.69 inch LCDs are user-wired SPI modules. The
standalone 1.47 sketch uses
`begin<Board_Seeed_1inch47_LCD,
Config_Seeed_1inch47_LCD_ST7789>()`; the 1.69 sketch uses the product enum.
Their reference XIAO wiring is `CS=D1`, `DC=D3`, `RST=D0`, `BL=D6`,
`MOSI=D10`, and `SCLK=D8`.

The XIAO 1.47 inch Touch Display is a separate product: its LCD controller is
JD9853A, touch controller is AXS5106L, and LCD/touch share RST. Its touch
example initializes the LCD first and constructs `Touch_AXS5106L` with
`RST=-1`, preventing touch startup from resetting the initialized LCD.

Compile-time LCD Board aliases are collected in
`board/boards/XIAO_LCD_Board.h`. MCU-only headers such as
`XIAO_ESP32S3.h` no longer contain 1.69 or ILI9341 LCD product aliases.

## ePaper product examples

All small XIAO ePaper panels in this tree use the **EE04** display board
(ESP32-S3). Panels are grouped under
`ePaper Displays/Expansion Board Series/ESP32 Series/EE04/<panel>`.

| Board | Panel demonstrated | Library selection |
|---|---|---|
| EE04 | 2.9 inch monochrome | `XIAO_EPAPER_2INCH9` |
| EE04 | 4.26 inch monochrome | `XIAO_EPAPER_4INCH26` |
| EE04 | 7.5 inch monochrome | `XIAO_EPAPER_7INCH5` |
| EE04 | 7.3 inch E Ink Spectra 6 | `XIAO_EPAPER_7INCH3_C` |
| EE02 | 13.3 inch E Ink Spectra 6 | `XIAO_EPAPER_13INCH3_C` |
| EE03 | 10.3 inch monochrome | `XIAO_EPAPER_10INCH3` |
| reTerminal E1001 | 7.5 inch monochrome | `RETERMINAL_E1001` |
| reTerminal E1002 | 7.3 inch E Ink Spectra 6 | `RETERMINAL_E1002` |
| reTerminal E1003 | 10.3 inch monochrome | `RETERMINAL_E1003` |
| reTerminal E1004 | 13.3 inch E Ink Spectra 6 | `RETERMINAL_E1004` |

Every ePaper sketch renders the whole frame first and calls `refresh()` once,
so initialization, communication, allocation, and BUSY-timeout failures remain
visible to the sketch.
Large panels require an ESP32-S3 build with PSRAM enabled. EE02 and reTerminal
E1004 use T133A01 panels and therefore require the board's second display chip
select; they cannot be replaced with a normal single-CS ePaper adapter.

reTerminal E1001-E1004 display initialization uses the official HSPI host and
places the on-board microSD slot in a deterministic deselected state first.
This follows Seeed's recommended prerequisite for screen-only tests with a card
inserted. An application that also mounts the card must keep SD CS (GPIO14) and
display CS mutually exclusive because both devices share HSPI pins 7/8/9. SD
power enable is GPIO16 on E1001/E1002/E1004 and GPIO39 on E1003.

## Naming and support boundary

The folders intentionally do not advertise generic controller combinations as
Seeed products. For example, a manually wired ILI9341 panel remains a supported
controller/template combination, but it is not presented here as a current
Seeed Wiki display product. Likewise, legacy products that are absent from the
current Wiki matrix are not used as example folder names.

## Official Wiki references

- [Seeed ePaper Displays — Product Matrix Overview](https://wiki.seeedstudio.com/seeed_epaper_displays/)
- [Work with Arduino — ePaper](https://wiki.seeedstudio.com/epaper_work_with_arduino/)
- [ePaper Driver Board](https://wiki.seeedstudio.com/xiao_eink_expansion_board_v2/)
- [XIAO ePaper Display Board EE04](https://wiki.seeedstudio.com/epaper_ee04/)
- [reTerminal E Series](https://wiki.seeedstudio.com/reterminal_e10xx_main_page/)
- [Seeed Studio Round Display for XIAO](https://wiki.seeedstudio.com/get_start_round_display/)
- [1.47 inch LCD SPI Display](https://wiki.seeedstudio.com/1-47inch_lcd_spi_display/)
- [Wio Terminal](https://wiki.seeedstudio.com/Wio-Terminal-Getting-Started/)
