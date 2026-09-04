#include "ProductCatalog.h"
#include "DisplayInstance.h"

#include "../core/Board.h"
#include "../core/Bus.h"
#include "../core/Driver.h"
#include "../core/Panel.h"
#include "../board/Board.h"

// A Wio Terminal or generic ESP32 build exposes only products it can
// construct. The generic ESP32-S3 variant used by SenseCAP intentionally has
// no XIAO D0..D10 aliases.
#if defined(ARDUINO_WIO_TERMINAL)
#define SEEED_GFX_CATALOG_INCLUDE_XIAO 0
#elif defined(ARDUINO_ARCH_ESP32) && \
      !defined(ARDUINO_XIAO_ESP32C3) && \
      !defined(ARDUINO_XIAO_ESP32C5) && \
      !defined(ARDUINO_XIAO_ESP32C6) && \
      !defined(ARDUINO_XIAO_ESP32S2) && \
      !defined(ARDUINO_XIAO_ESP32S3) && \
      !defined(ARDUINO_XIAO_ESP32S3_PLUS)
#define SEEED_GFX_CATALOG_INCLUDE_XIAO 0
#else
#define SEEED_GFX_CATALOG_INCLUDE_XIAO 1
#endif

#if SEEED_GFX_CATALOG_INCLUDE_XIAO
#include "../board/boards/XIAO_ESP32S3.h"
#include "../board/boards/XIAO_LCD_Board.h"
#include "../board/boards/XIAO_ePaper_Boards.h"
#endif
#include "../board/boards/reTerminal_ePaper_Boards.h"
#include "../board/boards/Wio_Terminal.h"
#include "../panel/Panel_TFT.h"
#include "../panel/Panel_EPaper.h"
#include "../driver/tft/Driver_ST7789.h"
#include "../driver/tft/Driver_GC9A01.h"
#include "../driver/tft/Driver_ILI9341.h"
#include "../driver/epaper/Driver_UC8179.h"
#include "../driver/epaper/Driver_UC8151D.h"
#include "../driver/epaper/Driver_JD79686B.h"
#include "../driver/epaper/Driver_SSD1680.h"
#include "../driver/epaper/Driver_SSD1681.h"
#include "../driver/epaper/Driver_SSD1683.h"
#include "../driver/epaper/Driver_SSD1677.h"
#include "../driver/epaper/Driver_SSD2677.h"
#include "../driver/epaper/Driver_Sticky_Auto.h"
#include "../driver/epaper/Driver_ED2208.h"
#include "../driver/epaper/Driver_JD79660.h"
#include "../driver/epaper/Driver_JD79667.h"
#include "../driver/epaper/Driver_JD79676.h"
#include "../driver/epaper/Driver_ED103TC2.h"
#include "../driver/epaper/Driver_T133A01.h"
#include "../driver/epaper/Driver_GDEB0709E01.h"
#include "../touch/Touch_CHSCX6X.h"
#include "../board/boards/SenseCAP_Products.h"
#include "../driver/tft/Driver_SPD2010.h"
#include "../driver/tft/Driver_RGB565.h"
#include "../touch/Touch_SPD2010.h"
#include "../touch/Touch_FT6x36.h"
#include <new>
#include <type_traits>

#if SEEED_GFX_HAS_ESP32S3_LCD
#define SEEED_GFX_SENSECAP_FACTORY(functionName) &functionName
#define SEEED_GFX_SENSECAP_UNSUPPORTED nullptr
#else
#define SEEED_GFX_SENSECAP_FACTORY(functionName) nullptr
#define SEEED_GFX_SENSECAP_UNSUPPORTED \
    "SenseCAP integrated displays require an ESP32-S3 Arduino target with PSRAM"
#endif

