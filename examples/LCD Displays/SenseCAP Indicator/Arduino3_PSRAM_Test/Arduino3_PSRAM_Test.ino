#include <Arduino.h>
#include <Seeed_GFX.h>

// 创建 SenseCAP Indicator 对象
// 如果是 DX 版本，改成 SenseCAP_Indicator_DX
// SenseCAP Indicator + Arduino 3.x PSRAM overclocking test
// This example demonstrates the SenseCAP Indicator running on the
// Arduino-ESP32 3.x core with Octal PSRAM at 80 MHz.
// Compatible environment (PlatformIO):
//   platform = https://github.com/pioarduino/platform-espressif32.git#55.03.37
//   board_build.arduino.memory_type = qio_opi
//   build_flags =
//       -DBOARD_HAS_PSRAM
//       -DCONFIG_SPIRAM_USE=1
//       -DCONFIG_SPIRAM_SPEED_80M=1

// Create SenseCAP Indicator object.
// Use SenseCAP_Indicator_DX if your unit has the RGB-only DX panel.
Seeed_GFX display(Seeed_Product::SenseCAP_Indicator_GX);

void setup() {
    Serial.begin(115200);
    while (!Serial) { delay(10); }  // 等待 USB 串口连接
    delay(500);

    Serial.println("=== SenseCAP Indicator + Arduino 3.x Test ===");

    if (!display.begin()) {
        Serial.print("display.begin() failed: ");
        Serial.println(display.lastResult().message);
        return;
    }

    Serial.println("Display init OK");

    // 画一个简单的彩色画面
    display.fillScreen(TFT_RED);
    delay(500);
    display.fillScreen(TFT_GREEN);
    delay(500);
    display.fillScreen(TFT_BLUE);
    delay(500);
    display.fillScreen(TFT_BLACK);

    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.setTextDatum(MC_DATUM);
    display.drawString("Hello Indicator", 240, 220, 4);
    display.drawString("Arduino 3.x + PSRAM", 240, 260, 2);

    Serial.println("First frame drawn");
}

void loop() {
    // 每 5 秒切换一次背景色，观察是否闪烁/撕裂
    static uint32_t last = 0;
    static uint8_t c = 0;

    if (millis() - last > 5000) {
        last = millis();
        c = static_cast<uint8_t>((c + 1) % 3);
        switch (c) {
            case 0: display.fillScreen(TFT_BLACK); break;
            case 1: display.fillScreen(TFT_NAVY); break;
            case 2: display.fillScreen(TFT_DARKGREEN); break;
        }
        display.setTextColor(TFT_WHITE);
        display.drawString("Frame updated", 240, 240, 4);
        Serial.printf("Frame updated @ %lu ms\n", millis());
    }

    delay(20);
}
