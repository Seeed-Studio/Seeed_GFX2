// Example: 7_5_inch_Monochrome_Slideshow.ino
// XIAO ePaper Display Board (ESP32-S3) - EE04
// 7.5 inch monochrome ePaper slideshow demo
// 展示: 多页切换、图文混排、全屏图形
// Panel: 7.5 inch monochrome ePaper, 800x480

#include <Seeed_GFX.h>
#include "board/boards/XIAO_EPaper_Boards.h"
#include "driver/epaper/Driver_UC8179.h"
#include "panel/Panel_EPaper.h"

Seeed_GFX display;

// 幻灯片 1: 封面
void slideCover() {
    display.fillScreen(TFT_WHITE);

    // 装饰线条
    for (int i = 0; i < 10; i++) {
        display.drawFastHLine(50, 50 + i * 40, 700, TFT_BLACK);
    }

    // 标题区域
    display.fillRect(100, 100, 600, 120, TFT_BLACK);
    display.setTextColor(TFT_WHITE);
    display.setTextSize(5);
    display.drawString("Seeed_GFX", 170, 120);

    display.setTextColor(TFT_BLACK);
    display.setTextSize(2);
    display.drawString("ePaper Display Library v2.0", 200, 250);

    // 特性列表
    display.setTextSize(2);
    const char* features[] = {
        "> Ultra-low Power Consumption",
        "> Sunlight Readable",
        "> 800x480 High Resolution",
        "> Rich Graphics Primitives",
        "> Multi-font Support",
        "> Frame Buffer Architecture",
    };
    for (int i = 0; i < 6; i++) {
        display.drawString(features[i], 150, 300 + i * 30);
    }

    // 底部
    display.drawFastHLine(100, 430, 600, TFT_BLACK);
    display.setTextSize(1);
    display.drawString("www.seeedstudio.com", 300, 440);

    { const GfxResult _r = display.refresh(); if (!_r) Serial.println(_r.message); }
}

// 幻灯片 2: 架构图
void slideArchitecture() {
    display.fillScreen(TFT_WHITE);

    display.fillRect(0, 0, 800, 40, TFT_BLACK);
    display.setTextColor(TFT_WHITE);
    display.setTextSize(2);
    display.drawString("Architecture Overview", 20, 10);

    // 架构层次
    struct Layer {
        const char* name;
        int y;
        int h;
        const char* desc;
    };

    Layer layers[] = {
        {"User Application", 60, 40, "Your Arduino Sketch"},
        {"Seeed_GFX", 110, 40, "Graphics API (Print subclass)"},
        {"Panel", 160, 40, "Panel_TFT / Panel_EPaper / Panel_OLED"},
        {"Driver", 210, 40, "Driver_ST7789 / Driver_UC8179 / ..."},
        {"Bus", 260, 40, "Bus_SPI / Bus_I2C / Bus_Parallel8"},
        {"Board", 310, 40, "Board_XIAO_ESP32S3 / Board_Wio_Terminal / ..."},
        {"Hardware", 360, 40, "MCU GPIO, SPI, I2C Peripherals"},
    };

    for (int i = 0; i < 7; i++) {
        int x = 50 + i * 10;
        int w = 700 - i * 20;
        display.drawRect(x, layers[i].y, w, layers[i].h, TFT_BLACK);
        display.setTextColor(TFT_BLACK);
        display.setTextSize(1);
        display.drawString(layers[i].name, x + 10, layers[i].y + 5);
        display.drawString(layers[i].desc, x + 200, layers[i].y + 20);
    }

    // 箭头
    for (int i = 0; i < 6; i++) {
        int ax = 400;
        int ay1 = layers[i].y + layers[i].h;
        int ay2 = layers[i + 1].y;
        display.drawLine(ax, ay1, ax, ay2, TFT_BLACK);
        display.fillTriangle(ax, ay2, ax - 5, ay2 - 8, ax + 5, ay2 - 8, TFT_BLACK);
    }

    { const GfxResult _r = display.refresh(); if (!_r) Serial.println(_r.message); }
}

// 幻灯片 3: 几何图形展示
void slideGeometry() {
    display.fillScreen(TFT_WHITE);

    display.fillRect(0, 0, 800, 40, TFT_BLACK);
    display.setTextColor(TFT_WHITE);
    display.setTextSize(2);
    display.drawString("Graphics Primitives", 20, 10);

    // 点
    for (int i = 0; i < 50; i++) {
        display.drawPixel(50 + i * 3, 70, TFT_BLACK);
    }

    // 线条
    display.drawLine(50, 90, 250, 90, TFT_BLACK);
    display.drawLine(50, 100, 250, 150, TFT_BLACK);
    display.drawLine(50, 150, 250, 100, TFT_BLACK);

    // 矩形
    display.drawRect(50, 170, 100, 60, TFT_BLACK);
    display.fillRect(170, 170, 80, 60, TFT_BLACK);
    display.drawRoundRect(50, 250, 100, 50, 10, TFT_BLACK);
    display.fillRoundRect(170, 250, 80, 50, 15, TFT_BLACK);

    // 圆形
    display.drawCircle(350, 100, 40, TFT_BLACK);
    display.fillCircle(450, 100, 30, TFT_BLACK);
    display.drawCircle(350, 210, 50, TFT_BLACK);
    display.drawCircle(450, 210, 50, TFT_BLACK);

    // 三角形
    display.drawTriangle(330, 320, 280, 400, 380, 400, TFT_BLACK);
    display.fillTriangle(430, 320, 380, 400, 480, 400, TFT_BLACK);

    // 椭圆
    display.drawEllipse(600, 100, 60, 35, TFT_BLACK);
    display.fillEllipse(600, 200, 40, 60, TFT_BLACK);

    // 标签
    display.setTextSize(1);
    display.drawString("Points", 50, 55);
    display.drawString("Lines", 50, 75);
    display.drawString("Rects", 50, 155);
    display.drawString("Circles", 330, 75);
    display.drawString("Triangles", 330, 305);
    display.drawString("Ellipses", 580, 70);

    { const GfxResult _r = display.refresh(); if (!_r) Serial.println(_r.message); }
}

