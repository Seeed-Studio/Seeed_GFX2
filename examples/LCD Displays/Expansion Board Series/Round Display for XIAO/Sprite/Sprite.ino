// Example: Sprite.ino
// 离屏渲染 (Sprite) 演示
// Sprite 允许在内存中创建离屏缓冲区进行绘制，
// 然后一次性推送到主屏幕。适用于:
//   - 动画帧缓冲
//   - 弹出窗口/对话框
//   - 避免闪烁的复杂绘制
// 默认硬件: Seeed Studio Round Display for XIAO

#include <Seeed_GFX.h>
#include "core/Sprite.h"

Seeed_GFX display(Seeed_Product::XIAO_ROUND_DISPLAY);
Seeed_Sprite sprite;

// 演示 1: 弹窗效果
void demoPopup() {
    display.fillScreen(TFT_BLACK);
    display.setTextColor(TFT_WHITE);
    display.setTextSize(1);
    display.drawString("Sprite Popup Demo", 10, 10);

    // 创建弹窗 sprite (100x60, 16bpp)
    sprite.createSprite(display, 100, 60, 1);

    // 在 sprite 上绘制弹窗
    sprite.fillScreen(TFT_DARKGREY);
    sprite.fillRect(1, 1, 98, 20, TFT_NAVY);
    sprite.setTextColor(TFT_WHITE);
    sprite.setTextSize(1);
    sprite.drawString("Alert", 5, 3);
    sprite.drawString("Hello World!", 10, 30);
    sprite.drawRect(0, 0, 100, 60, TFT_WHITE);

    // 推送到主屏幕 (居中)
    int px = (display.width() - 100) / 2;
    int py = (display.height() - 60) / 2;
    sprite.pushSprite(px, py);

    sprite.deleteSprite();
    delay(2000);
}

// 演示 2: 进度条动画
void demoProgressBar() {
    display.fillScreen(TFT_BLACK);

    // 创建进度条 sprite
    sprite.createSprite(display, 200, 30, 1);

    for (int p = 0; p <= 100; p += 5) {
        sprite.fillScreen(TFT_BLACK);
        sprite.drawRect(0, 0, 200, 30, TFT_WHITE);
        sprite.fillRect(2, 2, p * 196 / 100, 26, TFT_GREEN);

        char buf[10];
        snprintf(buf, sizeof(buf), "%d%%", p);
        sprite.setTextColor(TFT_WHITE);
        sprite.setTextSize(1);
        sprite.drawString(buf, 80, 8);

        sprite.pushSprite(20, 100);
        delay(50);
    }

    sprite.deleteSprite();
    delay(1000);
}

// 演示 3: 弹跳球
void demoBouncingBall() {
    int ballSize = 20;

    // 创建球 sprite
    sprite.createSprite(display, ballSize, ballSize, 1);

    // 绘制球
    sprite.fillScreen(TFT_BLACK);
    sprite.fillCircle(ballSize / 2, ballSize / 2, ballSize / 2, TFT_RED);

    int x = 50, y = 50;
    int dx = 3, dy = 2;
    int w = display.width() - ballSize;
    int h = display.height() - ballSize;

    for (int frame = 0; frame < 200; frame++) {
        // 清除旧位置 (用背景色覆盖)
        display.fillRect(x - 1, y - 1, ballSize + 2, ballSize + 2, TFT_BLACK);

        // 更新位置
        x += dx;
        y += dy;
        if (x <= 0 || x >= w) dx = -dx;
        if (y <= 0 || y >= h) dy = -dy;

        // 绘制球
        sprite.pushSprite(x, y);
        delay(10);
    }

    sprite.deleteSprite();
}

// 演示 4: 1bpp 图标 (适用于 ePaper)
void demoIcon() {
    display.fillScreen(TFT_BLACK);

    // 创建 1bpp sprite (节省内存)
    sprite.createSprite(display, 40, 40, 1);

    // 绘制一个简单的图标 (1bpp: 0=黑, 1=白)
    sprite.fillScreen(0xFFFF); // 全白
    sprite.fillCircle(20, 15, 8, 0x0000);   // 头 (黑)
    sprite.fillRect(16, 23, 8, 12, 0x0000);  // 身体
    sprite.fillRect(10, 25, 20, 3, 0x0000);  // 手臂
    sprite.fillRect(16, 35, 3, 5, 0x0000);   // 左腿
    sprite.fillRect(23, 35, 3, 5, 0x0000);   // 右腿

    // 推送到屏幕
    sprite.pushSprite(100, 80);
    display.drawString("1bpp Icon", 95, 130);

    sprite.deleteSprite();
    delay(2000);
}

// 主程序
void setup() {
    Serial.begin(115200);

    display.begin();
    Serial.println("Sprite Demo Start!");

    demoPopup();
    demoProgressBar();
    demoBouncingBall();
    demoIcon();

    display.fillScreen(TFT_BLACK);
    display.setTextColor(TFT_WHITE);
    display.setTextSize(2);
    display.drawString("Sprite Demo", 30, 80);
    display.drawString("Complete!", 60, 110);

    Serial.println("Sprite Demo Complete!");
}

void loop() {
    delay(1000);
}
