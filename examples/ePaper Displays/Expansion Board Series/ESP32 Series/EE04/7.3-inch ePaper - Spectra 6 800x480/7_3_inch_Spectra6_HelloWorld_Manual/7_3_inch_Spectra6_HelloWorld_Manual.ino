/**
 * Product: XIAO ePaper Display Board (ESP32-S3) - EE04
 * Panel: 7.3 inch full-color E Ink Spectra 6, 800x480, ED2208
 * Wiki: https://wiki.seeedstudio.com/epaper_ee04/
 *
 * 本示例演示手动构造 Board / Bus / Driver / Panel 的方式，
 * 效果与 7_3_inch_Spectra6_HelloWorld 一致。
 */

#include <Seeed_GFX.h>
#include "board/boards/XIAO_EPaper_Boards.h"
#include "driver/epaper/Driver_ED2208.h"
#include "panel/Panel_EPaper.h"

// 1) Board：EE04 板卡，内部固定了 CS/DC/RST/ENABLE/BUSY 等引脚
Board_XIAO_EPaper_EE04 board;

// 2) Bus：SPI 总线，引脚顺序与 EE04 板卡一致
//    Bus_SPI(cs, dc, rst, mosi, miso, sclk, freq, cs2 = -1)
Bus_SPI bus(44, 10, 38, D10, -1, D8, 10000000);

// 3) Driver：ED2208 六色 ePaper 驱动
Driver_ED2208 driver(800, 480);

// 4) Panel：ePaper 面板对象
Panel_EPaper panel(driver, bus, &board);

// 5) Seeed_GFX：绑定外部 Panel
Seeed_GFX display(panel);

void setup() {
    Serial.begin(115200);

    if (!display.begin()) {
        Serial.println(display.lastResult().message);
        return;
    }

    // 7.3 inch Spectra 6 是 4bpp 六色面板，必须显式切换到 Colorful 模式。
    // 产品枚举路径会自动完成这步；手动构造时需要自己调用 configure。
    const GfxResult modeResult =
        display.panel().configure(PanelMode::Colorful);
    if (!modeResult) {
        Serial.println(modeResult.message);
        return;
    }

    display.fillScreen(TFT_WHITE);
    display.fillRect(0, 0, display.width(), 82, TFT_BLACK);
    display.setTextColor(TFT_WHITE);
    display.setTextSize(3);
    display.drawString("XIAO ePaper Display Board - EE04", 36, 26);

    const uint16_t colors[] = {TFT_BLACK, TFT_RED, TFT_YELLOW,
                               TFT_GREEN, TFT_BLUE, TFT_WHITE};
    for (uint8_t i = 0; i < 6; ++i) {
        int16_t x = 46 + i * 120;
        display.fillRoundRect(x, 135, 96, 190, 10, colors[i]);
        display.drawRoundRect(x, 135, 96, 190, 10, TFT_BLACK);
    }

    display.setTextColor(TFT_BLACK);
    display.setTextSize(3);
    display.drawString("7.3 inch Spectra 6", 195, 380);
    const GfxResult refreshResult = display.refresh();
    if (!refreshResult) Serial.println(refreshResult.message);
}

void loop() { delay(1000); }
