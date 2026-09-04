/**
 * Product: XIAO 1.47 inch Touch Display (JD9853A 172x320 + AXS5106L touch)
 * Display: JD9853A 172x320, BGR, capacitive touch
 * Target:  XIAO ESP32-S3 Plus (RST=13, BL=12). For nRF52840 Plus use the sibling
 *          nRF52840 Plus folder (RST=38, BL=37).
 * Wiring:  CS=D2, DC=D3, MOSI=D10, SCLK=D8; RST=13, BL=12.
 * Demo:    Reads battery voltage from D16 (316k/160k divider) and draws the
 *          voltage + mV on the 1.47" LCD with a color-coded value.
 *
 * Ported from XIAO-Display-Board-main (xiao_esp32s3_147_bat, Arduino_GFX). The
 * Seeed_GFX Board/Config templates replace the original bus+panel setup, manual
 * MADCTL/invert/swapbytes. Drop applyXIAO147PanelFix() (Config handles MADCTL);
 * keep analogReadMilliVolts/316k/160k ADC math + colorForVoltage. Map RGB565_*
 * -> TFT_*; the raw 0x8410 color literal stays unchanged. This is the touch
 * board run LCD-only (like LCD_Only_Diagnostic); IRQ unused.
 */

#include <Seeed_GFX.h>
#include "board/boards/XIAO_LCD_Board.h"
#include "driver/tft/Driver_JD9853A.h"
#include "panel/Panel_TFT.h"

Seeed_GFX display;

static constexpr int8_t LCD_RST_PIN = 13;  // XIAO ESP32-S3 Plus
static constexpr int8_t LCD_BL_PIN  = 12;

// Battery ADC: VBAT -- 316k -- ADC(D16) -- 160k -- GND.
// Divider ratio: (316k + 160k) / 160k = 2.975
static constexpr uint8_t BAT_ADC_PIN = D16;
static constexpr float BAT_DIVIDER_RATIO = (316.0f + 160.0f) / 160.0f;
static constexpr uint32_t SAMPLE_INTERVAL_MS = 1000;

static uint32_t lastSampleMs = 0;
static uint32_t lastBatteryMv = 0;

static uint32_t readBatteryMilliVolts() {
  uint32_t sumMv = 0;
  static constexpr uint8_t SAMPLE_COUNT = 16;

  // Average several readings so the serial output is easier to compare.
  for (uint8_t i = 0; i < SAMPLE_COUNT; ++i) {
#if defined(ARDUINO_ARCH_ESP32)
    sumMv += analogReadMilliVolts(BAT_ADC_PIN);
#else
    // Fallback for non-ESP32 compilation tests. ESP32S3 should use the branch above.
    sumMv += (uint32_t)analogRead(BAT_ADC_PIN) * 3300UL / 4095UL;
#endif
    delay(2);
  }

  uint32_t adcMv = sumMv / SAMPLE_COUNT;
  return (uint32_t)((float)adcMv * BAT_DIVIDER_RATIO);
}

static uint16_t colorForVoltage(uint32_t batteryMv) {
  if (batteryMv < 3400) return TFT_RED;
  if (batteryMv < 3700) return TFT_YELLOW;
  return TFT_GREEN;
}

static void drawBatteryVoltage(uint32_t batteryMv) {
  char voltageText[16];
  snprintf(voltageText, sizeof(voltageText), "%.3fV", (float)batteryMv / 1000.0f);

  // Clear only the dynamic area so the static labels do not flicker.
  display.fillRect(0, 90, 172, 118, TFT_BLACK);

  display.setTextSize(3);
  display.setTextColor(colorForVoltage(batteryMv), TFT_BLACK);
  display.setCursor(26, 124);
  display.print(voltageText);

  display.setTextSize(2);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setCursor(38, 184);
  display.print(String(batteryMv) + " mV");
}

static void initDisplay() {
  if (!display.begin<
          Board_XIAO_1inch47_Touch_Display<LCD_RST_PIN, LCD_BL_PIN>,
                     Config_Seeed_1inch47_Touch_JD9853A>()) {
    Serial.println(display.lastResult().message);
    return;
  }

  display.fillScreen(TFT_BLACK);

  display.setTextSize(2);
  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.setCursor(18, 24);
  display.print("ESP32S3 BAT");

  display.drawFastHLine(16, 62, 140, 0x8410);

  display.setTextSize(1);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setCursor(58, 250);
  display.print("D16 ADC");

  display.setTextColor(TFT_YELLOW, TFT_BLACK);
  display.setCursor(28, 276);
  display.print("316k / 160k divider");
}

void setup() {
  Serial.begin(115200);
  delay(800);

#if defined(ARDUINO_ARCH_ESP32)
  analogReadResolution(12);
  // 11 dB attenuation gives the ESP32 ADC its widest input range.
  analogSetPinAttenuation(BAT_ADC_PIN, ADC_11db);
#endif

  Serial.println("=== ESP32S3 Battery basic ===");
  Serial.println("ADC pin: D16");
  Serial.println("Divider: 316k / 160k");

  initDisplay();
  drawBatteryVoltage(0);
}

void loop() {
  uint32_t now = millis();
  if (now - lastSampleMs < SAMPLE_INTERVAL_MS) return;
  lastSampleMs = now;

  uint32_t batteryMv = readBatteryMilliVolts();
  lastBatteryMv = batteryMv;

  Serial.print("battery=");
  Serial.print(batteryMv);
  Serial.print("mV ");
  Serial.print((float)batteryMv / 1000.0f, 3);
  Serial.println("V");

  drawBatteryVoltage(lastBatteryMv);
}
