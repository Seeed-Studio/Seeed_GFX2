/**
 * Product: XIAO 0.96 inch LCD Board (ST7789 80x160 IPS, no touch)
 * Display: ST7789 80x160, BGR, rotation 2, no touch
 * Target:  XIAO nRF52840 Plus (RST=38, BL=37). For ESP32-S3 Plus use the sibling
 *           ESP32-S3 Plus folder (RST=13, BL=12).
 * Wiring:  CS=D2, DC=D3, MOSI=D10, SCLK=D8; RST=38, BL=37.
 * Demo:    IMU motion-wake + nRF System ON sleep: screen sleeps, LSM6DS3 wake
 *          IRQ on D9 wakes the MCU and redraws. USR1 forces sleep, USR2 wake/reset.
 *
 * Ported from XIAO-Display-Board-main (0526_WakeUp_096_nRF52840, Arduino_GFX). The
 * Seeed_GFX Board/Config templates replace the original Arduino_SWSPI+Arduino_ST7789
 * bus/panel setup and the manual MADCTL/invert/swapbytes. Dropped
 * invertDisplay(true) and the V_RED/V_BLUE/V_YELLOW/V_CYAN color-swap aliases:
 * Config_Seeed_0inch96_LCD_ST7789 is BGR/rot2/invert=false and already produces
 * correct colors. Kept LSM6DS3 wake-register config (WAKE_UP_THS/MD1_CFG),
 * __SEV()/__WFE() System ON sleep, and the GPIO ISRs/attachInterrupt.
 */

#include <Seeed_GFX.h>
#include "board/boards/XIAO_LCD_Board.h"
#include "driver/tft/Driver_ST7789.h"
#include "panel/Panel_TFT.h"
#include <Wire.h>
#include "SparkFunLSM6DS3.h"
#include <nrf.h>

Seeed_GFX display;

static constexpr int8_t LCD_RST_PIN = 38;  // XIAO nRF52840 Plus
static constexpr int8_t LCD_BL_PIN  = 37;

// ========================= Pins =========================

static constexpr uint8_t I2C_SDA_PIN   = D4;
static constexpr uint8_t I2C_SCL_PIN   = D5;
static constexpr uint8_t USR1_PIN      = D6;   // force sleep
static constexpr uint8_t USR2_PIN      = D7;   // force wake / reset counter
// IMU INT1 is on D14 (the LSM6DS3 on the XIAO nRF52840 Sense routes INT1 to
// D14 — same as the 1.14 WakeUp, which wakes correctly on motion). The original
// XIAO-Display-Board-main 0.96 sketch used D9, but D9 never sees the interrupt
// on this board (D9 stays 0), so shaking didn't wake it. D14 is unused by the
// 0.96 LCD/buttons/mic, so it's safe to use here.
static constexpr uint8_t IMU_INT_PIN   = D14;  // IMU INT1 (was D9 — wrong for this board)
// LCD CS/DC/MOSI/SCK are wired by the Board_XIAO_0inch96_LCD template
// (CS=D2, DC=D3, MOSI=D10, SCK=D8); RST/BL come from the constants above.

// ========================= Colors =========================
// Config_Seeed_0inch96_LCD_ST7789 is BGR/rot2/invert=false and already produces
// correct colors, so we use the TFT_* palette directly. The original sketch's
// V_RED/V_BLUE/V_YELLOW/V_CYAN color-swap aliases (and the now-unused C_RED/
// C_BLUE/C_CYAN/C_YELLOW raw constants) are dropped — they existed only to
// compensate for the panel's R/B + Y/C swap under the old invertDisplay(true)
// init. Raw 0xRRGB literals (C_GRAY, C_LINE) stay unchanged.

static constexpr uint16_t C_BLACK  = TFT_BLACK;
static constexpr uint16_t C_WHITE  = TFT_WHITE;
static constexpr uint16_t C_GREEN  = TFT_GREEN;
static constexpr uint16_t C_GRAY   = 0x8410;
static constexpr uint16_t C_LINE   = 0x39E7;

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

