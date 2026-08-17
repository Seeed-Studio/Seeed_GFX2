/**
 * Product: XIAO 1.47 inch Touch Display (JD9853A 172x320 + AXS5106L touch)
 * Display: JD9853A 172x320, BGR, capacitive touch
 * Target:  XIAO nRF52840 Plus (RST=38, BL=37). For ESP32-S3 Plus use the sibling
 *          ESP32-S3 Plus folder (RST=13, BL=12).
 * Wiring:  CS=D2, DC=D3, MOSI=D10, SCLK=D8; RST=38, BL=37. I2C SDA=D4, SCL=D5
 *          (LSM6DS3 0x6A); IMU wake INT1=D14; USR1=D19, USR2=D15.
 * Demo:    IMU D14 wake / screen-sleep demo: backlight off = sleep; nRF enters System ON
 *          sleep; LSM6DS3 wake-up INT1 on D14 (or USR2=D15) wakes CPU + screen.
 *          Dashboard: power/battery/motion.
 *
 * Ported from XIAO-Display-Board-main (xiao_nrf52840_147_wakeup, TFT_eSPI). The Seeed_GFX
 * Board/Config templates replace the original bus+panel setup, manual MADCTL/invert/
 * swapbytes, and (for TFT_eSPI) the driver.h User_Setup. DROP driver.h + manual init +
 * applyXIAO147PanelFix/invertDisplay. Keep <Adafruit_TinyUSB.h>, LSM6DS3.h, <nrf.h>/
 * <nrf_gpio.h>, NRF_P0 battery divider, __SEV/__WFE System ON sleep, attachInterrupt ISR,
 * analogReadResolution/PIN_VBAT. Backlight PWM via analogWrite(LCD_BL_PIN, pwm) on BL=37.
 */

#include <Seeed_GFX.h>
#include "board/boards/XIAO_LCD_Board.h"
#include "driver/tft/Driver_JD9853A.h"
#include "panel/Panel_TFT.h"
#include <Adafruit_TinyUSB.h>
#include <Wire.h>
#include "SparkFunLSM6DS3.h"   // was "LSM6DS3.h" (Seeed's lib, not installed) — SparkFun's is API-identical, already installed
#include <nrf.h>
#include <nrf_gpio.h>

// ========================= Pins =========================

static constexpr int8_t LCD_RST_PIN = 38;  // XIAO nRF52840 Plus
static constexpr int8_t LCD_BL_PIN  = 37;

static constexpr uint8_t I2C_SDA_PIN  = D4;
static constexpr uint8_t I2C_SCL_PIN  = D5;
static constexpr uint8_t IMU_INT_PIN  = D14;

// Optional manual test buttons.
static constexpr uint8_t USR1_PIN = D19; // force sleep
static constexpr uint8_t USR2_PIN = D15; // force wake

// nRF52840 Plus battery measurement pins.
static constexpr uint8_t READ_BAT_P0_PIN = 14; // P0.14 / READ_BAT, active-low divider enable.
static constexpr uint8_t CHG_P0_PIN = 17;      // P0.17 / CHG, active-low charging status.

#ifndef PIN_VBAT
#define PIN_VBAT 35
#endif

// ========================= LCD =========================

Seeed_GFX display;

// ========================= IMU =========================

LSM6DS3 myIMU(I2C_MODE, 0x6A);

static constexpr uint8_t LSM6DS3_ADDR      = 0x6A;
static constexpr uint8_t REG_WAKE_UP_SRC   = 0x1B;
static constexpr uint8_t REG_CTRL1_XL      = 0x10;
static constexpr uint8_t REG_CTRL3_C       = 0x12;
static constexpr uint8_t REG_TAP_CFG       = 0x58;
static constexpr uint8_t REG_WAKE_UP_THS   = 0x5B;
static constexpr uint8_t REG_WAKE_UP_DUR   = 0x5C;
static constexpr uint8_t REG_MD1_CFG       = 0x5E;

