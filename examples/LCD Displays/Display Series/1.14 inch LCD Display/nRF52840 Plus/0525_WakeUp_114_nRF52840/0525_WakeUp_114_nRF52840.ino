/**
 * Product: XIAO 1.14 inch LCD Board (ST7789 135x240 IPS, no touch)
 * Display: ST7789 135x240, RGB, no touch
 * Target:  XIAO nRF52840 Plus (RST=38, BL=37). For ESP32-S3 Plus use the sibling
 *          ESP32-S3 Plus folder (RST=13, BL=12).
 * Wiring:  CS=D2, DC=D3, MOSI=D10, SCLK=D8; RST=38, BL=37. I2C (LSM6DS3) SDA=D4, SCL=D5;
 *          IMU wake INT1 = D14; USR1=D6, USR2=D7, USR3=D19.
 * Demo:    IMU motion-wake + nRF System ON sleep for the 1.14 board: screen sleeps, LSM6DS3
 *          wake IRQ on D14 wakes and repaints. USR1 sleep / USR2 wake / USR3 reset.
 *
 * Ported from XIAO-Display-Board-main (0525_WakeUp_114_nRF52840, Arduino_GFX). The Seeed_GFX
 * Board/Config templates replace the original Arduino_SWSPI bus + Arduino_ST7789 panel setup
 * (and the manual RST/BL init). Keep LSM6DS3 wake-register config, __SEV()/__WFE() System ON
 * sleep, and the GPIO ISR under #if defined(ARDUINO_ARCH_NRF52). Runtime backlight on/off
 * stays as direct digitalWrite on BL=37 (the Board template drives BL only during begin<>).
 */

#include <Seeed_GFX.h>
#include "board/boards/XIAO_LCD_Board.h"
#include "driver/tft/Driver_ST7789.h"
#include "panel/Panel_TFT.h"
#include <Arduino.h>
#include <Wire.h>
#include "SparkFunLSM6DS3.h"
#include <nrf.h>

Seeed_GFX display;

static constexpr int8_t LCD_RST_PIN = 38;  // XIAO nRF52840 Plus
static constexpr int8_t LCD_BL_PIN  = 37;

// ========================= Pins =========================
// LCD CS/DC/MOSI/SCLK and RST/BL are driven by the Board_XIAO_1inch14_LCD template:
// CS=D2, DC=D3, MOSI=D10, SCLK=D8, RST=38, BL=37.

static constexpr uint8_t I2C_SDA_PIN  = D4;
static constexpr uint8_t I2C_SCL_PIN  = D5;
static constexpr uint8_t USR1_PIN     = D6;   // force sleep
static constexpr uint8_t USR2_PIN     = D7;   // force wake
static constexpr uint8_t IMU_INT_PIN  = D14;
static constexpr uint8_t USR3_PIN     = D19;  // reset counters

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

// Wake sensitivity.
// Lower = easier to wake; higher = harder.
// Suggested range: 0x03 ~ 0x0A.
static constexpr uint8_t IMU_WAKE_THRESHOLD = 0x05;

// ========================= Timing =========================

static constexpr uint32_t AUTO_SLEEP_MS     = 8000;
static constexpr uint32_t UI_REFRESH_MS     = 250;
static constexpr uint32_t WAKE_LOCK_MS      = 1200;
static constexpr uint32_t SLEEP_STATUS_MS   = 2000;
static constexpr uint32_t BTN_DEBOUNCE_MS   = 35;

// ========================= Colors =========================
// Source used RGB565_*; Seeed_GFX exposes TFT_*. Seeed_GFX has no TFT_LIGHTGREEN,
// so RGB565_LIGHTGREEN maps to TFT_GREEN. Raw 0xRRGB literals stay unchanged.

static constexpr uint16_t C_BLACK  = TFT_BLACK;
static constexpr uint16_t C_WHITE  = TFT_WHITE;
static constexpr uint16_t C_GREEN  = TFT_GREEN;
static constexpr uint16_t C_CYAN   = TFT_CYAN;
static constexpr uint16_t C_YELLOW = TFT_YELLOW;
static constexpr uint16_t C_RED    = TFT_RED;
static constexpr uint16_t C_BLUE   = TFT_BLUE;
static constexpr uint16_t C_GRAY   = 0x8410;
static constexpr uint16_t C_LINE   = 0x39E7;

// ========================= Runtime =========================

volatile bool g_imuWakeFlag = false;
volatile bool g_usrWakeFlag = false;

bool g_lcdOk = false;
bool g_screenAwake = true;

