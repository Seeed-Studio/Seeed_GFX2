/**
 * @file   Seeed_Panel_Configs.h
 * @brief  Seeed Studio 全产品面板配置表
 *
 * 本文件按 Seeed Studio 产品线组织面板配置。每个配置结构体对应一款
 * 实际在售产品，包含分辨率、驱动 IC 类型和面板类型。
 *
 * 使用方式:
 *   display.begin<Board_XIAO_ESP32S3, Config_XIAO_Expansion_1inch28_GC9A01>();
 *
 * 产品线:
 *   1. XIAO 扩展板系列 (TFT LCD)
 *   2. XIAO ePaper 扩展板系列 (单色)
 *   3. XIAO ePaper 扩展板系列 (6色彩色)
 *   4. XIAO ePaper 扩展板系列 (BWRY 四色)
 *   5. Wio Terminal 系列
 *   6. reTerminal 系列
 *
 * 产品页面: https://www.seeedstudio.com
 * Wiki:      https://wiki.seeedstudio.com
 */

#ifndef SEEED_GFX_SEEED_PANEL_CONFIGS_H
#define SEEED_GFX_SEEED_PANEL_CONFIGS_H

#include <stdint.h>
#include "../../core/Panel.h"

// Forward declarations
class IDriver;
class IPanel;
class IBus;
class IBoard;

// TFT driver forward declarations
class Driver_ILI9341;
class Driver_ST7789;
class Driver_JD9853A;
class Driver_GC9A01;
class Driver_ILI9488;
class Driver_ST7735;
class Driver_SSD1351;

// ePaper driver forward declarations
class Driver_UC8179;
class Driver_UC8151D;
class Driver_IT8951;
class Driver_SSD1680;
class Driver_SSD1681;
class Driver_SSD1683;
class Driver_SSD1677;
class Driver_SSD2677;
class Driver_JD79686B;
class Driver_JD79667;
class Driver_JD79676;
class Driver_JD79660;
class Driver_ED2208;
class Driver_ED103TC2;
class Driver_T133A01;

// Panel forward declarations
class Panel_EPaper;
class Panel_TFT;
class Panel_OLED;

// 第一部分: XIAO 扩展板系列 - TFT LCD 显示屏
// 接口: XIAO 排针直插 (D0-D10)
// 板卡: Board_XIAO_ESP32S3 / Board_XIAO_RP2040 / Board_XIAO_nRF52840 等

// --- XIAO Round Display (1.28" 圆形) ---
// 产品: XIAO Round Display (GC9A01, 240x240 圆形 TFT)
// SKU:  104030008
// 芯片: GC9A01
// 尺寸: 1.28 英寸, 240x240 像素, 16-bit RGB565
// 链接: https://wiki.seeedstudio.com/xiao_round_display/
struct Config_XIAO_Expansion_1inch28_Round_GC9A01 {
    using Driver = Driver_GC9A01;
    using Panel  = Panel_TFT;
    static constexpr uint16_t width      = 240;
    static constexpr uint16_t height     = 240;
    static constexpr uint8_t  colorDepth = 16;
};

// --- 1.47 inch LCD SPI Display (standalone, no touch) ---
// Official Wiki product: 1.47 inch LCD SPI Display
// 芯片: ST7789V3. This is the eight-pin user-wired module, not the
// XIAO 1.47" Touch Display Board.
// 尺寸: 1.47 英寸, 172x320 像素, 16-bit RGB565
struct Config_Seeed_1inch47_LCD_ST7789 {
    using Driver = Driver_ST7789;
    using Panel  = Panel_TFT;
    static constexpr uint16_t width      = 172;
    static constexpr uint16_t height     = 320;
    static constexpr uint8_t  colorDepth = 16;
    static constexpr uint8_t  rgbOrder   = 0x08; // ST7789 BGR (Setup75)
    static constexpr uint8_t  initialRotation = 1; // Official landscape 320x172
};