// ========================= UI / timing =========================

static constexpr uint32_t AUTO_SLEEP_MS = 8000;
static constexpr uint32_t UI_REFRESH_MS = 250;
static constexpr uint32_t BAT_REFRESH_MS = 1000;
static constexpr uint32_t WAKE_LOCK_MS  = 1200;
static constexpr uint32_t SLEEP_STATUS_MS = 2000;

// Wake-up sensitivity.
// Lower threshold: easier to wake, but more false triggers.
// Higher threshold: harder to wake.
// Suggested tuning range: 0x03 ~ 0x0A
static constexpr uint8_t IMU_WAKE_THRESHOLD = 0x05;

static constexpr uint8_t BACKLIGHT_AWAKE_PWM = 120;
static constexpr uint8_t BACKLIGHT_SLEEP_PWM = 0;

static constexpr uint16_t C_BLACK  = TFT_BLACK;
static constexpr uint16_t C_WHITE  = TFT_WHITE;
static constexpr uint16_t C_GREEN  = TFT_GREEN;
static constexpr uint16_t C_CYAN   = TFT_CYAN;
static constexpr uint16_t C_YELLOW = TFT_YELLOW;
static constexpr uint16_t C_RED    = TFT_RED;
static constexpr uint16_t C_GRAY   = 0x8410;
static constexpr uint16_t C_LINE   = 0x39E7;

// Battery ADC conversion, aligned with example/basic/xiao_nrf52840_147_bat.
static constexpr int ADC_BITS = 12;
static constexpr int ADC_MAX = (1 << ADC_BITS) - 1;
static constexpr float ADC_FULL_SCALE_V = 3.600f;
static constexpr float BAT_DIVIDER_RATIO = (1000.0f + 510.0f) / 510.0f;

// ========================= Runtime state =========================

volatile bool g_imuWakeFlag = false;
volatile bool g_usrWakeFlag = false;

bool g_screenAwake = true;
uint32_t g_lastActivityMs = 0;
uint32_t g_lastUiMs = 0;
uint32_t g_lastBatMs = 0;
uint32_t g_wakeCount = 0;
uint32_t g_intCount = 0;
uint32_t g_lastWakeMs = 0;
uint32_t g_sleepEnterMs = 0;
uint32_t g_lastSleepStatusMs = 0;
uint32_t g_sleepLoopCount = 0;

float g_ax = 0.0f;
float g_ay = 0.0f;
float g_az = 0.0f;
float g_gx = 0.0f;
float g_gy = 0.0f;
float g_gz = 0.0f;
uint8_t g_lastWakeSrc = 0;

struct BatteryState {
  uint16_t raw = 0;
  float vadc = 0.0f;
  float vbat = 0.0f;
  int percent = 0;
  bool charging = false;
};

BatteryState g_bat;

// ========================= Helpers =========================