uint32_t g_lastActivityMs = 0;
uint32_t g_lastUiMs = 0;
uint32_t g_lastWakeMs = 0;
uint32_t g_sleepEnterMs = 0;
uint32_t g_lastSleepStatusMs = 0;
uint32_t g_sleepLoopCount = 0;

uint32_t g_wakeCount = 0;
uint32_t g_imuIntCount = 0;
uint32_t g_usrWakeCount = 0;

float g_ax = 0.0f;
float g_ay = 0.0f;
float g_az = 0.0f;
float g_gx = 0.0f;
float g_gy = 0.0f;
float g_gz = 0.0f;

uint8_t g_lastWakeSrc = 0;

// ========================= Low-level helpers =========================

static void backlightOn() {
  pinMode(LCD_BL_PIN, OUTPUT);
  digitalWrite(LCD_BL_PIN, HIGH);
}

static void backlightOff() {
  pinMode(LCD_BL_PIN, OUTPUT);
  digitalWrite(LCD_BL_PIN, LOW);
}

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

static void printFixed(int x, int y, uint16_t color, const String &text, int width) {
  if (!g_lcdOk) return;

  String out = text;
  while ((int)out.length() < width) out += ' ';
  if ((int)out.length() > width) out = out.substring(0, width);

  display.setTextSize(1);
  display.setTextColor(color, C_BLACK);
  display.setCursor(x, y);
  display.print(out);
}

// ========================= Interrupts =========================

void imuWakeIsr() {
  g_imuWakeFlag = true;
  g_imuIntCount++;
}

void usrWakeIsr() {
  g_usrWakeFlag = true;
  g_usrWakeCount++;
}

// ========================= nRF System ON sleep =========================
// WFE = Wait For Event. In System ON sleep, RAM/state are preserved.
// GPIO interrupt on D14 wakes CPU and loop continues.
// SEV/WFE/WFE clears stale events first, then sleeps.

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
  g_sleepLoopCount++;

  uint32_t now = millis();
  if (now - g_lastSleepStatusMs >= SLEEP_STATUS_MS) {
    g_lastSleepStatusMs = now;
    Serial.print("[SYS_ON_SLEEP] waiting loops=");
    Serial.print((unsigned long)g_sleepLoopCount);
    Serial.print(" D14=");
    Serial.print(digitalRead(IMU_INT_PIN));
    Serial.print(" screen=");
    Serial.println(g_screenAwake ? "AWAKE" : "SLEEP");
  }

  systemOnSleepOnce();
}

// ========================= LCD UI =========================

static bool initLcd() {
  // The Board template drives RST + BL during begin<>(); BL is left ON here.
  if (!display.begin<Board_XIAO_1inch14_LCD<LCD_RST_PIN, LCD_BL_PIN>,
                     Config_Seeed_1inch14_LCD_ST7789>()) {
    g_lcdOk = false;
    Serial.println("[LCD] begin failed");
    Serial.println(display.lastResult().message);
    return false;
  }

  display.fillScreen(C_BLACK);
  display.setTextWrap(false);
  g_lcdOk = true;
  Serial.println("[LCD] OK 1.14 ST7789 135x240 RST=38 BL=37");
  return true;
}

static void drawCard(int x, int y, int w, int h, uint16_t c, const char *title) {
  display.drawRoundRect(x, y, w, h, 5, c);
  display.drawRoundRect(x + 1, y + 1, w - 2, h - 2, 5, C_LINE);
  display.fillRect(x + 4, y + 9, 3, h - 18, c);
  display.setTextSize(1);
  display.setTextColor(c, C_BLACK);
  display.setCursor(x + 13, y + 8);
  display.print(title);
}

static void drawAwakeLayout() {
  if (!g_lcdOk) return;

  display.fillScreen(C_BLACK);

  display.setTextSize(2);
  display.setTextColor(C_GREEN, C_BLACK);
  display.setCursor(1, 2);
  display.print("Hello,XIAO!");

  display.setTextSize(1);
  display.setTextColor(C_CYAN, C_BLACK);
  display.setCursor(6, 22);
  display.print("1.14 IMU D14 Wake");

  display.drawFastHLine(6, 34, 123, C_LINE);

  drawCard(4, 42, 127, 48, C_CYAN, "STATE");
  display.setTextColor(C_WHITE, C_BLACK);
  display.setCursor(17, 61);
  display.print("Screen");
  display.setCursor(17, 76);
  display.print("Wake");

  drawCard(4, 96, 127, 62, C_YELLOW, "MOTION");
  display.setTextColor(C_WHITE, C_BLACK);
  display.setCursor(17, 116);
  display.print("A");
  display.setCursor(17, 132);
  display.print("G");
  display.setCursor(17, 146);
  display.print("INT");

  drawCard(4, 166, 127, 41, C_BLUE, "TEST");
  display.setTextColor(C_WHITE, C_BLACK);
  display.setCursor(17, 186);
  display.print("U1 Sleep  U2 Wake");

  display.setTextColor(C_GRAY, C_BLACK);
  display.setCursor(19, 222);
  display.print("pick up device to wake");
}

