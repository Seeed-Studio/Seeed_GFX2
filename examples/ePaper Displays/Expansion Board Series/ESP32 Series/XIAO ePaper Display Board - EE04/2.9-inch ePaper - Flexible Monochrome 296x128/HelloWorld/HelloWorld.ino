/**
 * Product: ePaper Driver Board for Seeed Studio XIAO (EE04)
 * Panel:   Good Display GDEW029I6FD — 2.9" flexible monochrome ePaper,
 *          128x296 native (296x128 landscape), driver IC UC8151D (UltraChip)
 * SKU:     104990852
 * Wiki:    https://wiki.seeedstudio.com/xiao_eink_expansion_board_v2/
 *
 * This example selects the panel via the Seeed_Product::Seeed_ePaper_2INCH9_FLEX
 * product enum; the product catalog maps it to Driver_UC8151D + Panel_EPaper.
 * The UC8151D init follows the GDEW029I6FD reference program "4.2-2) BW mode &
 * LUT from OTP" (PSR bit7=0 -> factory OTP LUT, no LUT register writes).
 *
 * The 2.9" flexible glass is physically landscape (296x128). The UC8151D
 * controller RAM is natively portrait (128x296, 296 gate lines), so we rotate
 * the frame buffer into a 296x128 landscape viewport with setRotation(3).
 */
#include <Seeed_GFX.h>

Seeed_GFX display(Seeed_Product::Seeed_ePaper_2INCH9_FLEX);

void setup() {
    Serial.begin(115200);
    delay(500);  // Give USB CDC time to enumerate so the host sees our prints.
    Serial.println("[DBG] before begin");
    if (!display.begin()) {
        Serial.println(display.lastResult().message);
        return;
    }
    Serial.printf("[DBG] begin OK w=%d h=%d\n", (int)display.width(), (int)display.height());

    // Rotate the portrait controller buffer into 296x128 landscape so text
    // reads along the long edge of the flexible glass.
    display.setRotation(3);
    Serial.printf("[DBG] after rot3 w=%d h=%d\n", (int)display.width(), (int)display.height());

    display.fillScreen(TFT_WHITE);

    const int16_t w = display.width();   // 296
    const int16_t h = display.height();  // 128

    // --- Outer frame ---
    display.drawRect(2, 2, w - 4, h - 4, TFT_BLACK);

    // --- Title row ---
    display.setTextColor(TFT_BLACK);
    display.setTextSize(2);
    display.drawString("2.9 inch Flex BW", 10, 6);
    display.setTextSize(1);
    display.drawRightString("UC8151D", w - 8, 10, 1);

    // --- Divider under the title ---
    display.drawLine(8, 28, w - 8, 28, TFT_BLACK);

    // --- Shape sampler ---
    display.drawCircle(30, 60, 14, TFT_BLACK);
    display.fillCircle(30, 60, 6, TFT_BLACK);
    display.drawRect(58, 46, 44, 28, TFT_BLACK);
    display.fillRect(70, 54, 20, 12, TFT_BLACK);
    for (int i = 0; i < 5; i++)
        display.drawLine(118 + i * 6, 46, 142 + i * 6, 74, TFT_BLACK);
    display.drawTriangle(180, 74, 200, 46, 220, 74, TFT_BLACK);
    for (int i = 0; i < 24; i += 4)
        display.drawLine(232 + i, 46, 232, 46 + i, TFT_BLACK);

    // --- Hello-world banner ---
    display.fillRect(8, 92, w - 16, 26, TFT_BLACK);
    display.setTextColor(TFT_WHITE);
    display.setTextSize(2);
    display.drawCentreString("Hello World", w / 2, 97, 1);

    display.update();
}

void loop() { delay(1000); }