// Motion wake threshold.
// Lower = easier to wake. Suggested range: 0x03 ~ 0x0A.
// 0x05 is a good starting point for "pick up device".
static constexpr uint8_t IMU_WAKE_THRESHOLD = 0x05;

// ========================= Timing =========================

static constexpr uint32_t AUTO_SLEEP_MS     = 8000;
static constexpr uint32_t UI_REFRESH_MS     = 250;
static constexpr uint32_t WAKE_LOCK_MS      = 1200;
static constexpr uint32_t SLEEP_LOG_MS      = 2000;
static constexpr uint32_t BTN_DEBOUNCE_MS   = 35;

// ========================= Runtime =========================

volatile bool g_imuWakeFlag = false;
volatile bool g_usrWakeFlag = false;

bool g_lcdOk = false;
bool g_screenAwake = true;
bool g_imuOk = false;

uint32_t g_lastActivityMs = 0;
uint32_t g_lastUiMs = 0;
uint32_t g_lastWakeMs = 0;
uint32_t g_sleepEnterMs = 0;
uint32_t g_lastSleepLogMs = 0;
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

// Backlight is runtime GPIO for the sleep demo (BL off while in System ON
// sleep, BL on when a wake redraws). display.begin<>() drives BL high at init;
// these helpers only toggle it during the run.
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

static String padRight(const String &s, int width) {
  String out = s;
  while ((int)out.length() < width) out += ' ';
  if ((int)out.length() > width) out = out.substring(0, width);
  return out;
}

static void printTextFixed(int x, int y, uint16_t color, const String &s, int widthChars) {
  if (!g_lcdOk) return;

  display.setTextSize(1);
  display.setTextColor(color, C_BLACK);
  display.setCursor(x, y);
  display.print(padRight(s, widthChars));
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
// WFE = Wait For Event.
// SEV/WFE/WFE clears stale events first, then enters System ON sleep.

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
  if (now - g_lastSleepLogMs >= SLEEP_LOG_MS) {
    g_lastSleepLogMs = now;

    Serial.print("[SYS_ON_SLEEP] loops=");
    Serial.print((unsigned long)g_sleepLoopCount);
    Serial.print(" D9=");
    Serial.print(digitalRead(IMU_INT_PIN));
    Serial.print(" screen=");
    Serial.println(g_screenAwake ? "AWAKE" : "SLEEP");
  }

  systemOnSleepOnce();
}

// ========================= LCD UI =========================

static bool initLcd() {
  backlightOn();

  if (!display.begin<Board_XIAO_0inch96_LCD<LCD_RST_PIN, LCD_BL_PIN>,
                     Config_Seeed_0inch96_LCD_ST7789>()) {
    g_lcdOk = false;
    Serial.print("[LCD] begin failed: ");
    Serial.println(display.lastResult().message);
    return false;
  }

  // Dropped: gfx->invertDisplay(true). Config_Seeed_0inch96_LCD_ST7789 is
  // BGR/rot2/invert=false and already produces correct colors.

  display.fillScreen(C_BLACK);
  display.setTextWrap(false);
  g_lcdOk = true;

  Serial.println("[LCD] OK 0.96 ST7789 80x160");
  return true;
}

static void drawStaticAwakeUi() {
  if (!g_lcdOk) return;

  display.fillScreen(C_BLACK);

  display.setTextSize(2);
  display.setTextColor(C_GREEN, C_BLACK);
  display.setCursor(6, 3);
  display.print("Hello");

  display.setTextSize(1);
  display.setTextColor(TFT_CYAN, C_BLACK);
  display.setCursor(4, 23);
  display.print("0.96 IMU Wake");

  display.drawFastHLine(5, 32, 70, C_WHITE);

  display.setTextColor(TFT_CYAN, C_BLACK);
  display.setCursor(4, 39);
  display.print("STATE");

  display.setTextColor(C_WHITE, C_BLACK);
  display.setCursor(4, 54);
  display.print("Screen");
  display.setCursor(4, 67);
  display.print("Wake");
  display.setCursor(4, 80);
  display.print("Src");

  display.drawFastHLine(5, 92, 70, C_LINE);

  display.setTextColor(TFT_YELLOW, C_BLACK);
  display.setCursor(4, 100);
  display.print("MOTION");

  display.setTextColor(C_WHITE, C_BLACK);
  display.setCursor(4, 115);
  display.print("A");
  display.setCursor(4, 128);
  display.print("G");

  display.drawFastHLine(5, 141, 70, C_LINE);

  display.setTextColor(TFT_YELLOW, C_BLACK);
  display.setCursor(4, 150);
  display.print("USR1 Sleep");
}

