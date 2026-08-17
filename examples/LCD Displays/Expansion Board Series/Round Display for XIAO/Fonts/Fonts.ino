// Example: Font_Demo.ino
// 字体演示 - 展示 GLCD 字体和 GFX FreeFont
// 默认硬件: Seeed Studio Round Display for XIAO

#include <Seeed_GFX.h>
#include <font/GFXFF/FreeMono12pt7b.h>
#include <font/GFXFF/FreeSans9pt7b.h>

Seeed_GFX display(Seeed_Product::XIAO_ROUND_DISPLAY);

// 演示 1: GLCD 内置字体 (5x7, 缩放)
void demoGLCD() {
    display.fillScreen(TFT_BLACK);
    display.setTextColor(TFT_WHITE);
    display.setTextDatum(MC_DATUM);

    display.setTextSize(1);
    display.drawString("GLCD FONT", 120, 38);

    display.setTextSize(2);
    display.drawString("Size 2", 120, 72);

    display.setTextSize(3);
    display.drawString("Size 3", 120, 112);

    display.setTextSize(4);
    display.drawString("Size 4", 120, 158);

    delay(2200);

    // 所有可打印字符
    display.fillScreen(TFT_BLACK);
    display.setTextSize(1);
    display.setTextColor(TFT_GREEN);
    display.setTextDatum(TC_DATUM);
    char line[33];
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 32; col++) {
            line[col] = (char)(32 + row * 32 + col);
        }
        line[32] = '\0';
        display.drawString(line, 120, 80 + row * 10);
    }
    display.setTextDatum(TL_DATUM);
    delay(2000);
}

// 演示 2: GFX FreeFont (比例字体)
void demoFreeFont() {
    display.fillScreen(TFT_BLACK);
    display.setTextColor(TFT_WHITE);
    display.setTextDatum(MC_DATUM);

    display.setTextSize(2);
    display.drawString("FreeFont", 120, 42);

    display.setTextSize(1);
    display.setFreeFont(&FreeSans9pt7b);
    display.drawString("FreeSans 9pt", 120, 100);
    display.setFreeFont(&FreeMono12pt7b);
    display.drawString("Mono 12pt", 120, 154);

    display.setFreeFont(nullptr);
    display.setTextDatum(TL_DATUM);

    delay(2400);
}

// 演示 3: 文字对齐 (Datum)
void demoTextAlign() {
    display.fillScreen(TFT_BLACK);
    int cx = display.width() / 2;
    int cy = display.height() / 2;

    // 十字参考线
    display.drawFastHLine(0, cy, display.width(), TFT_DARKGREY);
    display.drawFastVLine(cx, 0, display.height(), TFT_DARKGREY);

    display.setTextColor(TFT_WHITE);
    display.setTextSize(1);

    // 左上角对齐
    display.setTextDatum(TL_DATUM);
    display.drawString("TL_DATUM", cx - 60, cy - 10);

    // 右上角对齐
    display.setTextDatum(TR_DATUM);
    display.drawString("TR_DATUM", cx + 60, cy - 10);

    // 左下角对齐
    display.setTextDatum(BL_DATUM);
    display.drawString("BL_DATUM", cx - 60, cy + 10);

    // 右下角对齐
    display.setTextDatum(BR_DATUM);
    display.drawString("BR_DATUM", cx + 60, cy + 10);

    // 中心对齐
    display.setTextDatum(MC_DATUM);
    display.setTextColor(TFT_YELLOW);
    display.drawString("CENTER", cx, cy);

    delay(3000);
    display.setTextDatum(TL_DATUM);
}

// 演示 4: 文字颜色和背景
void demoTextColors() {
    display.fillScreen(TFT_BLACK);

    uint16_t fgColors[] = {TFT_RED, TFT_GREEN, TFT_BLUE, TFT_YELLOW,
                           TFT_CYAN, TFT_MAGENTA, TFT_WHITE};
    uint16_t bgColors[] = {TFT_DARKGREY, TFT_NAVY, TFT_MAROON, TFT_DARKGREEN,
                           TFT_PURPLE, TFT_OLIVE, TFT_BLACK};

    display.setTextSize(2);
    display.setTextDatum(MC_DATUM);
    for (int i = 0; i < 7; i++) {
        int y = 30 + i * 29;
        display.setTextColor(fgColors[i], bgColors[i]);
        display.drawString("Colored Text", 120, y);
    }

    delay(2000);
    display.setTextDatum(TL_DATUM);
}

// 演示 5: 长文本 / 自动换行
void demoLongText() {
    display.fillScreen(TFT_BLACK);
    display.setTextColor(TFT_WHITE);
    display.setTextSize(1);
    display.setTextDatum(TL_DATUM);
    display.setCursor(45, 40);

    display.println("Arduino Print class");
    display.println("compatible!");
    display.println("");
    display.println("Seeed_GFX inherits");
    display.println("from Print, so you");
    display.println("can use println(),");
    display.print("print(), and even");
    display.println(" printf()-style");
    display.println("formatting.");
    display.println("");
    display.print("Numbers: ");
    display.println(12345);
    display.print("Floats: ");
    display.println(3.14159, 4);

    delay(3000);
}

// 主程序
void setup() {
    Serial.begin(115200);

    if (!display.begin()) {
        Serial.println(display.lastResult().message);
        return;
    }
    display.setRotation(0);
    Serial.println("Font Demo Start!");

    demoGLCD();
    demoFreeFont();
    demoTextAlign();
    demoTextColors();
    demoLongText();

    display.fillScreen(TFT_BLACK);
    display.setTextColor(TFT_WHITE);
    display.setTextSize(2);
    display.setTextDatum(MC_DATUM);
    display.drawString("Font Demo", 120, 103);
    display.setTextColor(TFT_GREEN);
    display.drawString("Complete!", 120, 137);
    display.setTextDatum(TL_DATUM);

    Serial.println("Font Demo Complete!");
}

void loop() {
    delay(1000);
}