static void updateAwakeUi() {
  if (!g_lcdOk || !g_screenAwake) return;

  char buf[48];

  printFixed(67, 61, C_GREEN, "AWAKE", 10);

  snprintf(buf, sizeof(buf), "%lu", (unsigned long)g_wakeCount);
  printFixed(67, 76, C_YELLOW, buf, 10);

  snprintf(buf, sizeof(buf), "%+.1f %+.1f %+.1f", g_ax, g_ay, g_az);
  printFixed(34, 116, C_WHITE, buf, 15);

  snprintf(buf, sizeof(buf), "%+.0f %+.0f %+.0f", g_gx, g_gy, g_gz);
  printFixed(34, 132, C_WHITE, buf, 15);

  snprintf(buf, sizeof(buf), "%lu 0x%02X", (unsigned long)g_imuIntCount, g_lastWakeSrc);
  printFixed(34, 146, C_CYAN, buf, 15);

  uint32_t now = millis();
  uint32_t remain = 0;
  if (now - g_lastActivityMs < AUTO_SLEEP_MS) {
    remain = (AUTO_SLEEP_MS - (now - g_lastActivityMs)) / 1000;
  }

  snprintf(buf, sizeof(buf), "auto sleep %lus", (unsigned long)remain);
  printFixed(17, 201, C_GRAY, buf, 18);
}

static void drawSleepHintThenOff() {
  if (!g_lcdOk) return;

  display.fillScreen(C_BLACK);

  display.setTextSize(2);
  display.setTextColor(C_CYAN, C_BLACK);
  display.setCursor(12, 70);
  display.print("Sleeping");

  display.setTextSize(1);
  display.setTextColor(C_WHITE, C_BLACK);
  display.setCursor(10, 104);
  display.print("System ON sleep");

  display.setTextColor(C_YELLOW, C_BLACK);
  display.setCursor(10, 122);
  display.print("Move / pick up to wake");

  display.setTextColor(C_GRAY, C_BLACK);
  display.setCursor(10, 150);
  display.print("wake source: IMU D14");
}

static void screenWake(const char *reason) {
  if (g_screenAwake && (millis() - g_lastWakeMs < WAKE_LOCK_MS)) return;

  g_screenAwake = true;
  g_lastActivityMs = millis();
  g_lastWakeMs = millis();
  g_wakeCount++;

  backlightOn();

  // Repaint after waking. It is intentionally a full repaint to avoid any
  // stale pixels after screen-off.
  drawAwakeLayout();
  updateAwakeUi();

  Serial.print("[WAKE] reason=");
  Serial.print(reason);
  Serial.print(" wakeCount=");
  Serial.print((unsigned long)g_wakeCount);
  Serial.print(" sleptMs=");
  Serial.print((unsigned long)(millis() - g_sleepEnterMs));
  Serial.print(" sleepLoops=");
  Serial.print((unsigned long)g_sleepLoopCount);
  Serial.print(" wakeSrc=0x");
  Serial.println(g_lastWakeSrc, HEX);
}

static void screenSleep() {
  if (!g_screenAwake) return;

  Serial.println("[SLEEP] screen off, entering System ON sleep; wake source IMU D14");

  drawSleepHintThenOff();
  delay(450);

  // Clear stale IMU source before sleeping.
  uint8_t dummy = 0;
  imuReadReg(REG_WAKE_UP_SRC, dummy);

  noInterrupts();
  g_imuWakeFlag = false;
  g_usrWakeFlag = false;
  interrupts();

  backlightOff();

  g_screenAwake = false;
  g_sleepEnterMs = millis();
  g_lastSleepStatusMs = 0;
  g_sleepLoopCount = 0;
}

// ========================= IMU =========================