// --- XIAO 1.47 inch Touch Display Board ---
// LCD controller: JD9853A (ST7789-compatible DCS command subset)
// Touch controller: AXS5106L on I2C address 0x63 (kept in the Touch layer)
// Native LCD orientation is MADCTL 0x48 and normal colors require INVOFF.
struct Config_XIAO_1inch47_Touch_JD9853A {
    using Driver = Driver_JD9853A;
    using Panel  = Panel_TFT;
    static constexpr uint16_t width      = 172;
    static constexpr uint16_t height     = 320;
    static constexpr uint8_t  colorDepth = 16;
    static constexpr uint8_t  rgbOrder   = 0x08;
    static constexpr bool     invert     = false;
};

// --- 0.96 inch LCD (XIAO Display Board, ST7789 80x160) ---
// IPS panel on the XIAO Display Board. Rotation 2, col offset 24 / row 0.
// The downloaded dashboard proves that its RGB-mode path displays red/blue
// swapped, so this config selects BGR. Its final state explicitly requests
// color inversion, which Arduino_GFX maps to INVOFF for an IPS=true panel.
struct Config_Seeed_0inch96_LCD_ST7789 {
    using Driver = Driver_ST7789;
    using Panel  = Panel_TFT;
    static constexpr uint16_t width      = 80;
    static constexpr uint16_t height     = 160;
    static constexpr uint8_t  colorDepth = 16;
    static constexpr uint8_t  rgbOrder   = 0x08; // BGR
    static constexpr uint8_t  initialRotation = 2;
    static constexpr bool     invert     = false;
};

// --- 1.14 inch LCD (XIAO Display Board, ST7789 135x240) ---
// IPS panel on the XIAO Display Board. Rotation 0, offset 52/40 (53/40 when
// inverted), RGB order. Both Arduino_GFX IPS=true initialization and the
// TFT_eSPI basic example finish with INVON.
struct Config_Seeed_1inch14_LCD_ST7789 {
    using Driver = Driver_ST7789;
    using Panel  = Panel_TFT;
    static constexpr uint16_t width      = 135;
    static constexpr uint16_t height     = 240;
    static constexpr uint8_t  colorDepth = 16;
    static constexpr uint8_t  rgbOrder   = 0x00; // RGB
    static constexpr bool     invert     = true;
};

// --- 1.69 inch LCD Display ---
// Seeed product: 1.69inch LCD Display Module, 240x280, SPI, IPS, ST7789V2.
// Color order RGB (ST7789 controller default). If red/blue appear swapped,
// change rgbOrder to 0x08 (BGR).
struct Config_Seeed_1inch69_LCD_ST7789 {
    using Driver = Driver_ST7789;
    using Panel  = Panel_TFT;
    static constexpr uint16_t width      = 240;
    static constexpr uint16_t height     = 280;
    static constexpr uint8_t  colorDepth = 16;
    static constexpr uint8_t  rgbOrder   = 0x00; // ST7789 RGB
    static constexpr uint8_t  initialRotation = 3; // Landscape 280x240, hardware upright
};

// --- XIAO Expansion Board + 2.4" TFT ---
// 产品: XIAO Expansion Board + 2.4" TFT Display
// 芯片: ILI9341
// 尺寸: 2.4 英寸, 240x320 像素, 16-bit RGB565
struct Config_XIAO_Expansion_2inch4_ILI9341 {
    using Driver = Driver_ILI9341;
    using Panel  = Panel_TFT;
    static constexpr uint16_t width      = 240;
    static constexpr uint16_t height     = 320;
    static constexpr uint8_t  colorDepth = 16;
};

// 第二部分: XIAO ePaper 扩展板系列 - 单色 (Black & White)
// 接口: XIAO ePaper Driver Board / Breakout Board
// 板卡: Board_XIAO_EPaper_Driver / Board_XIAO_EPaper_Breakout

// --- XIAO ePaper 1.54" 单色 ---
// 产品: XIAO ePaper 1.54" 单色 (200x200)
// 芯片: SSD1681
// 尺寸: 1.54 英寸, 200x200 像素, 1-bit 黑白
struct Config_XIAO_EPaper_1inch54_BW_SSD1681 {
    using Driver = Driver_SSD1681;
    using Panel  = Panel_EPaper;
    static constexpr uint16_t width      = 200;
    static constexpr uint16_t height     = 200;
    static constexpr uint8_t  colorDepth = 1;
    static constexpr bool     breakoutDisplayHorizontalMirror = true;
};