static String fmtAxis(float v) {
  int iv = (int)roundf(v);
  if (iv > 99) iv = 99;
  if (iv < -99) iv = -99;

  char buf[6];
  snprintf(buf, sizeof(buf), "%+d", iv);
  return String(buf);
}

static void updateAwakeUi() {
  if (!g_lcdOk || !g_screenAwake) return;

  printTextFixed(46, 54, C_GREEN, "ON", 5);
  printTextFixed(46, 67, TFT_YELLOW, String((unsigned long)g_wakeCount), 5);

  char srcBuf[8];
  snprintf(srcBuf, sizeof(srcBuf), "0x%02X", g_lastWakeSrc);
  printTextFixed(46, 80, TFT_CYAN, srcBuf, 5);

  String acc = fmtAxis(g_ax) + " " + fmtAxis(g_ay) + " " + fmtAxis(g_az);
  printTextFixed(16, 115, C_WHITE, acc, 10);

  String gyr = fmtAxis(g_gx / 10.0f) + " " + fmtAxis(g_gy / 10.0f) + " " + fmtAxis(g_gz / 10.0f);
  printTextFixed(16, 128, C_WHITE, gyr, 10);
}

static void drawSleepHintThenOff() {
  if (!g_lcdOk) return;

  display.fillScreen(C_BLACK);

  display.setTextSize(2);
  display.setTextColor(TFT_CYAN, C_BLACK);
  display.setCursor(7, 55);
  display.print("Sleep");

  display.setTextSize(1);
  display.setTextColor(C_WHITE, C_BLACK);
  display.setCursor(8, 85);
  display.print("System ON");

  display.setTextColor(TFT_YELLOW, C_BLACK);
  display.setCursor(8, 104);
  display.print("Move to wake");

  display.setTextColor(C_GRAY, C_BLACK);
  display.setCursor(8, 124);
  display.print("IMU INT = D9");
}

static void screenWake(const char *reason) {
  if (g_screenAwake && (millis() - g_lastWakeMs < WAKE_LOCK_MS)) return;

  g_screenAwake = true;
  g_lastActivityMs = millis();
  g_lastWakeMs = millis();
  g_wakeCount++;

  backlightOn();
  drawStaticAwakeUi();
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

  Serial.println("[SLEEP] screen off, entering System ON sleep; wake source IMU D9");

  drawSleepHintThenOff();
  delay(450);

  uint8_t dummy = 0;
  imuReadReg(REG_WAKE_UP_SRC, dummy);

  noInterrupts();
  g_imuWakeFlag = false;
  g_usrWakeFlag = false;
  interrupts();

  backlightOff();

  g_screenAwake = false;
  g_sleepEnterMs = millis();
  g_lastSleepLogMs = 0;
  g_sleepLoopCount = 0;
}

// ========================= IMU =========================