static bool initImuWakeInterrupt() {
  Wire.begin();

  int imuOk = myIMU.begin();
  Serial.print("[IMU] SparkFun begin=");
  Serial.println(imuOk);

  bool ok = true;

  // BDU=1 and register auto-increment enabled.
  ok &= imuWriteReg(REG_CTRL3_C, 0x44);

  // Accelerometer: 104Hz, +/-2g.
  ok &= imuWriteReg(REG_CTRL1_XL, 0x40);

  // Enable embedded interrupts.
  ok &= imuWriteReg(REG_TAP_CFG, 0x80);

  // Wake threshold and duration.
  ok &= imuWriteReg(REG_WAKE_UP_THS, IMU_WAKE_THRESHOLD);
  ok &= imuWriteReg(REG_WAKE_UP_DUR, 0x00);

  // Route wake-up interrupt to INT1.
  // MD1_CFG bit5 = INT1_WU.
  ok &= imuWriteReg(REG_MD1_CFG, 0x20);

  uint8_t dummy = 0;
  imuReadReg(REG_WAKE_UP_SRC, dummy);

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
    screenWake("USR2");
    return;
  }

  // Fallback: if interrupt edge is missed but INT remains high briefly, still wake.
  if (digitalRead(IMU_INT_PIN) == HIGH) {
    imuPending = true;
  }

  if (!imuPending) return;

  uint8_t src = 0;
  if (imuReadReg(REG_WAKE_UP_SRC, src)) {
    g_lastWakeSrc = src;
  }

  // WAKE_UP_SRC bit3 WU_IA indicates wake-up event latched.
  // Some boards also expose axis bits, so wake on any non-zero source too.
  if ((src & 0x08) || (src & 0x07) || !g_screenAwake) {
    screenWake("IMU_D14");
  }
}

static void resetCounters() {
  g_wakeCount = 0;
  g_imuIntCount = 0;
  g_usrWakeCount = 0;
  g_lastWakeSrc = 0;
  g_lastActivityMs = millis();

  Serial.println("[BTN] USR3 reset counters");

  if (g_screenAwake) {
    drawAwakeLayout();
    updateAwakeUi();
  }
}

// ========================= Setup / loop =========================

void setup() {
  Serial.begin(115200);
  delay(800);

  pinMode(USR1_PIN, INPUT_PULLUP);
  pinMode(USR2_PIN, INPUT_PULLUP);
  pinMode(USR3_PIN, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(USR2_PIN), usrWakeIsr, FALLING);

  Serial.println();
  Serial.println("=== XIAO nRF52840 Plus 1.14 IMU D14 System ON Wake Test v0.1 ===");
  Serial.println("[TEST] Device sleeps -> pick up/move -> IMU D14 wakes screen");
  Serial.println("[BTN] USR1 force sleep, USR2 force wake, USR3 reset counters");
  Serial.println("[NOTE] Use battery power for current measurement; USB current is dominated by USB.");

  initImuWakeInterrupt();

  if (!initLcd()) {
    Serial.println("[FAIL] LCD init failed");
    return;
  }

  g_lastActivityMs = millis();

  drawAwakeLayout();
  updateImuData();
  updateAwakeUi();
}

void loop() {
  handleWakeEvents();

  if (!g_screenAwake) {
    // Sleep state: no UI refresh, no sensor polling.
    // CPU enters nRF System ON sleep and wakes on D14 / USR2 GPIO event.
    enterSystemOnSleepLoop();
    handleWakeEvents();
    delay(1);
    return;
  }

  // Manual force sleep.
  if (digitalRead(USR1_PIN) == LOW) {
    delay(BTN_DEBOUNCE_MS);
    if (digitalRead(USR1_PIN) == LOW) {
      while (digitalRead(USR1_PIN) == LOW) delay(5);
      screenSleep();
      return;
    }
  }

  // Manual wake keeps screen active.
  if (digitalRead(USR2_PIN) == LOW) {
    delay(BTN_DEBOUNCE_MS);
    if (digitalRead(USR2_PIN) == LOW) {
      while (digitalRead(USR2_PIN) == LOW) delay(5);
      screenWake("USR2");
    }
  }

  // Reset counters.
  if (digitalRead(USR3_PIN) == LOW) {
    delay(BTN_DEBOUNCE_MS);
    if (digitalRead(USR3_PIN) == LOW) {
      while (digitalRead(USR3_PIN) == LOW) delay(5);
      resetCounters();
    }
  }

  uint32_t now = millis();

  if (now - g_lastUiMs >= UI_REFRESH_MS) {
    g_lastUiMs = now;
    updateImuData();
    updateAwakeUi();
  }

  if (now - g_lastActivityMs >= AUTO_SLEEP_MS) {
    screenSleep();
  }

  delay(5);
}