// --- XIAO ePaper 2.13" 单色 ---
// 面板: GDEY0213B74；控制器: SSD1680
// 官网可见分辨率是 122x250，颜色为黑/白。
// 重要：SSD1680 的行地址按整字节组织，122 像素需要 16 字节，
// 因此控制器/帧缓冲必须保留 128x250 的存储几何。width/height 是用户
// 可以绘制的可见区域；storageWidth/storageHeight 只用于驱动构造、
// 帧缓冲步长和传输。每行末尾 6 个隐藏像素不会暴露给绘图 API，并在
// 刷屏前被强制清为白色。这等价于原 Seeed_GFX 的 128 宽 +
// COL_OFFSET=6 viewport，不能把存储宽度直接改成 122。
struct Config_XIAO_EPaper_2inch13_BW_SSD1680 {
    using Driver = Driver_SSD1680;
    using Panel  = Panel_EPaper;
    static constexpr uint16_t width      = 122;
    static constexpr uint16_t height     = 250;
    static constexpr uint16_t storageWidth  = 128;
    static constexpr uint16_t storageHeight = 250;
    static constexpr uint8_t  colorDepth = 1;
    static constexpr uint8_t  nativeColorCount = 2;
    static constexpr uint8_t  nativeGrayLevels = 2;
    static constexpr bool     breakoutDisplayHorizontalMirror = true;
    static constexpr EPaperColorSystem colorSystem =
        EPaperColorSystem::Monochrome;
};

// --- XIAO ePaper 2.9" 单色 ---
// 产品: XIAO ePaper 2.9" 单色 (128x296)
// 芯片: SSD1680
// 尺寸: 2.9 英寸, 128x296 像素, 1-bit 黑白
struct Config_XIAO_EPaper_2inch9_BW_SSD1680 {
    using Driver = Driver_SSD1680;
    using Panel  = Panel_EPaper;
    static constexpr uint16_t width      = 128;
    static constexpr uint16_t height     = 296;
    static constexpr uint8_t  colorDepth = 1;
    static constexpr bool     breakoutDisplayHorizontalMirror = true;
};

// --- XIAO ePaper 2.9" 柔性单色 (FLEX) ---
// 产品: XIAO ePaper 2.9" Flexible Monochrome (GDEW029I6FD, 128x296 native)
// 芯片: UC8151D (UltraChip, OTP LUT path)
// 尺寸: 2.9 英寸柔性, 128x296 像素, 1-bit 黑白
// 与刚性 2.9" (SSD1680) 分离: FLEX 用 UC8151D 控制器及 GDEW029I6FD OTP 初始化。
struct Config_XIAO_EPaper_2inch9_Flex_BW_UC8151D {
    using Driver = Driver_UC8151D;
    using Panel  = Panel_EPaper;
    static constexpr uint16_t width      = 128;
    static constexpr uint16_t height     = 296;
    static constexpr uint8_t  colorDepth = 1;
};


// --- XIAO ePaper 4.2" 单色 ---
// 产品: XIAO ePaper 4.2" 单色 (400x300)
// 芯片: SSD1683
// 尺寸: 4.2 英寸, 400x300 像素, 1-bit 黑白
struct Config_XIAO_EPaper_4inch2_BW_SSD1683 {
    using Driver = Driver_SSD1683;
    using Panel  = Panel_EPaper;
    static constexpr uint16_t width      = 400;
    static constexpr uint16_t height     = 300;
    static constexpr uint8_t  colorDepth = 1;
    static constexpr bool     breakoutDisplayVerticalMirror = true;
};

// --- XIAO ePaper 4.26" 单色 ---
// 产品: XIAO ePaper 4.26" 单色 (800x480)
// 芯片: SSD1677
// 注意: 4.26" 与 7.5" 分辨率相同 (800x480), 但尺寸不同
struct Config_XIAO_EPaper_4inch26_BW_SSD1677 {
    using Driver = Driver_SSD1677;
    using Panel  = Panel_EPaper;
    static constexpr uint16_t width      = 800;
    static constexpr uint16_t height     = 480;
    static constexpr uint8_t  colorDepth = 1;
    static constexpr bool     mirror     = true; // 4.26" glass is horizontally mirrored
};

