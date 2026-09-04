// Example: 7_5_inch_Monochrome_Clock.ino
// XIAO ePaper Display Board (ESP32-S3) - EE04
// 7.5 inch monochrome ePaper clock demo
// 展示: 大字体数字时钟、日期、模拟表盘
// Panel: 7.5 inch monochrome ePaper, 800x480

#include <Seeed_GFX.h>
#include "board/boards/XIAO_ePaper_Boards.h"
#include "driver/epaper/Driver_UC8179.h"
#include "panel/Panel_EPaper.h"

Seeed_GFX display;

// 大字体数字时钟
void drawDigitalClock(int hour, int minute, int second) {
    display.fillScreen(TFT_WHITE);

    // 时间字符串
    char timeStr[16];
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", hour, minute, second);

    // 大字体显示时间 (居中)
    display.setTextColor(TFT_BLACK);
    display.setTextSize(8);
    int textW = strlen(timeStr) * 6 * 8;  // 粗略估算宽度
    display.drawString(timeStr, (display.width() - textW) / 2, 150);

    // 日期
    display.setTextSize(2);
    display.setTextColor(TFT_BLACK);
    display.drawString("July 10, 2026  Thursday", 200, 100);

    // 分隔线
    display.drawFastHLine(150, 330, 500, TFT_BLACK);
    display.drawFastHLine(150, 332, 500, TFT_BLACK);

    // 底部信息
    display.setTextSize(1);
    display.drawString("Seeed ePaper 7.5\" Clock", 150, 360);
    display.drawString("Powered by Seeed_GFX v2.0", 150, 380);
    display.drawString("XIAO ESP32S3 + EE04 Board", 150, 400);

    // 装饰边框
    display.drawRect(10, 10, display.width() - 20, display.height() - 20, TFT_BLACK);
    display.drawRect(15, 15, display.width() - 30, display.height() - 30, TFT_BLACK);

    { const GfxResult _r = display.refresh(); if (!_r) Serial.println(_r.message); }
}

// 模拟表盘
void drawAnalogClock(int hour, int minute) {
    display.fillScreen(TFT_WHITE);

    int cx = 400;
    int cy = 240;
    int r = 200;

    // 外圈
    display.drawCircle(cx, cy, r, TFT_BLACK);
    display.drawCircle(cx, cy, r - 2, TFT_BLACK);

    // 刻度线 (60 条)
    for (int i = 0; i < 60; i++) {
        float angle = i * 6.0 * PI / 180.0;
        bool isHour = (i % 5 == 0);
        int len = isHour ? 25 : 12;
        int thickness = isHour ? 2 : 1;
        int x1 = cx + sin(angle) * (r - len - 10);
        int y1 = cy - cos(angle) * (r - len - 10);
        int x2 = cx + sin(angle) * (r - 10);
        int y2 = cy - cos(angle) * (r - 10);
        for (int t = 0; t < thickness; t++) {
            display.drawLine(x1 + t, y1, x2 + t, y2, TFT_BLACK);
        }
    }

    // 数字 (12, 3, 6, 9)
    display.setTextSize(3);
    display.setTextColor(TFT_BLACK);
    display.drawString("12", cx - 25, cy - r + 25);
    display.drawString("3", cx + r - 45, cy - 20);
    display.drawString("6", cx - 15, cy + r - 55);
    display.drawString("9", cx - r + 15, cy - 20);

    // 时针
    float hAngle = (hour % 12 + minute / 60.0) * 30.0 * PI / 180.0;
    int hx = cx + sin(hAngle) * (r * 0.45);
    int hy = cy - cos(hAngle) * (r * 0.45);
    for (int t = 0; t < 6; t++) {
        display.drawLine(cx + t, cy, hx + t, hy, TFT_BLACK);
    }

    // 分针
    float mAngle = minute * 6.0 * PI / 180.0;
    int mx = cx + sin(mAngle) * (r * 0.65);
    int my = cy - cos(mAngle) * (r * 0.65);
    for (int t = 0; t < 4; t++) {
        display.drawLine(cx + t, cy, mx + t, my, TFT_BLACK);
    }

    // 中心点
    display.fillCircle(cx, cy, 8, TFT_BLACK);
    display.fillCircle(cx, cy, 3, TFT_WHITE);

    // 标题
    display.setTextSize(2);
    display.drawString("Analog Clock", 50, 20);

    { const GfxResult _r = display.refresh(); if (!_r) Serial.println(_r.message); }
}

// 世界时钟 (多时区)
void drawWorldClock(int localHour) {
    display.fillScreen(TFT_WHITE);

    struct TimeZone {
        const char* city;
        int offset;  // UTC offset
    };

    TimeZone zones[] = {
        {"New York",   -4},
        {"London",      1},
        {"Paris",       2},
        {"Dubai",       4},
        {"Beijing",     8},
        {"Tokyo",       9},
        {"Sydney",     10},
    };
    const int numZones = sizeof(zones) / sizeof(zones[0]);

    display.setTextColor(TFT_BLACK);
    display.setTextSize(2);
    display.drawString("World Clock", 20, 10);
    display.drawFastHLine(20, 40, 300, TFT_BLACK);

    int y = 60;
    for (int i = 0; i < numZones; i++) {
        int zoneHour = (localHour + zones[i].offset + 24) % 24;
        char buf[64];
        snprintf(buf, sizeof(buf), "%s  %02d:00", zones[i].city, zoneHour);

        if (zoneHour >= 6 && zoneHour < 18) {
            display.fillCircle(30, y + 8, 6, TFT_BLACK);  // 白天
        } else {
            display.drawCircle(30, y + 8, 6, TFT_BLACK);  // 夜晚
        }

        display.setTextSize(2);
        display.drawString(buf, 50, y);
        y += 40;
    }

    // 右侧装饰
    display.drawCircle(650, 240, 80, TFT_BLACK);
    display.fillCircle(650, 240, 40, TFT_BLACK);

    { const GfxResult _r = display.refresh(); if (!_r) Serial.println(_r.message); }
}

// 主程序
void setup() {
    Serial.begin(115200);
    while (!Serial) delay(100);

    Serial.println("=== EE04 7.5-inch Monochrome Clock Demo ===");

    if (!display.begin<Board_XIAO_ePaper_EE04, Config_Seeed_ePaper_7inch5_BW_UC8179>()) {
        Serial.println(display.lastResult().message);
        return;
    }
    Serial.println("Display initialized!");

    // 演示 1: 数字时钟 (10:30:45)
    Serial.println("Drawing digital clock...");
    drawDigitalClock(10, 30, 45);
    delay(5000);

    // 演示 2: 模拟表盘 (10:10)
    Serial.println("Drawing analog clock...");
    drawAnalogClock(10, 10);
    delay(5000);

    // 演示 3: 世界时钟 (UTC+8 北京时间 10:00)
    Serial.println("Drawing world clock...");
    drawWorldClock(10);
    delay(5000);

    Serial.println("Clock Demo Complete!");
}

void loop() {
    delay(5000);
}