static bool imuWriteReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(LSM6DS3_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

static bool imuReadReg(uint8_t reg, uint8_t &val) {
  Wire.beginTransmission(LSM6DS3_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;

  if (Wire.requestFrom((int)LSM6DS3_ADDR, 1) != 1) return false;
  val = Wire.read();
  return true;
}

void imuWakeIsr() {
  g_imuWakeFlag = true;
  g_intCount++;
}

void usrWakeIsr() {
  g_usrWakeFlag = true;
}

// Backlight PWM is driven directly on the BL pin; the Board template sets BL as an
// output during display.begin<>() but does not own the PWM level.
static void setBacklight(uint8_t pwm) {
  pinMode(LCD_BL_PIN, OUTPUT);
  analogWrite(LCD_BL_PIN, pwm);
}

static void enableBatteryDivider() {
  NRF_P0->OUTCLR = (1UL << READ_BAT_P0_PIN);
  NRF_P0->DIRSET = (1UL << READ_BAT_P0_PIN);
}

static void disableBatteryDivider() {
  NRF_P0->DIRCLR = (1UL << READ_BAT_P0_PIN);
}

static int lipoPercent(float voltage) {
  struct BatPoint {
    float v;
    int p;
  };

  static const BatPoint table[] = {
    {4.20f, 100}, {4.10f, 90}, {4.00f, 80}, {3.90f, 70}, {3.80f, 60},
    {3.75f, 50},  {3.70f, 40}, {3.65f, 30}, {3.60f, 20}, {3.45f, 10},
    {3.30f, 0}
  };

  if (voltage >= table[0].v) return 100;
  if (voltage <= table[10].v) return 0;

  for (uint8_t i = 0; i < 10; i++) {
    if (voltage <= table[i].v && voltage >= table[i + 1].v) {
      float span = table[i].v - table[i + 1].v;
      float t = (voltage - table[i + 1].v) / span;
      return table[i + 1].p + (int)(t * (table[i].p - table[i + 1].p) + 0.5f);
    }
  }

  return 0;
}

static uint16_t readBatteryRaw() {
  uint32_t sum = 0;

  for (uint8_t i = 0; i < 6; i++) {
    (void)analogRead(PIN_VBAT);
    delay(2);
  }

  for (uint8_t i = 0; i < 16; i++) {
    sum += analogRead(PIN_VBAT);
    delay(2);
  }

  return (uint16_t)(sum / 16);
}

static void updateBattery() {
  enableBatteryDivider();
  delay(30);

  g_bat.raw = readBatteryRaw();
  disableBatteryDivider();

  g_bat.vadc = ((float)g_bat.raw * ADC_FULL_SCALE_V) / (float)ADC_MAX;
  g_bat.vbat = g_bat.vadc * BAT_DIVIDER_RATIO;
  g_bat.percent = lipoPercent(g_bat.vbat);
  g_bat.charging = (NRF_P0->IN & (1UL << CHG_P0_PIN)) == 0;
}

static uint16_t batteryColor() {
  if (g_bat.percent <= 15) return C_RED;
  if (g_bat.percent <= 35) return C_YELLOW;
  return C_GREEN;
}

static void drawChargeIcon(bool charging) {
  display.fillRect(144, 126, 16, 18, C_BLACK);
  if (!charging) return;

  display.fillTriangle(151, 126, 145, 136, 151, 136, C_YELLOW);
  display.fillTriangle(151, 134, 157, 134, 149, 144, C_YELLOW);
}

// ========================= nRF System ON sleep =========================
// WFE = Wait For Event. In System ON sleep, RAM/state are preserved.
// Any enabled GPIO interrupt, including D14 from IMU, wakes the CPU and loop continues.
// The SEV/WFE/WFE pattern clears any stale event first, then actually sleeps.
// This avoids immediately falling through because of a previous event flag.
static void systemOnSleepOnce() {
#if defined(NRF52840_XXAA) || defined(NRF52_SERIES) || defined(ARDUINO_ARCH_NRF52)
  __SEV();
  __WFE();
  __WFE();
#else
  delay(10);
#endif
}

static void enterSystemOnSleepLoop() {
  // Keep Serial alive for debug, but note USB itself will dominate current if connected.
  g_sleepLoopCount++;

  // Avoid spamming serial while asleep.
  uint32_t now = millis();
  if (now - g_lastSleepStatusMs >= SLEEP_STATUS_MS) {
    g_lastSleepStatusMs = now;
    Serial.print("[SYS_ON_SLEEP] waiting, sleepLoops=");
    Serial.print((unsigned long)g_sleepLoopCount);
    Serial.print(" D14=");
    Serial.print(digitalRead(IMU_INT_PIN));
    Serial.print(" awake=");
    Serial.println(g_screenAwake ? "Y" : "N");
  }

  // Enter nRF System ON sleep until an interrupt/event arrives.
  systemOnSleepOnce();
}

// In-place fixed-width text: padded with spaces so each redraw clears the prior value.
// bgfill=true is required so the padding spaces paint the black background over stale
// digits (Seeed_GFX setTextColor(fg,bg) defaults to transparent, unlike TFT_eSPI).
static void printFixed(int x, int y, uint16_t color, const String &text, int width) {
  String out = text;
  while ((int)out.length() < width) out += ' ';
  if ((int)out.length() > width) out = out.substring(0, width);

  display.setTextSize(1);
  display.setTextColor(color, C_BLACK, true);
  display.setCursor(x, y);
  display.print(out);
}

// ========================= LCD UI =========================

static bool initLcd() {
  if (!display.begin<Board_XIAO_1inch47_Touch_Display<LCD_RST_PIN, LCD_BL_PIN>, Config_XIAO_1inch47_Touch_JD9853A>()) {
    Serial.println(display.lastResult().message);
    return false;
  }
  setBacklight(BACKLIGHT_AWAKE_PWM);
  display.fillScreen(C_BLACK);
  return true;
}

static void drawAwakeLayout() {
  display.fillScreen(C_BLACK);

  display.setTextSize(2);
  display.setTextColor(C_GREEN, C_BLACK);
  display.setCursor(8, 10);
  display.print("Hello,XIAO!");

  display.setTextSize(1);
  display.setTextColor(C_CYAN, C_BLACK);
  display.setCursor(10, 36);
  display.print("IMU D14 Wake Demo");

  display.drawFastHLine(8, 52, 156, C_LINE);

  display.drawRoundRect(8, 66, 156, 82, 6, C_CYAN);
  display.setTextColor(C_CYAN, C_BLACK);
  display.setCursor(18, 78);
  display.print("POWER STATE");
  display.setTextColor(C_WHITE, C_BLACK);
  display.setCursor(18, 98);
  display.print("Screen");
  display.setCursor(18, 116);
  display.print("WakeCnt");
  display.setCursor(18, 134);
  display.print("BAT");

  display.drawRoundRect(8, 156, 156, 90, 6, C_YELLOW);
  display.setTextColor(C_YELLOW, C_BLACK);
  display.setCursor(18, 168);
  display.print("MOTION");
  display.setTextColor(C_WHITE, C_BLACK);
  display.setCursor(18, 190);
  display.print("Acc");
  display.setCursor(18, 210);
  display.print("Gyr");
  display.setCursor(18, 230);
  display.print("INT");

  display.drawRoundRect(8, 258, 156, 42, 6, C_GREEN);
  display.setTextColor(C_GREEN, C_BLACK);
  display.setCursor(18, 270);
  display.print("TEST");
  display.setTextColor(C_WHITE, C_BLACK);
  display.setCursor(18, 288);
  display.print("USR1 sleep  USR2 wake");
}

static void updateAwakeUi() {
  char buf[48];

  printFixed(78, 98, C_GREEN, "AWAKE", 12);

  snprintf(buf, sizeof(buf), "%lu", (unsigned long)g_wakeCount);
  printFixed(78, 116, C_YELLOW, buf, 12);

  snprintf(buf, sizeof(buf), "%.2fV %d%%", g_bat.vbat, g_bat.percent);
  printFixed(48, 134, batteryColor(), buf, 12);
  drawChargeIcon(g_bat.charging);

  snprintf(buf, sizeof(buf), "%.1f %.1f %.1f", g_ax, g_ay, g_az);
  printFixed(48, 190, C_WHITE, buf, 16);

  snprintf(buf, sizeof(buf), "%.1f %.1f %.1f", g_gx, g_gy, g_gz);
  printFixed(48, 210, C_WHITE, buf, 16);

  snprintf(buf, sizeof(buf), "D14:%lu src:0x%02X", (unsigned long)g_intCount, g_lastWakeSrc);
  printFixed(48, 230, C_CYAN, buf, 16);

  uint32_t remain = 0;
  uint32_t now = millis();
  if (now - g_lastActivityMs < AUTO_SLEEP_MS) {
    remain = (AUTO_SLEEP_MS - (now - g_lastActivityMs)) / 1000;
  }
  snprintf(buf, sizeof(buf), "Auto sleep in %lus", (unsigned long)remain);
  printFixed(18, 306, C_GRAY, buf, 20);
}

static void screenWake(const char *reason) {
  if (g_screenAwake && (millis() - g_lastWakeMs < WAKE_LOCK_MS)) return;

  g_screenAwake = true;
  g_lastActivityMs = millis();
  g_lastWakeMs = millis();
  g_wakeCount++;

  setBacklight(BACKLIGHT_AWAKE_PWM);
  drawAwakeLayout();
  updateAwakeUi();

  Serial.print("[WAKE] reason=");
  Serial.print(reason);
  Serial.print(" wakeCount=");
  Serial.print(g_wakeCount);
  Serial.print(" sleptMs=");
  Serial.print((unsigned long)(millis() - g_sleepEnterMs));
  Serial.print(" sleepLoops=");
  Serial.println((unsigned long)g_sleepLoopCount);
}

static void screenSleep() {
  if (!g_screenAwake) return;

  Serial.println("[SLEEP] screen backlight off, waiting for IMU D14 wake");

  display.fillScreen(C_BLACK);
  display.setTextSize(2);
  display.setTextColor(C_CYAN, C_BLACK);
  display.setCursor(18, 120);
  display.print("Sleeping...");
  display.setTextSize(1);
  display.setCursor(18, 150);
  display.print("Pick up device to wake");
  delay(500);

  setBacklight(BACKLIGHT_SLEEP_PWM);
  g_screenAwake = false;
  g_sleepEnterMs = millis();
  g_lastSleepStatusMs = 0;
  g_sleepLoopCount = 0;

  // Clear stale GPIO/source states before entering WFE loop.
  uint8_t dummy = 0;
  imuReadReg(REG_WAKE_UP_SRC, dummy);
  g_imuWakeFlag = false;
  g_usrWakeFlag = false;
}

// ========================= IMU wake config =========================

static bool initImuWakeInterrupt() {
  Wire.begin();

  int imuOk = myIMU.begin();
  Serial.print("[IMU] Seeed LSM6DS3 begin=");
  Serial.println(imuOk);

  bool ok = true;

  // BDU=1 and register auto-increment enabled.
  ok &= imuWriteReg(REG_CTRL3_C, 0x44);

  // Accelerometer: 104Hz, +/-2g.
  // Wake-up event uses accelerometer.
  ok &= imuWriteReg(REG_CTRL1_XL, 0x40);

  // Enable embedded interrupts.
  ok &= imuWriteReg(REG_TAP_CFG, 0x80);

  // Wake-up threshold.
  // 0x05 is a medium-low threshold for "pick up / move device".
  ok &= imuWriteReg(REG_WAKE_UP_THS, IMU_WAKE_THRESHOLD);

  // No extra wake duration, responsive wake.
  ok &= imuWriteReg(REG_WAKE_UP_DUR, 0x00);

  // Route wake-up interrupt to INT1.
  // MD1_CFG bit5 = INT1_WU.
  ok &= imuWriteReg(REG_MD1_CFG, 0x20);

  uint8_t dummy = 0;
  imuReadReg(REG_WAKE_UP_SRC, dummy); // clear stale event.

  pinMode(IMU_INT_PIN, INPUT_PULLDOWN);
  attachInterrupt(digitalPinToInterrupt(IMU_INT_PIN), imuWakeIsr, RISING);

  Serial.print("[IMU] D14 wake interrupt ");
  Serial.println(ok ? "OK" : "FAILED");
  Serial.print("[IMU] wake threshold=0x");
  Serial.println(IMU_WAKE_THRESHOLD, HEX);

  return ok;
}

static void updateImuData() {
  g_ax = myIMU.readFloatAccelX();
  g_ay = myIMU.readFloatAccelY();
  g_az = myIMU.readFloatAccelZ();
  g_gx = myIMU.readFloatGyroX();
  g_gy = myIMU.readFloatGyroY();
  g_gz = myIMU.readFloatGyroZ();
}

static void handleWakeEvents() {
  bool imuPending = false;
  bool usrPending = false;

  noInterrupts();
  if (g_imuWakeFlag) {
    g_imuWakeFlag = false;
    imuPending = true;
  }
  if (g_usrWakeFlag) {
    g_usrWakeFlag = false;
    usrPending = true;
  }
  interrupts();

  if (usrPending) {
    screenWake("USR2_INT");
    return;
  }

  // Polling fallback: if the edge is missed but INT is held high briefly, still wake.
  if (digitalRead(IMU_INT_PIN) == HIGH) {
    imuPending = true;
  }

  if (!imuPending) return;

  uint8_t src = 0;
  if (imuReadReg(REG_WAKE_UP_SRC, src)) {
    g_lastWakeSrc = src;
  }

  // WAKE_UP_SRC bit3 WU_IA indicates wake-up event latched.
  // But some boards may only expose axis bits, so wake on any non-zero source too.
  if ((src & 0x08) || (src & 0x07) || !g_screenAwake) {
    screenWake("IMU_D14");
  }
}

// ========================= Setup / loop =========================

void setup() {
  Serial.begin(115200);
  delay(800);

  pinMode(USR1_PIN, INPUT_PULLUP);
  pinMode(USR2_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(USR2_PIN), usrWakeIsr, FALLING);

  analogReadResolution(ADC_BITS);
  nrf_gpio_cfg_input(CHG_P0_PIN, NRF_GPIO_PIN_PULLUP);
  disableBatteryDivider();

  Serial.println();
  Serial.println("=== XIAO nRF52840 Plus 1.47 IMU D14 Wake Demo v0.2 ===");
  Serial.println("Screen sleeps by backlight off, then nRF enters System ON sleep.");
  Serial.println("D14 IMU wake-up interrupt wakes CPU and turns screen on.");

  initImuWakeInterrupt();

  if (!initLcd()) {
    Serial.println("[FAIL] LCD init failed");
    return;
  }

  updateBattery();
  g_lastBatMs = millis();
  g_lastActivityMs = millis();
  drawAwakeLayout();
  updateAwakeUi();
}

void loop() {
  handleWakeEvents();

  if (!g_screenAwake) {
    // In sleep state, do not refresh UI or poll sensors.
    // CPU sleeps here and wakes on IMU D14 or USR2 interrupt.
    enterSystemOnSleepLoop();
    handleWakeEvents();
    delay(1);
    return;
  }

  // Manual test while awake:
  // USR1: force sleep.
  // USR2: force wake.
  if (digitalRead(USR1_PIN) == LOW) {
    delay(30);
    if (digitalRead(USR1_PIN) == LOW) {
      screenSleep();
      while (digitalRead(USR1_PIN) == LOW) delay(5);
    }
  }

  if (digitalRead(USR2_PIN) == LOW) {
    delay(30);
    if (digitalRead(USR2_PIN) == LOW) {
      screenWake("USR2");
      while (digitalRead(USR2_PIN) == LOW) delay(5);
    }
  }

  uint32_t now = millis();

  if (g_screenAwake) {
    if (now - g_lastBatMs >= BAT_REFRESH_MS) {
      g_lastBatMs = now;
      updateBattery();
    }

    if (now - g_lastUiMs >= UI_REFRESH_MS) {
      g_lastUiMs = now;
      updateImuData();
      updateAwakeUi();
    }

    if (now - g_lastActivityMs >= AUTO_SLEEP_MS) {
      screenSleep();
    }
  }

  delay(5);
}
