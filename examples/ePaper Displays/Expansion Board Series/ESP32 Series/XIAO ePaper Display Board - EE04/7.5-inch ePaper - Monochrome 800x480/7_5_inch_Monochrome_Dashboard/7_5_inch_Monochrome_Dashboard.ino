// Example: 7_5_inch_Monochrome_Dashboard.ino
// XIAO ePaper Display Board (ESP32-S3) - EE04
// 7.5 inch monochrome ePaper dashboard demo
// 展示: 天气卡片、数据面板、图表、进度条
// Panel: 7.5 inch monochrome ePaper, 800x480

#include <Seeed_GFX.h>
#include "board/boards/XIAO_ePaper_Boards.h"
#include "driver/epaper/Driver_UC8179.h"
#include "panel/Panel_EPaper.h"

Seeed_GFX display;

// 天气卡片
void drawWeatherCard(int x, int y, int w, int h,
                     const char* day, const char* icon, int tempHi, int tempLo) {
    display.fillRoundRect(x, y, w, h, 8, TFT_WHITE);
    display.drawRoundRect(x, y, w, h, 8, TFT_BLACK);

    display.setTextColor(TFT_BLACK);
    display.setTextSize(2);
    display.drawString(day, x + 10, y + 5);

    display.setTextSize(3);
    display.drawString(icon, x + 10, y + 35);

    display.setTextSize(2);
    char buf[16];
    snprintf(buf, sizeof(buf), "%d/%dC", tempHi, tempLo);
    display.drawString(buf, x + 10, y + h - 35);
}

// 系统状态面板
void drawSystemPanel(int x, int y, int w, int h) {
    display.fillRoundRect(x, y, w, h, 8, TFT_WHITE);
    display.drawRoundRect(x, y, w, h, 8, TFT_BLACK);

    display.setTextColor(TFT_BLACK);
    display.setTextSize(2);
    display.drawString("System Status", x + 10, y + 5);
    display.drawFastHLine(x + 10, y + 35, w - 20, TFT_BLACK);

    struct StatusItem {
        const char* label;
        const char* value;
        bool ok;
    };

    StatusItem items[] = {
        {"CPU Usage",   "23%",    true},
        {"Memory",      "45/512KB", true},
        {"WiFi Signal", "-42dBm", true},
        {"Battery",     "85%",    true},
        {"Storage",     "1.2/16MB", false},
    };

    display.setTextSize(1);
    int iy = y + 45;
    for (auto& item : items) {
        display.drawString(item.label, x + 15, iy);
        display.drawString(item.value, x + w - 80, iy);
        display.fillCircle(x + w - 20, iy + 4, 4, item.ok ? TFT_BLACK : TFT_WHITE);
        if (!item.ok) display.drawCircle(x + w - 20, iy + 4, 4, TFT_BLACK);
        iy += 22;
    }
}

// 进度条
void drawProgressBar(int x, int y, int w, int h, int percent, const char* label) {
    display.setTextColor(TFT_BLACK);
    display.setTextSize(1);
    display.drawString(label, x, y - 15);

    display.drawRect(x, y, w, h, TFT_BLACK);
    int fillW = (w - 4) * percent / 100;
    if (fillW > 0) {
        display.fillRect(x + 2, y + 2, fillW, h - 4, TFT_BLACK);
    }

    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", percent);
    display.drawString(buf, x + w + 5, y - 1);
}

// 简单折线图
void drawLineChart(int x, int y, int w, int h,
                   const int* data, int count, const char* title) {
    display.fillRect(x, y, w, h, TFT_WHITE);
    display.drawRect(x, y, w, h, TFT_BLACK);

    display.setTextColor(TFT_BLACK);
    display.setTextSize(1);
    display.drawString(title, x + 5, y + 2);

    // 坐标轴
    display.drawFastHLine(x + 30, y + h - 20, w - 35, TFT_BLACK);
    display.drawFastVLine(x + 30, y + 20, h - 40, TFT_BLACK);

    if (count < 2) return;

    int maxVal = data[0], minVal = data[0];
    for (int i = 1; i < count; i++) {
        if (data[i] > maxVal) maxVal = data[i];
        if (data[i] < minVal) minVal = data[i];
    }
    if (maxVal == minVal) maxVal = minVal + 1;

    int chartW = w - 35;
    int chartH = h - 40;
    int baseY = y + h - 20;

    // 数据线
    for (int i = 0; i < count - 1; i++) {
        int x1 = x + 30 + i * chartW / (count - 1);
        int y1 = baseY - (data[i] - minVal) * chartH / (maxVal - minVal);
        int x2 = x + 30 + (i + 1) * chartW / (count - 1);
        int y2 = baseY - (data[i + 1] - minVal) * chartH / (maxVal - minVal);
        display.drawLine(x1, y1, x2, y2, TFT_BLACK);
        display.fillCircle(x1, y1, 2, TFT_BLACK);
    }
    display.fillCircle(x + 30 + (count - 1) * chartW / (count - 1),
                   baseY - (data[count - 1] - minVal) * chartH / (maxVal - minVal),
                   2, TFT_BLACK);
}

// 主仪表盘
void drawDashboard() {
    display.fillScreen(TFT_WHITE);

    // 标题栏
    display.fillRect(0, 0, 800, 40, TFT_BLACK);
    display.setTextColor(TFT_WHITE);
    display.setTextSize(3);
    display.drawString("DASHBOARD", 20, 5);
    display.setTextSize(1);
    display.drawString("Last updated: 2026-07-10 10:30", 500, 15);

    // 天气卡片 (左侧 3 列)
    drawWeatherCard(10, 55, 190, 140, "Mon", "*", 28, 18);
    drawWeatherCard(210, 55, 190, 140, "Tue", "~", 30, 20);
    drawWeatherCard(410, 55, 190, 140, "Wed", "~", 25, 15);
    drawWeatherCard(610, 55, 180, 140, "Thu", "*", 27, 19);

    // 系统状态面板 (左侧)
    drawSystemPanel(10, 210, 250, 200);

    // 进度条区域 (中间)
    display.setTextColor(TFT_BLACK);
    display.setTextSize(2);
    display.drawString("Tasks", 280, 210);
    drawProgressBar(280, 240, 200, 15, 75, "Firmware Update");
    drawProgressBar(280, 270, 200, 15, 100, "Data Sync");
    drawProgressBar(280, 300, 200, 15, 45, "Backup");
    drawProgressBar(280, 330, 200, 15, 10, "Upload");

    // 折线图 (右侧)
    int chartData[] = {45, 52, 38, 65, 48, 70, 55, 62, 58, 72, 68, 75};
    drawLineChart(500, 210, 290, 200, chartData, 12, "CPU Usage (24h)");

    // 底部信息
    display.drawFastHLine(10, 430, 780, TFT_BLACK);
    display.setTextSize(1);
    display.drawString("Seeed_GFX v2.0 | XIAO ESP32S3 | EE04 Board | 7.5\" ePaper", 10, 440);

    const GfxResult refreshResult = display.refresh();
    if (!refreshResult) Serial.println(refreshResult.message);
}

// 主程序
void setup() {
    Serial.begin(115200);
    while (!Serial) delay(100);

    Serial.println("=== EE04 7.5-inch Monochrome Dashboard Demo ===");

    if (!display.begin<Board_XIAO_ePaper_EE04, Config_Seeed_ePaper_7inch5_BW_UC8179>()) {
        Serial.println(display.lastResult().message);
        return;
    }
    Serial.println("Display initialized!");

    drawDashboard();
    Serial.println("Dashboard displayed!");
}

void loop() {
    delay(5000);
}