// --- XIAO ePaper 5.83" 单色 ---
// 产品: XIAO ePaper 5.83" 单色 (648x480)
// 芯片: UC8179
// 尺寸: 5.83 英寸, 648x480 像素, 1-bit 黑白
struct Config_XIAO_EPaper_5inch83_BW_UC8179 {
    using Driver = Driver_UC8179;
    using Panel  = Panel_EPaper;
    static constexpr uint16_t width      = 648;
    static constexpr uint16_t height     = 480;
    static constexpr uint8_t  colorDepth = 1;
};

// --- XIAO ePaper 7.5" 单色 (EE04 驱动板) ---
// 产品: XIAO ePaper 7.5" 单色 + EE04 驱动板 (800x480)
// 芯片: UC8179
// 尺寸: 7.5 英寸, 800x480 像素, 1-bit 黑白
// Board-independent 7.5" UC8179 config. Select Driver Board or EE04 in
// begin<Board, Config>(); the legacy EE04-specific type remains an alias.
struct Config_XIAO_EPaper_7inch5_BW_UC8179 {
    using Driver = Driver_UC8179;
    using Panel  = Panel_EPaper;
    static constexpr uint16_t width      = 800;
    static constexpr uint16_t height     = 480;
    static constexpr uint8_t  colorDepth = 1;
};

struct Config_XIAO_EPaper_7inch5_BW_JD79686B {
    using Driver = Driver_JD79686B;
    using Panel  = Panel_EPaper;
    static constexpr uint16_t width      = 800;
    static constexpr uint16_t height     = 480;
    static constexpr uint8_t  colorDepth = 1;
};

// --- XIAO ePaper 10.3" 单色 ---
// 产品: XIAO ePaper 10.3" 单色 (1872x1404)
// 芯片: ED103TC2（需要外部 TCON 实现）
// 尺寸: 10.3 英寸, 1872x1404 像素, 1-bit 黑白
struct Config_XIAO_EPaper_10inch3_BW_ED103TC2 {
    using Driver = Driver_ED103TC2;
    using Panel  = Panel_EPaper;
    static constexpr uint16_t width      = 1872;
    static constexpr uint16_t height     = 1404;
    static constexpr uint8_t  colorDepth = 1;
};

// 第三部分: XIAO ePaper 扩展板系列 - 6色彩色 (Colorful)
// 支持: 黑色、白色、红色、黄色、蓝色、绿色
// 使用方式: begin() 后调用 panel->initColorfulMode()

// --- XIAO ePaper 7.3" 6色彩色 ---
// 产品: XIAO ePaper 7.3" 6色彩色 (800x480)
// 芯片: ED2208
// 颜色: 黑/白/红/黄/蓝/绿
struct Config_XIAO_EPaper_7inch3_Colorful_ED2208 {
    using Driver = Driver_ED2208;
    using Panel  = Panel_EPaper;
    static constexpr uint16_t width      = 800;
    static constexpr uint16_t height     = 480;
    static constexpr uint8_t  colorDepth = 4;
    static constexpr PanelMode panelMode() { return PanelMode::Colorful; }
};

// --- XIAO ePaper 13.3" 6色彩色 ---
// 产品: XIAO ePaper 13.3" 6色彩色 (1200x1600)
// 芯片: T133A01
// 颜色: 黑/白/红/黄/蓝/绿
struct Config_XIAO_EPaper_13inch3_Colorful_T133A01 {
    using Driver = Driver_T133A01;
    using Panel  = Panel_EPaper;
    static constexpr uint16_t width      = 1200;
    static constexpr uint16_t height     = 1600;
    static constexpr uint8_t  colorDepth = 4;
    static constexpr PanelMode panelMode() { return PanelMode::Colorful; }
};

// 第四部分: XIAO ePaper 扩展板系列 - BWRY 四色
// 支持: 黑色、白色、红色、黄色

