// Example: Graphics_Test.ino
// 综合图形测试 - 展示 Seeed_GFX v2.0 的所有基础绘图功能
// 默认硬件: Seeed Studio Round Display for XIAO

#include <Seeed_GFX.h>

Seeed_GFX display(Seeed_Product::Seeed_Round_Display_XIAO);

// 测试 1: 颜色填充
void testFillScreen() {
    const uint32_t colors[] = {
        TFT_RED, TFT_GREEN, TFT_BLUE, TFT_YELLOW,
        TFT_CYAN, TFT_MAGENTA, TFT_WHITE, TFT_BLACK
    };
    for (int i = 0; i < 8; i++) {
        display.fillScreen(colors[i]);
        delay(200);
    }
}

// 测试 2: 线条绘制
void testLines() {
    display.fillScreen(TFT_BLACK);
    int w = display.width();
    int h = display.height();

    // 对角线
    for (int i = 0; i < w; i += 8) {
        display.drawLine(0, 0, i, h - 1, TFT_RED);
        display.drawLine(w - 1, 0, i, h - 1, TFT_GREEN);
        display.drawLine(0, h - 1, i, 0, TFT_BLUE);
        display.drawLine(w - 1, h - 1, i, 0, TFT_YELLOW);
    }
    delay(1000);

    // 水平/垂直线
    display.fillScreen(TFT_BLACK);
    for (int i = 0; i < h; i += 5) {
        display.drawFastHLine(0, i, w, TFT_WHITE);
    }
    for (int i = 0; i < w; i += 5) {
        display.drawFastVLine(i, 0, h, TFT_WHITE);
    }
    delay(1000);
}

// 测试 3: 矩形绘制
void testRects() {
    display.fillScreen(TFT_BLACK);
    int w = display.width();
    int h = display.height();

    // 空心矩形
    for (int i = 0; i < w / 2; i += 4) {
        display.drawRect(i, i, w - 2 * i, h - 2 * i, TFT_GREEN);
    }
    delay(1000);

    // 实心矩形
    display.fillScreen(TFT_BLACK);
    for (int i = 0; i < w / 2; i += 8) {
        uint16_t color = (i / 8) % 2 == 0 ? TFT_RED : TFT_BLUE;
        display.fillRect(i, i, w - 2 * i, h - 2 * i, color);
    }
    delay(1000);

    // 圆角矩形
    display.fillScreen(TFT_BLACK);
    display.drawRoundRect(10, 10, 100, 60, 10, TFT_CYAN);
    display.drawRoundRect(130, 10, 100, 60, 20, TFT_MAGENTA);
    display.fillRoundRect(10, 90, 100, 60, 10, TFT_YELLOW);
    display.fillRoundRect(130, 90, 100, 60, 20, TFT_ORANGE);
    delay(1000);
}

// 测试 4: 圆形绘制
void testCircles() {
    display.fillScreen(TFT_BLACK);
    int cx = display.width() / 2;
    int cy = display.height() / 2;

    // 同心圆
    for (int r = 10; r < cx; r += 10) {
        display.drawCircle(cx, cy, r, TFT_GREEN);
    }
    delay(1000);

    // 实心圆
    display.fillScreen(TFT_BLACK);
    int colors[] = {TFT_RED, TFT_ORANGE, TFT_YELLOW, TFT_GREEN, TFT_BLUE, TFT_PURPLE};
    for (int r = 100; r > 0; r -= 15) {
        display.fillCircle(cx, cy, r, colors[(100 - r) / 15 % 6]);
    }
    delay(1000);
}

// 测试 5: 三角形绘制
void testTriangles() {
    display.fillScreen(TFT_BLACK);
    int w = display.width();
    int h = display.height();

    // 空心三角形
    display.drawTriangle(w / 2, 10, 10, h - 10, w - 10, h - 10, TFT_RED);
    display.drawTriangle(w / 2, h - 10, 10, 10, w - 10, 10, TFT_BLUE);
    delay(1000);

    // 实心三角形
    display.fillScreen(TFT_BLACK);
    display.fillTriangle(w / 2, 10, 10, h - 10, w - 10, h - 10, TFT_GREEN);
    display.fillTriangle(w / 2, h - 10, 10, 10, w - 10, 10, TFT_YELLOW);
    delay(1000);
}

// 测试 6: 椭圆绘制
void testEllipses() {
    display.fillScreen(TFT_BLACK);
    int cx = display.width() / 2;
    int cy = display.height() / 2;

    display.drawEllipse(cx, cy, 80, 40, TFT_CYAN);
    display.drawEllipse(cx, cy, 40, 80, TFT_MAGENTA);
    display.fillEllipse(cx, cy, 30, 50, TFT_YELLOW);
    delay(1000);
}

// 测试 7: 文字渲染
void testText() {
    display.fillScreen(TFT_BLACK);
    display.setTextColor(TFT_WHITE);
    display.setTextDatum(TC_DATUM);

    display.setTextSize(1);
    display.drawString("Size 1: Hello Seeed!", 120, 5);

    display.setTextSize(2);
    display.drawString("Size 2: Hello!", 120, 20);

    display.setTextSize(3);
    display.drawString("Size 3", 120, 50);

    display.setTextSize(4);
    display.drawString("Size 4", 120, 90);

    // 不同颜色
    display.setTextSize(2);
    display.setTextColor(TFT_RED);
    display.drawString("Red Text", 120, 140);
    display.setTextColor(TFT_GREEN);
    display.drawString("Green Text", 120, 160);
    display.setTextColor(TFT_BLUE);
    display.drawString("Blue Text", 120, 180);
    display.setTextColor(TFT_YELLOW);
    display.drawString("Yellow Text", 120, 200);

    display.setTextDatum(TL_DATUM);
    delay(2000);
}

// 测试 8: 像素点阵
void testPixels() {
    display.fillScreen(TFT_BLACK);
    int w = display.width();
    int h = display.height();

    // 随机彩色像素
    for (int i = 0; i < 2000; i++) {
        int x = random(0, w);
        int y = random(0, h);
        uint16_t color = random(0xFFFF);
        display.drawPixel(x, y, color);
    }
    delay(2000);
}

// 主程序
void setup() {
    Serial.begin(115200);
    delay(100);

    Serial.println("=== Seeed_GFX v2.0 - Graphics Test ===");

    // 初始化显示
    display.begin();
    Serial.printf("Display: %dx%d\n", display.width(), display.height());

    // 运行所有测试
    testFillScreen();
    testLines();
    testRects();
    testCircles();
    testTriangles();
    testEllipses();
    testText();
    testPixels();

    // 回到黑色
    display.fillScreen(TFT_BLACK);
    display.setTextColor(TFT_WHITE);
    display.setTextSize(2);
    display.setTextDatum(MC_DATUM);
    display.drawString("Test Complete!", 120, 120);
    display.setTextDatum(TL_DATUM);

    Serial.println("Graphics Test Complete!");
}

void loop() {
    delay(1000);
}
