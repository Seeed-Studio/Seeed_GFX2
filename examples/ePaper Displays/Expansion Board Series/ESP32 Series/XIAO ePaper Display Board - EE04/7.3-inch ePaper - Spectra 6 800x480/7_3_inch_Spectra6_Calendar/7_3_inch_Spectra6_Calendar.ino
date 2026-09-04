// Example: 7_3_inch_Spectra6_Calendar.ino
// XIAO ePaper Display Board (ESP32-S3) - EE04
// 7.3 inch full-color E Ink Spectra 6 calendar demo
// Panel: 800x480, ED2208

#include <Seeed_GFX.h>
Seeed_GFX display(Seeed_Product::Seeed_ePaper_7INCH3_C);

// 月历视图
void drawMonthlyCalendar(int year, int month) {
    display.fillScreen(TFT_WHITE);

    // 月份标题
    display.fillRect(0, 0, 800, 50, TFT_BLACK);
    display.setTextColor(TFT_WHITE);
    const char* monthNames[] = {"", "January", "February", "March", "April",
                                "May", "June", "July", "August", "September",
                                "October", "November", "December"};
    char title[32];
    snprintf(title, sizeof(title), "%s %d", monthNames[month], year);
    display.setTextSize(3);
    display.drawString(title, 30, 8);

    // 星期头
    display.setTextColor(TFT_BLACK);
    display.setTextSize(2);
    const char* days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    int colW = 110;
    int startX = 15;
    for (int i = 0; i < 7; i++) {
        int dx = startX + i * colW;
        display.fillRect(dx, 55, colW - 2, 25, TFT_BLACK);
        display.setTextColor(TFT_WHITE);
        display.drawString(days[i], dx + 10, 58);
    }

    // 计算当月第一天是周几
    // 2026年7月1日是周三 (dayOfWeek=3)
    int dayOfWeek = 3; // 0=Sun, 3=Wed
    int daysInMonth = 31; // July

    // 日期格子
    display.setTextColor(TFT_BLACK);
    display.setTextSize(2);
    int day = 1;
    for (int row = 0; row < 6; row++) {
        for (int col = 0; col < 7; col++) {
            int dx = startX + col * colW;
            int dy = 85 + row * 55;

            if (row == 0 && col < dayOfWeek) continue;
            if (day > daysInMonth) break;

            // 日期背景
            if (day == 10) {
                // 今天高亮
                display.fillRoundRect(dx, dy, colW - 4, 48, 6, TFT_BLACK);
                display.setTextColor(TFT_WHITE);
            } else if (col == 0 || col == 6) {
                // 周末
                display.fillRoundRect(dx, dy, colW - 4, 48, 6, TFT_LIGHTGREY);
                display.setTextColor(TFT_BLACK);
            } else {
                display.drawRoundRect(dx, dy, colW - 4, 48, 6, TFT_BLACK);
                display.setTextColor(TFT_BLACK);
            }

            char buf[4];
            snprintf(buf, sizeof(buf), "%d", day);
            display.drawString(buf, dx + 8, dy + 5);

            // 事件标记 (示例)
            if (day == 10) {
                display.setTextColor(TFT_WHITE);
                display.setTextSize(1);
                display.drawString("Meeting", dx + 5, dy + 28);
            } else if (day == 15) {
                display.setTextSize(1);
                display.drawString("Deadline", dx + 5, dy + 28);
            } else if (day == 22) {
                display.setTextSize(1);
                display.drawString("Review", dx + 5, dy + 28);
            }

            day++;
        }
    }

    const GfxResult refreshResult = display.refresh();
    if (!refreshResult) Serial.println(refreshResult.message);
}

// 周计划视图
void drawWeeklyPlanner() {
    display.fillScreen(TFT_WHITE);

    display.fillRect(0, 0, 800, 40, TFT_BLACK);
    display.setTextColor(TFT_WHITE);
    display.setTextSize(2);
    display.drawString("Weekly Planner - July 6-12, 2026", 20, 10);

    const char* weekDays[] = {"Mon 6", "Tue 7", "Wed 8", "Thu 9", "Fri 10", "Sat 11", "Sun 12"};
    const char* events[] = {
        "  Team standup", "  Client call", "  Sprint review", "  Workshop",
        "  Demo day!", "  Rest", "  Rest"
    };
    const char* times[] = {
        "9:00 AM", "10:00 AM", "2:00 PM", "9:00 AM", "3:00 PM", "--", "--"
    };
    const char* notes[] = {
        "Prepare slides", "Review proposal", "Demo features", "Bring laptop",
        "Show results", "Family time", "Plan next week"
    };

    int y = 55;
    for (int i = 0; i < 7; i++) {
        // 日期列
        bool isToday = (i == 4); // Friday = today

        if (isToday) {
            display.fillRoundRect(10, y, 780, 52, 6, TFT_BLACK);
            display.setTextColor(TFT_WHITE);
        } else {
            display.drawRoundRect(10, y, 780, 52, 6, TFT_BLACK);
            display.setTextColor(TFT_BLACK);
        }

        display.setTextSize(2);
        display.drawString(weekDays[i], 25, y + 5);
        display.drawString(events[i], 150, y + 5);
        display.setTextSize(1);
        display.drawString(times[i], 25, y + 30);
        display.drawString(notes[i], 150, y + 30);

        y += 60;
    }

    const GfxResult refreshResult = display.refresh();
    if (!refreshResult) Serial.println(refreshResult.message);
}