// --- XIAO ePaper 2.13" BWRY 四色 ---
// 产品/SKU: 2.13" Quadruple Color ePaper, 104990846
// 面板: GDEY0213F51；Seeed_GFX 兼容驱动名: JD79676
// 官网可见分辨率是 122x250，四种原生颜色为黑/白/红/黄。
// 重要：与上面的单色屏相同，控制器传输行必须保持 128 像素宽。
// 这里的 colorDepth=4 表示库内部用 4bpp 调色板暂存像素，
// 并不表示 16 级灰度；JD79676 驱动会在发送时转换为面板四色码。
// 122 个可见像素占 61 字节，而驱动按双字节成组转换，因此仍必须
// 使用 64 字节/行（128 像素）的存储缓冲，末尾 6 像素固定为白色。
struct Config_XIAO_EPaper_2inch13_BWRY_JD79676 {
    using Driver = Driver_JD79676;
    using Panel  = Panel_EPaper;
    static constexpr uint16_t width      = 122;
    static constexpr uint16_t height     = 250;
    static constexpr uint16_t storageWidth  = 128;
    static constexpr uint16_t storageHeight = 250;
    static constexpr uint8_t  colorDepth = 4;
    static constexpr uint8_t  nativeColorCount = 4;
    static constexpr uint8_t  nativeGrayLevels = 0;
    static constexpr EPaperColorSystem colorSystem =
        EPaperColorSystem::BWRY;
    static constexpr PanelMode panelMode() { return PanelMode::BWRY; }
};

// --- XIAO ePaper 2.9" BWRY 四色 ---
// 产品: XIAO ePaper 2.9" BWRY (128x296)
// 芯片: JD79667
// 颜色: 黑/白/红/黄
struct Config_XIAO_EPaper_2inch9_BWRY_JD79667 {
    using Driver = Driver_JD79667;
    using Panel  = Panel_EPaper;
    static constexpr uint16_t width      = 128;
    static constexpr uint16_t height     = 296;
    static constexpr uint8_t  colorDepth = 4;
    static constexpr PanelMode panelMode() { return PanelMode::BWRY; }
};


// 第五部分: Wio Terminal 系列

// --- Wio Terminal ---
// 产品: Wio Terminal (内置 2.4" ILI9341 320x240 TFT)
// SKU:  102991299
// 芯片: ILI9341
// 尺寸: 2.4 英寸, 320x240 像素, 16-bit RGB565
// 链接: https://wiki.seeedstudio.com/Wio-Terminal-Getting-Started/
struct Config_Wio_Terminal_ILI9341 {
    using Driver = Driver_ILI9341;
    using Panel  = Panel_TFT;
    static constexpr uint16_t width      = 240;
    static constexpr uint16_t height     = 320;
    static constexpr uint8_t  colorDepth = 16;
    static constexpr uint8_t  initialRotation = 3;
};

// 第六部分: reTerminal 系列

// --- reTerminal E1001 ---
// 产品: reTerminal E1001 (7.5" ePaper, 800x480)
// 面板/控制器: GDEY075T7 / UC8179
// 硬件支持 4 级灰阶；colorDepth=1 是省内存的默认黑白模式。
// 需要灰阶时显式使用 PanelMode::Gray4 或 initGrayMode(4)，对应 2bpp、
// 96,000 字节帧缓冲，不在 begin() 时自动启用。
// 板卡: Board_Custom (固定 GPIO)
struct Config_reTerminal_E1001_UC8179 {
    using Driver = Driver_UC8179;
    using Panel  = Panel_EPaper;
    static constexpr uint16_t width      = 800;
    static constexpr uint16_t height     = 480;
    static constexpr uint8_t  colorDepth = 1;
    static constexpr uint8_t  nativeColorCount = 0;
    static constexpr uint8_t  nativeGrayLevels = 4;
    static constexpr EPaperColorSystem colorSystem =
        EPaperColorSystem::MonochromeGray;
};

// --- reTerminal E1002 ---
// 产品: reTerminal E1002 (7.3", 800x480, E Ink Spectra 6)
// 面板/控制器: GDEP073E01 / ED2208
// 六种原生颜色: 黑/白/红/黄/绿/蓝。colorDepth=4 是索引帧缓冲位数，
// 不是“四色”或“16 级灰度”；Spectra 6 不支持局部刷新。
struct Config_reTerminal_E1002_ED2208 {
    using Driver = Driver_ED2208;
    using Panel  = Panel_EPaper;
    static constexpr uint16_t width      = 800;
    static constexpr uint16_t height     = 480;
    static constexpr uint8_t  colorDepth = 4;
    static constexpr uint8_t  nativeColorCount = 6;
    static constexpr uint8_t  nativeGrayLevels = 0;
    static constexpr EPaperColorSystem colorSystem =
        EPaperColorSystem::Spectra6;
    static constexpr PanelMode panelMode() { return PanelMode::Colorful; }
};