namespace {

using ProductFactory = GfxResult (*)(const ProductDescriptor&, DisplayInstance&);

struct ProductEntry {
    ProductDescriptor descriptor;
    ProductFactory factory;
    const char* unsupportedReason;
};

template <typename DriverT, uint8_t RGBOrder>
DriverT* createProductDriver(const ProductDescriptor& product,
                             std::true_type) {
    return new (std::nothrow) DriverT(
        product.driverWidth(), product.driverHeight(), RGBOrder);
}

template <typename DriverT, uint8_t RGBOrder>
DriverT* createProductDriver(const ProductDescriptor& product,
                             std::false_type) {
    return new (std::nothrow) DriverT(
        product.driverWidth(), product.driverHeight());
}

template <typename PanelT>
void applyProductMirror(PanelT* panel, std::true_type) {
    panel->setHorizontalMirror(true);
}

template <typename PanelT>
void applyProductMirror(PanelT*, std::false_type) {}

template <typename BoardT, typename DriverT, typename PanelT, uint8_t RGBOrder = 0xFF, bool Mirror = false>
GfxResult createConfigured(const ProductDescriptor& product,
                           DisplayInstance& instance) {
    BoardT* board = new (std::nothrow) BoardT();
    if (!board) return GfxResult(GfxError::AllocationFailed, "board allocation failed");
    IBus* bus = board->createBus();
    if (!bus) {
        delete board;
        return GfxResult(GfxError::AllocationFailed, "bus allocation failed");
    }
    DriverT* driver = createProductDriver<DriverT, RGBOrder>(
        product,
        std::integral_constant<bool, (RGBOrder != 0xFF)>());
    if (!driver) {
        delete bus;
        delete board;
        return GfxResult(GfxError::AllocationFailed, "driver allocation failed");
    }
    PanelT* panel = new (std::nothrow) PanelT(*driver, *bus, board);
    if (!panel) {
        delete driver;
        delete bus;
        delete board;
        return GfxResult(GfxError::AllocationFailed, "panel allocation failed");
    }
    // Horizontal mirror is a property of the panel glass, not the adapter
    // board, so it is applied here on the panel — letting the same product
    // entry work unchanged across any compatible board.
    applyProductMirror(panel, std::integral_constant<bool, Mirror>());
    if (product.storageWidth != 0 || product.storageHeight != 0) {
        const GfxResult geometry =
            panel->configureVisibleArea(product.width, product.height);
        if (!geometry) {
            delete panel;
            delete driver;
            delete bus;
            delete board;
            return geometry;
        }
    }
    return instance.adopt(board, bus, driver, panel);
}

#if SEEED_GFX_CATALOG_INCLUDE_XIAO
GfxResult createXiaoRoundDisplay(const ProductDescriptor& product,
                                 DisplayInstance& instance) {
    Board_XIAO_ESP32S3* board = new (std::nothrow) Board_XIAO_ESP32S3();
    IBus* bus = board ? board->createBus() : nullptr;
    Driver_GC9A01* driver = bus
        ? new (std::nothrow) Driver_GC9A01(product.width, product.height)
        : nullptr;
    Panel_TFT* panel = driver
        ? new (std::nothrow) Panel_TFT(*driver, *bus, board)
        : nullptr;
    Touch_CHSCX6X* touch = panel
        ? new (std::nothrow) Touch_CHSCX6X(D7, Wire, product.width, product.height)
        : nullptr;
    if (!board || !bus || !driver || !panel || !touch) {
        delete touch;
        delete panel;
        delete driver;
        delete bus;
        delete board;
        return GfxResult(GfxError::AllocationFailed,
                         "XIAO Round Display stack allocation failed");
    }
    return instance.adopt(board, bus, driver, panel, touch);
}
#endif // SEEED_GFX_CATALOG_INCLUDE_XIAO

GfxResult createWioTerminal(const ProductDescriptor&,
                            DisplayInstance& instance) {
    Board_Wio_Terminal* board = new (std::nothrow) Board_Wio_Terminal();
    IBus* bus = board ? board->createBus() : nullptr;
    // ILI9341 RAM is natively 240x320. Wio Terminal's glass is mounted so the
    // landscape view needs the 180-flipped MADCTL (MX|MY|MV); rotation 1 (MV
    // only) renders upside down. Use rotation 3 -> 320x240 landscape, upright.
    Driver_ILI9341* driver = bus
        ? new (std::nothrow) Driver_ILI9341(240, 320)
        : nullptr;
    Panel_TFT* panel = driver
        ? new (std::nothrow) Panel_TFT(*driver, *bus, board, 3)
        : nullptr;
    if (!board || !bus || !driver || !panel) {
        delete panel;
        delete driver;
        delete bus;
        delete board;
        return GfxResult(GfxError::AllocationFailed,
                         "Wio Terminal stack allocation failed");
    }
    return instance.adopt(board, bus, driver, panel);
}

#if SEEED_GFX_HAS_ESP32S3_LCD
GfxResult createSenseCAPWatcher(const ProductDescriptor& product,
                                DisplayInstance& instance) {
    Board_SenseCAP_Watcher* board =
        new (std::nothrow) Board_SenseCAP_Watcher();
    IBus* bus = board ? board->createBus() : nullptr;
    Driver_SPD2010* driver = bus
        ? new (std::nothrow) Driver_SPD2010(product.width, product.height)
        : nullptr;
    Panel_TFT* panel = driver
        ? new (std::nothrow) Panel_TFT(*driver, *bus, board)
        : nullptr;
    Touch_SPD2010* touch = panel
        ? new (std::nothrow) Touch_SPD2010(
              Wire1, 39, 38, product.width, product.height)
        : nullptr;
    if (!board || !bus || !driver || !panel || !touch) {
        delete touch;
        delete panel;
        delete driver;
        delete bus;
        delete board;
        return GfxResult(GfxError::AllocationFailed,
                         "SenseCAP Watcher stack allocation failed");
    }
    return instance.adopt(board, bus, driver, panel, touch);
}

GfxResult createSenseCAPIndicator(const ProductDescriptor& product,
                                  DisplayInstance& instance,
                                  SenseCAPIndicatorPanel variant) {
    Board_SenseCAP_Indicator* board =
        new (std::nothrow) Board_SenseCAP_Indicator(variant);
    IBus* bus = board ? board->createBus() : nullptr;
    Driver_RGB565* driver = bus
        ? new (std::nothrow) Driver_RGB565(
              product.width, product.height,
              variant == SenseCAPIndicatorPanel::GX_ST7701S
                  ? "SenseCAP Indicator GX RGB"
                  : "SenseCAP Indicator DX RGB")
        : nullptr;
    Panel_TFT* panel = driver
        ? new (std::nothrow) Panel_TFT(*driver, *bus, board)
        : nullptr;
    Touch_FT6x36* touch = panel
        ? new (std::nothrow) Touch_FT6x36(
              -1, Wire, board->touchAddress(), product.width, product.height,
              39, 40, 400000, false, true, 2)
        : nullptr;
    if (!board || !bus || !driver || !panel || !touch) {
        delete touch;
        delete panel;
        delete driver;
        delete bus;
        delete board;
        return GfxResult(GfxError::AllocationFailed,
                         "SenseCAP Indicator stack allocation failed");
    }
    return instance.adopt(board, bus, driver, panel, touch);
}

GfxResult createSenseCAPIndicatorGX(const ProductDescriptor& product,
                                    DisplayInstance& instance) {
    return createSenseCAPIndicator(product, instance,
                                   SenseCAPIndicatorPanel::GX_ST7701S);
}

GfxResult createSenseCAPIndicatorDX(const ProductDescriptor& product,
                                    DisplayInstance& instance) {
    return createSenseCAPIndicator(product, instance,
                                   SenseCAPIndicatorPanel::DX_RGB);
}
#endif

struct CustomEPaperPins {
    int8_t cs;
    int8_t cs2;
    int8_t dc;
    int8_t rst;
    int8_t mosi;
    int8_t miso;
    int8_t sclk;
    int8_t busy;
    int8_t enable;
    int8_t auxiliaryEnable;
    bool horizontalMirror;
};

template <typename DriverT>
GfxResult createCustomEPaper(const ProductDescriptor& product,
                             DisplayInstance& instance,
                             const CustomEPaperPins& pins) {
    Board_Custom* board = new (std::nothrow) Board_Custom(
        product.name, pins.cs, pins.dc, pins.rst, pins.mosi, pins.miso,
        pins.sclk, -1, 10000000, pins.busy, pins.enable, 4000000,
        pins.cs2, pins.auxiliaryEnable);
    if (!board) return GfxResult(GfxError::AllocationFailed, "board allocation failed");
    IBus* bus = board->createBus();
    DriverT* driver = bus
        ? new (std::nothrow) DriverT(product.driverWidth(),
                                     product.driverHeight())
        : nullptr;
    Panel_EPaper* panel = driver
        ? new (std::nothrow) Panel_EPaper(*driver, *bus, board)
        : nullptr;
    if (!bus || !driver || !panel) {
        delete panel;
        delete driver;
        delete bus;
        delete board;
        return GfxResult(GfxError::AllocationFailed, "custom ePaper stack allocation failed");
    }
    panel->setHorizontalMirror(pins.horizontalMirror);
    if (product.storageWidth != 0 || product.storageHeight != 0) {
        const GfxResult geometry =
            panel->configureVisibleArea(product.width, product.height);
        if (!geometry) {
            delete panel;
            delete driver;
            delete bus;
            delete board;
            return geometry;
        }
    }
    return instance.adopt(board, bus, driver, panel);
}

#if SEEED_GFX_CATALOG_INCLUDE_XIAO
const CustomEPaperPins kXiaoED103Pins =
    {44, -1, 10, 38, D10, D9, D8, 4, -1, -1, false};

GfxResult createXiaoED103(const ProductDescriptor& product,
                          DisplayInstance& instance) {
    return createCustomEPaper<Driver_ED103TC2>(product, instance,
                                               kXiaoED103Pins);
}
#endif // SEEED_GFX_CATALOG_INCLUDE_XIAO

const ProductEntry kProducts[] = {
#if SEEED_GFX_CATALOG_INCLUDE_XIAO
    // TFT displays
    {
        {Seeed_Product::Seeed_Round_Display_XIAO, "Seeed.round_display.r1",
         "Round Display for Seeed Studio XIAO", 240, 240, 16, ProductPanelMode::Default},
        &createXiaoRoundDisplay, nullptr
    },
    {
        {Seeed_Product::Seeed_LCD_1INCH47, "Seeed.LCD_1inch47.r1",
         "1.47-inch LCD SPI Display", 172, 320, 16, ProductPanelMode::Default},
        &createConfigured<Board_Seeed_1inch47_LCD, Driver_ST7789,
                          Panel_TFT, 0x08>,
        nullptr
    },
    {
        {Seeed_Product::Seeed_LCD_1INCH47_TOUCH, "Seeed.LCD_1inch47_touch.r1",
         "1.47-inch Touch Display", 172, 320, 16, ProductPanelMode::Default},
        nullptr,
        "JD9853A LCD + AXS5106L touch: "
        "begin<Board_XIAO_1inch47_Touch_Display<RST,BL>, "
        "Config_Seeed_1inch47_Touch_JD9853A>(), then attach "
        "Touch_AXS5106L(-1,D7,Wire,172,320). RST=-1 is required after LCD begin "
        "because LCD and touch share reset. Raw RST/BL: nRF <38,37>, "
        "ESP32-S3 <13,12>, SAMD <17,18>."
    },
    {
        {Seeed_Product::Seeed_LCD_0INCH96, "Seeed.LCD_0inch96.r1",
         "0.96-inch Display", 80, 160, 16, ProductPanelMode::Default},
        nullptr,
        "0.96\" LCD Board: begin<Board_XIAO_0inch96_LCD<RST,BL>, "
        "Config_Seeed_0inch96_LCD_ST7789>() (raw GPIO: nRF <38,37>, ESP32-S3 <13,12>, "
        "SAMD <17,18>). No touch; BGR and INVOFF follow the downloaded reference."
    },
    {
        {Seeed_Product::Seeed_LCD_1INCH14, "Seeed.LCD_1inch14.r1",
         "1.14-inch Display", 135, 240, 16, ProductPanelMode::Default},
        nullptr,
        "1.14\" LCD Board: begin<Board_XIAO_1inch14_LCD<RST,BL>, "
        "Config_Seeed_1inch14_LCD_ST7789>() (raw GPIO: nRF <38,37>, ESP32-S3 <13,12>, "
        "SAMD <17,18>). No touch; inversion follows the downloaded reference examples."
    },
    {
        {Seeed_Product::Seeed_LCD_1INCH69, "Seeed.LCD_1inch69.r1",
         "1.69-inch LCD SPI Display", 240, 280, 16, ProductPanelMode::Default},
        &createConfigured<Board_Seeed_1inch69_LCD, Driver_ST7789, Panel_TFT>, nullptr
    },
    {
        {Seeed_Product::Seeed_ILI9341_240x320, "Seeed.ILI9341_240x320.r1",
         "ILI9341 240x320", 240, 320, 16, ProductPanelMode::Default},
        &createConfigured<Board_XIAO_ILI9341, Driver_ILI9341, Panel_TFT>, nullptr
    },

    // Monochrome ePaper displays
    {
        {Seeed_Product::Seeed_ePaper_1INCH54, "Seeed.ePaper_1in54_bw.r1",
         "1.54-inch ePaper Display", 200, 200, 1, ProductPanelMode::Default},
        &createConfigured<Board_XIAO_ePaper_EE04, Driver_SSD1681, Panel_EPaper>, nullptr
    },
    {
        {Seeed_Product::Seeed_ePaper_2INCH13, "Seeed.ePaper_2in13_bw.r1",
         "2.13-inch ePaper Display", 122, 250, 1, ProductPanelMode::Default,
         128, 250, EPaperColorSystem::Monochrome, 2, 2},
        &createConfigured<Board_XIAO_ePaper_EE04, Driver_SSD1680, Panel_EPaper>, nullptr
    },
    {
        {Seeed_Product::Seeed_ePaper_2INCH9, "Seeed.ePaper_2in9_bw.r1",
         "2.9-inch ePaper Display", 128, 296, 1, ProductPanelMode::Default},
        &createConfigured<Board_XIAO_ePaper_EE04, Driver_SSD1680, Panel_EPaper>, nullptr
    },
    {
        {Seeed_Product::Seeed_ePaper_2INCH9_FLEX, "Seeed.ePaper_2in9_flex_bw.r1",
         "2.9-inch Flex ePaper Display", 128, 296, 1, ProductPanelMode::Default},
        &createConfigured<Board_XIAO_ePaper_EE04, Driver_UC8151D, Panel_EPaper>, nullptr
    },
    {
        {Seeed_Product::Seeed_ePaper_4INCH2, "Seeed.ePaper_4in2_bw.r1",
         "4.2-inch ePaper Display", 400, 300, 1, ProductPanelMode::Default},
        &createConfigured<Board_XIAO_ePaper_EE04, Driver_SSD1683, Panel_EPaper>, nullptr
    },
    {
        {Seeed_Product::Seeed_ePaper_4INCH26, "Seeed.ePaper_4in26_bw.r1",
         "4.26-inch ePaper Display", 800, 480, 1, ProductPanelMode::Default},
        &createConfigured<Board_XIAO_ePaper_EE04, Driver_SSD1677, Panel_EPaper, 0xFF, true>, nullptr
    },
    {
        {Seeed_Product::Seeed_ePaper_5INCH83, "Seeed.ePaper_5in83_bw.r1",
         "5.83-inch ePaper Display", 648, 480, 1, ProductPanelMode::Default},
        &createConfigured<Board_XIAO_ePaper_EE04, Driver_UC8179, Panel_EPaper>, nullptr
    },
    {
        {Seeed_Product::Seeed_ePaper_7INCH5, "Seeed.ePaper_7in5_bw.r1",
         "7.5-inch ePaper Display", 800, 480, 1, ProductPanelMode::Default},
        &createConfigured<Board_XIAO_ePaper_EE04, Driver_UC8179, Panel_EPaper>, nullptr
    },
    {
        {Seeed_Product::Seeed_ePaper_7INCH5_JD79686B,
         "Seeed.ePaper_7in5_bw.jd79686b.r1",
         "7.5-inch ePaper Display (JD79686B)", 800, 480, 1,
         ProductPanelMode::Default},
        &createConfigured<Board_XIAO_ePaper_EE04, Driver_JD79686B, Panel_EPaper>, nullptr
    },
    {
        {Seeed_Product::Seeed_ePaper_10INCH3, "Seeed.ePaper_10in3_bw.r1",
         "10.3-inch ePaper Display", 1872, 1404, 1, ProductPanelMode::Default},
        &createXiaoED103, nullptr
    },

    // Six-color ePaper displays
    {
        {Seeed_Product::Seeed_ePaper_4INCH0_C, "Seeed.ePaper_4in0_color.r1",
         "4.0-inch ePaper Display (Spectra 6)", 400, 600, 4, ProductPanelMode::Colorful},
        &createConfigured<Board_XIAO_ePaper_EE04, Driver_ED2208, Panel_EPaper>, nullptr
    },
    {
        {Seeed_Product::Seeed_ePaper_7INCH3_C, "Seeed.ePaper_7in3_color.r1",
         "7.3-inch ePaper Display (Spectra 6)", 800, 480, 4, ProductPanelMode::Colorful},
        &createConfigured<Board_XIAO_ePaper_EE04, Driver_ED2208, Panel_EPaper>, nullptr
    },
    {
        {Seeed_Product::Seeed_ePaper_13INCH3_C, "Seeed.ePaper_13in3_color.r1",
         "13.3-inch ePaper Display (Spectra 6)", 1200, 1600, 4, ProductPanelMode::Colorful},
        &createConfigured<Board_XIAO_ePaper_EE02, Driver_T133A01, Panel_EPaper>, nullptr
    },
    {
        // Good Display GDEB0709E01 (7.09" Spectra 6, NT61522+EK73601) reuses
        // the EE02 dual-chip-select wiring; the driver keeps the T133A01
        // register set byte for byte (vendor divergent values documented in
        // the driver source as fallback).
        {Seeed_Product::Seeed_ePaper_7INCH09_C, "Seeed.ePaper_7in09_color.r1",
         "7.09-inch ePaper Display (Spectra 6)", 1200, 1600, 4, ProductPanelMode::Colorful,
         0, 0, EPaperColorSystem::Spectra6, 6, 0},
        &createConfigured<Board_XIAO_ePaper_EE02, Driver_GDEB0709E01, Panel_EPaper>, nullptr
    },

    // BWRY ePaper displays
    {
        {Seeed_Product::Seeed_ePaper_1INCH54_BWRY, "Seeed.ePaper_1in54_bwry.r1",
         "1.54-inch ePaper Display (BWRY)", 200, 200, 4, ProductPanelMode::BWRY},
        &createConfigured<Board_XIAO_ePaper_EE04, Driver_JD79660, Panel_EPaper>, nullptr
    },
    {
        {Seeed_Product::Seeed_ePaper_2INCH13_BWRY, "Seeed.ePaper_2in13_bwry.r1",
         "2.13-inch ePaper Display (BWRY)", 122, 250, 4, ProductPanelMode::BWRY,
         128, 250, EPaperColorSystem::BWRY, 4, 0},
        &createConfigured<Board_XIAO_ePaper_EE04, Driver_JD79676, Panel_EPaper>, nullptr
    },
    {
        {Seeed_Product::Seeed_ePaper_2INCH9_BWRY, "Seeed.ePaper_2in9_bwry.r1",
         "2.9-inch ePaper Display (BWRY)", 128, 296, 4, ProductPanelMode::BWRY},
        &createConfigured<Board_XIAO_ePaper_EE04, Driver_JD79667, Panel_EPaper>, nullptr
    },
#endif // SEEED_GFX_CATALOG_INCLUDE_XIAO
    // reTerminal ePaper products
    {
        {Seeed_Product::reTerminal_E1001, "seeed.reterminal.e1001.r1",
         "reTerminal E1001", 800, 480, 1, ProductPanelMode::Default,
         0, 0, EPaperColorSystem::MonochromeGray, 0, 4},
        &createConfigured<Board_reTerminal_E1001, Driver_UC8179, Panel_EPaper>, nullptr
    },
    {
        {Seeed_Product::reTerminal_E1002, "seeed.reterminal.e1002.r1",
         "reTerminal E1002", 800, 480, 4, ProductPanelMode::Colorful,
         0, 0, EPaperColorSystem::Spectra6, 6, 0},
        &createConfigured<Board_reTerminal_E1002, Driver_ED2208, Panel_EPaper>, nullptr
    },
    {
        {Seeed_Product::reTerminal_E1003, "seeed.reterminal.e1003.r1",
         "reTerminal E1003", 1872, 1404, 1, ProductPanelMode::Default,
         0, 0, EPaperColorSystem::MonochromeGray, 0, 16, 1404, 1872},
        &createConfigured<Board_reTerminal_E1003, Driver_ED103TC2, Panel_EPaper>, nullptr
    },
    {
        {Seeed_Product::reTerminal_E1004, "seeed.reterminal.e1004.r1",
         "reTerminal E1004", 1200, 1600, 4, ProductPanelMode::Colorful,
         0, 0, EPaperColorSystem::Spectra6, 6, 0},
        &createConfigured<Board_reTerminal_E1004, Driver_T133A01, Panel_EPaper>, nullptr
    },
    {
        // Production mixes SSD1677 and SSD2677 units randomly; the driver
        // resolves the fitted controller at begin() time with the firmware
        // probe (reset -> 0x70 -> read -> 0x07 = SSD2677).
        {Seeed_Product::reTerminal_Sticky, "seeed.reterminal.sticky.r1",
         "reTerminal Sticky", 800, 480, 1, ProductPanelMode::Default},
        &createConfigured<Board_reTerminal_Sticky, Driver_Sticky_Auto, Panel_EPaper>, nullptr
    },
    // Wio Terminal
    {
        {Seeed_Product::Wio_Terminal, "seeed.wio_terminal.r1", "Wio Terminal",
         320, 240, 16, ProductPanelMode::Default},
        &createWioTerminal, nullptr
    },

    // Integrated ESP32-S3 products. These use native QSPI/RGB LCD
    // peripherals and are intentionally unavailable on other architectures.
    {
        {Seeed_Product::SenseCAP_Watcher, "seeed.sensecap.watcher.spd2010.r1",
         "SenseCAP Watcher", 412, 412, 16, ProductPanelMode::Default},
        SEEED_GFX_SENSECAP_FACTORY(createSenseCAPWatcher),
        SEEED_GFX_SENSECAP_UNSUPPORTED
    },
    {
        {Seeed_Product::SenseCAP_Indicator_GX,
         "seeed.sensecap.indicator.gx.st7701s.r1",
         "SenseCAP Indicator GX", 480, 480, 16, ProductPanelMode::Default},
        SEEED_GFX_SENSECAP_FACTORY(createSenseCAPIndicatorGX),
        SEEED_GFX_SENSECAP_UNSUPPORTED
    },
    {
        {Seeed_Product::SenseCAP_Indicator_DX,
         "seeed.sensecap.indicator.dx.rgb.r1",
         "SenseCAP Indicator DX", 480, 480, 16, ProductPanelMode::Default},
        SEEED_GFX_SENSECAP_FACTORY(createSenseCAPIndicatorDX),
        SEEED_GFX_SENSECAP_UNSUPPORTED
    },
};

const ProductEntry* findEntry(Seeed_Product::Product product) {
    for (size_t i = 0; i < sizeof(kProducts) / sizeof(kProducts[0]); ++i) {
        if (kProducts[i].descriptor.id == product) return &kProducts[i];
    }
    return nullptr;
}

} // namespace

const ProductDescriptor* ProductCatalog::find(Seeed_Product::Product product) {
    const ProductEntry* entry = findEntry(product);
    return entry ? &entry->descriptor : nullptr;
}

size_t ProductCatalog::count() {
    return sizeof(kProducts) / sizeof(kProducts[0]);
}

GfxResult ProductCatalog::create(Seeed_Product::Product product,
                                 DisplayInstance& instance) {
    const ProductEntry* entry = findEntry(product);
    if (!entry) {
        return GfxResult(GfxError::ProductNotFound, "unknown or custom product");
    }
    if (!entry->factory) {
        return GfxResult(GfxError::NotSupported, entry->unsupportedReason);
    }
    return entry->factory(entry->descriptor, instance);
}