// 待办清单
void drawTodoList() {
    display.fillScreen(TFT_WHITE);

    display.fillRect(0, 0, 800, 40, TFT_BLACK);
    display.setTextColor(TFT_WHITE);
    display.setTextSize(2);
    display.drawString("TODO List", 20, 10);

    struct TodoItem {
        const char* task;
        bool done;
        const char* priority;
    };

    TodoItem items[] = {
        {"Complete project documentation",     true,  "HIGH"},
        {"Fix ePaper display refresh bug",     true,  "HIGH"},
        {"Add 7.3\" colorful ePaper support",  true,  "MED"},
        {"Create example sketches",            true,  "MED"},
        {"Test on XIAO ESP32S3 Plus",          false, "HIGH"},
        {"Optimize frame buffer memory",       false, "MED"},
        {"Add partial update support",         false, "LOW"},
        {"Write user guide",                   false, "LOW"},
        {"Release v2.0.1",                     false, "HIGH"},
        {"Plan v2.1 features",                 false, "LOW"},
    };

    int y = 55;
    for (int i = 0; i < 10; i++) {
        // 复选框
        int boxX = 25, boxY = y + 5;
        display.drawRect(boxX, boxY, 18, 18, TFT_BLACK);
        if (items[i].done) {
            // 勾选标记
            display.drawLine(boxX + 3, boxY + 9, boxX + 7, boxY + 14, TFT_BLACK);
            display.drawLine(boxX + 7, boxY + 14, boxX + 15, boxY + 3, TFT_BLACK);
        }

        // 任务文字
        display.setTextColor(items[i].done ? TFT_DARKGREY : TFT_BLACK);
        display.setTextSize(1);
        display.drawString(items[i].task, 55, y + 7);

        // 优先级标签
        display.setTextSize(1);
        if (strcmp(items[i].priority, "HIGH") == 0) {
            display.setTextColor(TFT_BLACK);
            display.drawString("[HIGH]", 550, y + 7);
        } else if (strcmp(items[i].priority, "MED") == 0) {
            display.setTextColor(TFT_BLACK);
            display.drawString("[MED]", 550, y + 7);
        } else {
            display.setTextColor(TFT_DARKGREY);
            display.drawString("[LOW]", 550, y + 7);
        }

        y += 35;
    }

    // 进度统计
    int done = 0;
    for (auto& item : items) if (item.done) done++;
    int total = sizeof(items) / sizeof(items[0]);

    display.drawFastHLine(25, y + 5, 600, TFT_BLACK);
    display.setTextColor(TFT_BLACK);
    display.setTextSize(2);
    char buf[32];
    snprintf(buf, sizeof(buf), "Progress: %d/%d (%d%%)", done, total, done * 100 / total);
    display.drawString(buf, 25, y + 15);

    // 进度条
    int barX = 25, barY = y + 45, barW = 600, barH = 15;
    display.drawRect(barX, barY, barW, barH, TFT_BLACK);
    int fillW = (barW - 4) * done / total;
    display.fillRect(barX + 2, barY + 2, fillW, barH - 4, TFT_BLACK);

    const GfxResult refreshResult = display.refresh();
    if (!refreshResult) Serial.println(refreshResult.message);
}

// 主程序
void setup() {
    Serial.begin(115200);
    while (!Serial) delay(100);

    Serial.println("=== EE04 7.3-inch Spectra 6 Calendar Demo ===");

    if (!display.begin()) {
        Serial.printf("Display initialization failed: %s\n",
                      display.lastResult().message);
        return;
    }
    Serial.println("Display initialized!");

    // 演示 1: 月历
    Serial.println("Drawing monthly calendar...");
    drawMonthlyCalendar(2026, 7);
    delay(5000);

    // 演示 2: 周计划
    Serial.println("Drawing weekly planner...");
    drawWeeklyPlanner();
    delay(5000);

    // 演示 3: 待办清单
    Serial.println("Drawing TODO list...");
    drawTodoList();
    delay(5000);

    Serial.println("Calendar Demo Complete!");
}

void loop() {
    delay(5000);
}