// --- reTerminal E1003 ---
// 产品: reTerminal E1003 (10.3" 单色触摸 ePaper)
// 面板/控制器: ED103TC2 / IT8951 TCON
// 商城按竖屏方向标为 1404x1872；驱动原生传输方向是 1872x1404，
// 两者只是旋转关系，因此这里保持原生方向，避免改变已验证的传输。
// 硬件支持 16 级灰阶；colorDepth=1 是默认黑白模式。Gray16 需显式
// 启用并使用约 1.25 MiB 的 4bpp PSRAM 帧缓冲。
struct Config_reTerminal_E1003_ED103TC2 {
    using Driver = Driver_ED103TC2;
    using Panel  = Panel_EPaper;
    static constexpr uint16_t width      = 1872;
    static constexpr uint16_t height     = 1404;
    static constexpr uint8_t  colorDepth = 1;
    static constexpr uint16_t portraitWidth  = 1404;
    static constexpr uint16_t portraitHeight = 1872;
    static constexpr uint8_t  nativeColorCount = 0;
    static constexpr uint8_t  nativeGrayLevels = 16;
    static constexpr EPaperColorSystem colorSystem =
        EPaperColorSystem::MonochromeGray;
};

// --- reTerminal E1004 ---
// 产品: reTerminal E1004 (13.3", 1200x1600, E Ink Spectra 6)
// 面板: T133A01，双芯片/双 CS 传输
// 六种原生颜色: 黑/白/红/黄/绿/蓝。colorDepth=4 是索引帧缓冲位数，
// 不是四色或 16 级灰度；Spectra 6 采用全屏刷新。
struct Config_reTerminal_E1004_T133A01 {
    using Driver = Driver_T133A01;
    using Panel  = Panel_EPaper;
    static constexpr uint16_t width      = 1200;
    static constexpr uint16_t height     = 1600;
    static constexpr uint8_t  colorDepth = 4;
    static constexpr uint8_t  nativeColorCount = 6;
    static constexpr uint8_t  nativeGrayLevels = 0;
    static constexpr EPaperColorSystem colorSystem =
        EPaperColorSystem::Spectra6;
    static constexpr PanelMode panelMode() { return PanelMode::Colorful; }
};

// --- reTerminal Sticky ---
// 产品: reTerminal Sticky (3.94" ePaper, 800x480)
// 芯片: SSD1677
// 注意: 受元器件供货影响，Sticky 产线混用 SSD1677 与 SSD2677 两种模组，
// 每台设备只装其中一种，买到哪一种随机。两种芯片共用同一套
// SPI/DC/CS/BUSY/RESET 接线。推荐走产品目录路径
// (Seeed_Product::RETERMINAL_Sticky)，其 Driver_Sticky_Auto 会在 begin()
// 时按固件同款探测自动选择驱动: 复位 -> 发 0x70 -> 读回 1 字节 ->
// 0x07 为 SSD2677，否则 SSD1677。本配置仅用于已知装 SSD1677 的
// begin<Board, Config> 直连写法。
struct Config_reTerminal_Sticky_SSD1677 {
    using Driver = Driver_SSD1677;
    using Panel  = Panel_EPaper;
    static constexpr uint16_t width      = 800;
    static constexpr uint16_t height     = 480;
    static constexpr uint8_t  colorDepth = 1;
};

// --- reTerminal Sticky (SSD2677 变体) ---
// 产品: reTerminal Sticky (3.94" ePaper, 800x480)
// 芯片: SSD2677
struct Config_reTerminal_Sticky_SSD2677 {
    using Driver = Driver_SSD2677;
    using Panel  = Panel_EPaper;
    static constexpr uint16_t width      = 800;
    static constexpr uint16_t height     = 480;
    static constexpr uint8_t  colorDepth = 1;
    static constexpr bool     mirror     = true;
};

#endif // SEEED_GFX_SEEED_PANEL_CONFIGS_H