// 幻灯片 4: 文字排版
void slideTypography() {
    display.fillScreen(TFT_WHITE);

    display.fillRect(0, 0, 800, 40, TFT_BLACK);
    display.setTextColor(TFT_WHITE);
    display.setTextSize(2);
    display.drawString("Typography & Fonts", 20, 10);

    // 字体大小
    display.setTextColor(TFT_BLACK);
    int y = 60;
    for (int s = 1; s <= 6; s++) {
        display.setTextSize(s);
        char buf[16];
        snprintf(buf, sizeof(buf), "Size %d", s);
        display.drawString(buf, 20, y);
        y += s * 10 + 8;
    }

    // 文字样式
    display.setTextSize(2);
    display.drawString("Normal", 250, 60);
    display.drawString("Bold text", 250, 80);
    display.drawString("Italic style", 250, 100);

    // 对齐方式
    display.drawFastVLine(400, 60, 200, TFT_BLACK);

    display.setTextSize(1);
    display.drawString("Left Aligned", 410, 70);
    display.drawString("Center Aligned", 410, 90);
    display.drawString("Right Aligned", 410, 110);

    // 特殊字符
    display.setTextSize(3);
    display.drawString("1234567890", 250, 200);
    display.drawString("!@#$%^&*()", 250, 240);
    display.drawString("ABCDEFGHIJ", 250, 280);

    // 底部文字块
    display.drawFastHLine(50, 350, 700, TFT_BLACK);
    display.setTextSize(1);
    display.drawString("The quick brown fox jumps over the lazy dog.", 50, 370);
    display.drawString("Seeed_GFX inherits from Arduino Print class,", 50, 390);
    display.drawString("supporting println(), print(), and printf() formatting.", 50, 410);

    { const GfxResult _r = display.refresh(); if (!_r) Serial.println(_r.message); }
}

// 幻灯片 5: API 使用示例
void slideAPI() {
    display.fillScreen(TFT_WHITE);

    display.fillRect(0, 0, 800, 40, TFT_BLACK);
    display.setTextColor(TFT_WHITE);
    display.setTextSize(2);
    display.drawString("Quick Start API", 20, 10);

    // 代码块
    display.drawRect(30, 60, 740, 250, TFT_BLACK);
    display.fillRect(30, 60, 740, 25, TFT_BLACK);
    display.setTextColor(TFT_WHITE);
    display.setTextSize(1);
    display.drawString("Example Sketch", 40, 65);

    display.setTextColor(TFT_BLACK);
    const char* code[] = {
        "#include <Seeed_GFX.h>",
        "#include \"board/boards/XIAO_EPaper_Boards.h\"",
        "#include \"driver/epaper/Driver_UC8179.h\"",
        "#include \"panel/Panel_EPaper.h\"",
        "",
        "Seeed_GFX display;",
        "",
        "void setup() {",
        "  display.begin<Board_XIAO_EPaper_EE04,",
        "            Config_XIAO_EPaper_7inch5_BW_UC8179>();",
        "  display.fillScreen(TFT_WHITE);",
        "  display.setTextColor(TFT_BLACK);",
        "  display.drawString(\"Hello!\", 50, 50);",
        "  display.update();",
        "}",
    };

    int cy = 95;
    for (int i = 0; i < 16; i++) {
        display.drawString(code[i], 40, cy);
        cy += 14;
    }

    // 右侧说明
    display.drawString("1. Include headers", 430, 100);
    display.drawString("2. Create display object", 430, 200);
    display.drawString("3. Initialize with template", 430, 240);
    display.drawString("4. Draw your content", 430, 290);
    display.drawString("5. Call update() to refresh", 430, 310);

    // 底部
    display.drawFastHLine(50, 400, 700, TFT_BLACK);
    display.setTextSize(2);
    display.drawString("Only 5 lines of code to get started!", 130, 420);

    { const GfxResult _r = display.refresh(); if (!_r) Serial.println(_r.message); }
}

// 主程序
void setup() {
    Serial.begin(115200);
    while (!Serial) delay(100);

    Serial.println("=== EE04 7.5-inch Monochrome Slideshow Demo ===");

    if (!display.begin<Board_XIAO_EPaper_EE04, Config_XIAO_EPaper_7inch5_BW_UC8179>()) {
        Serial.println(display.lastResult().message);
        return;
    }
    Serial.println("Display initialized!");

    // 自动播放幻灯片 (每张 5 秒)
    Serial.println("Slide 1: Cover");
    slideCover();
    delay(5000);

    Serial.println("Slide 2: Architecture");
    slideArchitecture();
    delay(5000);

    Serial.println("Slide 3: Geometry");
    slideGeometry();
    delay(5000);

    Serial.println("Slide 4: Typography");
    slideTypography();
    delay(5000);

    Serial.println("Slide 5: API");
    slideAPI();
    delay(5000);

    Serial.println("Slideshow Complete!");
}

void loop() {
    delay(5000);
}