static bool initImuWakeInterrupt() {
  pinMode(IMU_INT_PIN, INPUT_PULLDOWN);

  int imuBegin = myIMU.begin();
  bool ok = (imuBegin == 0);

  // BDU=1 and register auto-increment enabled.
  ok &= imuWriteReg(REG_CTRL3_C, 0x44);

  // Accelerometer: 104Hz, +/-2g.
  ok &= imuWriteReg(REG_CTRL1_XL, 0x40);

  // Enable embedded functions / interrupt block.
  ok &= imuWriteReg(REG_TAP_CFG, 0x80);

  // Wake threshold and duration.
  ok &= imuWriteReg(REG_WAKE_UP_THS, IMU_WAKE_THRESHOLD);
  ok &= imuWriteReg(REG_WAKE_UP_DUR, 0x00);

  // Route wake-up interrupt to INT1.
  // MD1_CFG bit5 = INT1_WU.
  ok &= imuWriteReg(REG_MD1_CFG, 0x20);

  uint8_t dummy = 0;
  imuReadReg(REG_WAKE_UP_SRC, dummy);

  attachInterrupt(digitalPinToInterrupt(IMU_INT_PIN), imuWakeIsr, RISING);

  g_imuOk = ok;

  Serial.print("[IMU] LSM6DS3 wake interrupt on D9 ");
  Serial.println(ok ? "OK" : "FAILED");
  Serial.print("[IMU] wake threshold=0x");
  Serial.println(IMU_WAKE_THRESHOLD, HEX);

  return ok;
}

static void updateImuData() {
  if (!g_imuOk) return;

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

  // Fallback: if edge was missed but INT remains high briefly, still wake/check.
  if (digitalRead(IMU_INT_PIN) == HIGH) {
    imuPending = true;
  }

  if (!imuPending) return;

  uint8_t src = 0;
  if (imuReadReg(REG_WAKE_UP_SRC, src)) {
    g_lastWakeSrc = src;
  }

  // WAKE_UP_SRC bit3 WU_IA indicates wake event latched.
  // Axis bits can also appear, so accept non-zero source as wake evidence.
  if ((src & 0x08) || (src & 0x07) || !g_screenAwake) {
    screenWake("IMU_D9");
  }
}

static void resetCounters() {
  g_wakeCount = 0;
  g_imuIntCount = 0;
  g_usrWakeCount = 0;
  g_lastWakeSrc = 0;
  g_lastActivityMs = millis();

  Serial.println("[BTN] USR2 reset counters / wake");

  if (g_screenAwake) {
    drawStaticAwakeUi();
    updateAwakeUi();
  }
}

// ========================= Arduino =========================

void setup() {
  Serial.begin(115200);
  delay(800);

  pinMode(USR1_PIN, INPUT_PULLUP);
  pinMode(USR2_PIN, INPUT_PULLUP);
  pinMode(LCD_BL_PIN, OUTPUT);

  attachInterrupt(digitalPinToInterrupt(USR2_PIN), usrWakeIsr, FALLING);

  Wire.begin();

  Serial.println();
  Serial.println("=== XIAO nRF52840 Plus 0.96 IMU D9 System ON Wake Test v0.1 ===");
  Serial.println("[TEST] screen off -> System ON sleep -> move/pick up -> IMU D9 wakes screen");
  Serial.println("[BTN] USR1 force sleep, USR2 wake/reset counters");
  Serial.println("[NOTE] Use battery power for current measurement; USB current will dominate.");

  initLcd();
  initImuWakeInterrupt();

  g_lastActivityMs = millis();

  drawStaticAwakeUi();
  updateImuData();
  updateAwakeUi();
}

void loop() {
  handleWakeEvents();

  if (!g_screenAwake) {
    // Sleep state: no UI refresh, no sensor polling.
    enterSystemOnSleepLoop();
    handleWakeEvents();
    delay(1);
    return;
  }

  // USR1: force sleep.
  if (digitalRead(USR1_PIN) == LOW) {
    delay(BTN_DEBOUNCE_MS);
    if (digitalRead(USR1_PIN) == LOW) {
      while (digitalRead(USR1_PIN) == LOW) delay(5);
      screenSleep();
      return;
    }
  }

  // USR2: wake / reset counter while awake.
  if (digitalRead(USR2_PIN) == LOW) {
    delay(BTN_DEBOUNCE_MS);
    if (digitalRead(USR2_PIN) == LOW) {
      while (digitalRead(USR2_PIN) == LOW) delay(5);
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
