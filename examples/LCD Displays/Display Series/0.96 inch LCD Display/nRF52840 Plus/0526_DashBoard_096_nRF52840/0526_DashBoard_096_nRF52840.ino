/**
 * Product: XIAO 0.96 inch LCD Board (ST7789 80x160 IPS, no touch)
 * Display: ST7789 80x160, BGR, rotation 2, no touch
 * Target:  XIAO nRF52840 Plus (RST=38, BL=37). For ESP32-S3 Plus use the sibling
 *          ESP32-S3 Plus folder (RST=13, BL=12).
 * Wiring:  CS=D2, DC=D3, MOSI=D10, SCLK=D8; RST=38, BL=37.
 * Demo:    Factory dashboard: battery presence/charge state machine, PDM mic VU meter,
 *          LSM6DS3 double-tap counter, 2 user buttons, backlight control.
 *
 * Ported from XIAO-Display-Board-main (0526_DashBoard_096_nRF52840, Arduino_GFX). The
 * Seeed_GFX Board/Config templates replace the original Arduino_SWSPI+Arduino_ST7789
 * bus/panel setup and the manual MADCTL/invert/swapbytes. Dropped invertDisplay(true)
 * and the V_RED/V_BLUE/V_YELLOW/V_CYAN color-swap aliases: Config_Seeed_0inch96_LCD_ST7789
 * is BGR/rot2/invert=false and already produces correct colors. Kept PDM mic, LSM6DS3
 * double-tap, and the nRF battery subsystem (P0.14/P0.17/PIN_VBAT, NRF_P0->) as the
 * nRF52840 Plus target's Arduino-level peripheral code.
 */

#include <Seeed_GFX.h>
#include "board/boards/XIAO_LCD_Board.h"
#include "driver/tft/Driver_ST7789.h"
#include "panel/Panel_TFT.h"
#include <Arduino.h>
#include <Wire.h>
#include <PDM.h>
#include "SparkFunLSM6DS3.h"
#include <nrf.h>
#include <nrf_gpio.h>
#include <math.h>

Seeed_GFX display;

static constexpr int8_t LCD_RST_PIN = 38;  // XIAO nRF52840 Plus
static constexpr int8_t LCD_BL_PIN  = 37;

// ========================= Pins =========================

static constexpr uint8_t PDM_CLK_PIN   = D0;
static constexpr uint8_t PDM_DATA_PIN  = D1;
static constexpr uint8_t I2C_SDA_PIN   = D4;
static constexpr uint8_t I2C_SCL_PIN   = D5;
static constexpr uint8_t BTN_A_PIN     = D6;   // USR1: backlight ON/OFF
static constexpr uint8_t BTN_B_PIN     = D7;   // USR2: toggle header + reset tap count
// IMU INT1 is on D14 (LSM6DS3 on the XIAO nRF52840 Sense routes INT1 to D14 —
// same as the 1.14 dashboard/WakeUp). The original 0.96 PRD said D9, but D9 never
// sees the interrupt on this board (stays 0), so the double-tap count never
// incremented. D14 is unused by the 0.96 LCD/buttons/mic.
static constexpr uint8_t IMU_INT_PIN   = D14;  // IMU INT1 (was D9 — wrong for this board)
// LCD CS/DC/MOSI/SCK are wired by the Board_XIAO_0inch96_LCD template
// (CS=D2, DC=D3, MOSI=D10, SCK=D8); RST/BL come from the constants above.

static constexpr uint8_t READ_BAT_P0_PIN = 14; // P0.14 / ~READ_BAT
static constexpr uint8_t CHG_P0_PIN      = 17; // P0.17 / ~CHG

#ifndef PIN_VBAT
#define PIN_VBAT 35
#endif

// ========================= Timing =========================

static constexpr uint32_t UI_FAST_MS       = 100;
static constexpr uint32_t UI_SLOW_MS       = 140;
static constexpr uint32_t UI_VU_MS         = 70;
static constexpr uint32_t BAT_REFRESH_MS   = 600;
static constexpr uint32_t SERIAL_MS        = 700;
static constexpr uint32_t BTN_DEBOUNCE_MS  = 18;
static constexpr uint32_t BTN_ACTION_LOCKOUT_MS = 70;
static constexpr uint32_t TAP_DEBOUNCE_MS  = 220;
static constexpr uint32_t MIC_DECAY_MS     = 120;

// ========================= LCD parameters =========================
// Width/height are used by the UI layout. Rotation (2), color order (BGR), inversion
// (false) and the 24/0 column offset are baked into Config_Seeed_0inch96_LCD_ST7789,
// so the manual Arduino_SWSPI/Arduino_ST7789 bus+panel construction and the
// invertDisplay(true) call are dropped (the Board template drives RST+BL).

static constexpr int LCD_W = 80;
static constexpr int LCD_H = 160;

// ========================= Colors =========================
// Config_Seeed_0inch96_LCD_ST7789 is BGR/rot2/invert=false and already produces
// correct colors, so we use the TFT_* palette directly. The original sketch's
// V_RED/V_BLUE/V_YELLOW/V_CYAN color-swap aliases (and the now-unused C_RED/
// C_BLUE/C_CYAN/C_YELLOW raw constants) are dropped — they existed only to
// compensate for the panel's R/B + Y/C swap under the old invertDisplay(true)
// init. RGB565_LIGHTGREEN maps to TFT_GREEN (TFT_LIGHTGREEN is not defined in
// Seeed_GFX). Raw 0xRRGB literals (C_ORANGE/C_GRAY/C_LINE/C_DIM) stay unchanged.

static constexpr uint16_t C_BLACK   = TFT_BLACK;
static constexpr uint16_t C_WHITE   = TFT_WHITE;
static constexpr uint16_t C_GREEN   = TFT_GREEN;
static constexpr uint16_t C_ORANGE  = 0xFD20;
static constexpr uint16_t C_GRAY    = 0x8410;
static constexpr uint16_t C_LINE    = 0x39E7;
static constexpr uint16_t C_DIM     = 0x2104;

// ========================= Battery =========================

static constexpr float BAT_DIVIDER_RATIO = (1000.0f + 499.0f) / 499.0f;
static constexpr float BAT_CAL_FACTOR    = 1.000f;
static constexpr int ADC_BITS            = 12;
static constexpr int ADC_MAX             = (1 << ADC_BITS) - 1;
static constexpr float ADC_FULL_SCALE_V  = 3.600f;

static constexpr uint16_t BAT_PRESENT_MIN_RAW = 80;
static constexpr uint16_t BAT_FLOAT_RANGE_RAW = 80;
static constexpr uint16_t BAT_STABLE_PRESENT_SPREAD_RAW = 30;
// When the board is already USB-powered and the user plugs in a battery,
// the charger can make VBAT noisier for the first few samples. Do not require
// the normal stableBatch spread in this specific CHG-low insertion path.
static constexpr uint16_t BAT_USB_INSERT_CHG_SPREAD_RAW = 180;
static constexpr float BAT_VALID_MIN_V = 2.80f;
static constexpr float BAT_VALID_MAX_V = 4.60f;
static constexpr float BAT_INSERT_DELTA_V = 0.10f;
static constexpr float BAT_REMOVE_DELTA_V = 0.14f;
static constexpr float BAT_NOISY_CLOSE_DELTA_V = 0.08f;
static constexpr uint8_t BAT_INSERT_CONFIRM_COUNT = 2;
static constexpr uint8_t BAT_REMOVE_CONFIRM_COUNT = 4;
static constexpr uint8_t BAT_PRESENT_NOISY_HOLD_COUNT = 4;
static constexpr uint32_t BAT_CHG_TRANSIENT_HOLD_MS = 900;
static constexpr uint8_t CHG_SAMPLE_COUNT = 9;
static constexpr uint8_t CHG_HIGH_CLEAR_COUNT = 1;

// ========================= PDM Mic =========================

static constexpr int MIC_SAMPLE_RATE_HZ = 16000;
static constexpr int MIC_CHANNELS       = 1;
static constexpr int MIC_GAIN           = 30;
static constexpr int MIC_BUF_SAMPLES    = 256;

volatile uint16_t g_micPeak = 0;
volatile uint32_t g_micRms = 0;
volatile uint32_t g_micBlocks = 0;
volatile uint32_t g_micLastUpdateMs = 0;
int16_t g_pdmBuf[MIC_BUF_SAMPLES];
float g_vuSmooth = 0.0f;
float g_vuFast = 0.0f;
int g_vuDisplaySegments = 0;
int g_cachedVuSegments = -1;
int g_cachedVuWidth = -1;
uint16_t g_cachedVuColor = 0xFFFF;
uint32_t g_lastVuStepMs = 0;

// VU anti-flicker state.
// Candidate debounce prevents adjacent blocks from flickering near thresholds.
// Delta drawing avoids full-bar clear/redraw flashes.
int g_vuCandidateSegments = 0;
uint8_t g_vuCandidateCount = 0;
uint32_t g_lastVuRenderMs = 0;

// Anti-rebound guard:
// after the VU starts falling, ignore small upward bounces for a short window.
// This fixes the visible reverse pulse during decay.
uint32_t g_vuFallGuardUntilMs = 0;

// 0.96 MIC calibration.
// The previous fixed thresholds made room noise look like full-scale.
// Track a slow noise floor and draw only signal above that floor.
float g_micNoiseFloor = 65.0f;
bool g_micNoiseReady = false;
uint32_t g_micNoiseStartMs = 0;

// ========================= State =========================

LSM6DS3 myIMU(I2C_MODE, 0x6A);

bool g_lcdOk = false;
bool g_micOk = false;
bool g_imuOk = false;
bool g_blOn = true;
bool g_headerSeeed = false;

float g_ax = 0, g_ay = 0, g_az = 0;
float g_gx = 0, g_gy = 0, g_gz = 0;

bool g_btnA = false;
bool g_btnB = false;
int g_btnARaw = HIGH;
int g_btnBRaw = HIGH;

bool g_btnALastRawPressed = false;
bool g_btnBLastRawPressed = false;
uint32_t g_btnALastChangeMs = 0;
uint32_t g_btnBLastChangeMs = 0;

volatile uint8_t g_btnAIrqCount = 0;
volatile uint8_t g_btnBIrqCount = 0;
bool g_btnAPendingAction = false;
bool g_btnBPendingAction = false;
bool g_btnAArmed = true;
bool g_btnBArmed = true;
uint32_t g_btnALastQueueMs = 0;
uint32_t g_btnBLastQueueMs = 0;
uint32_t g_lastBtnActionMs = 0;

volatile bool g_imuIntFlag = false;
uint32_t g_doubleTapCount = 0;
uint32_t g_lastTapMs = 0;

uint32_t g_lastFastMs = 0;
uint32_t g_lastSlowMs = 0;
uint32_t g_lastVuMs = 0;
uint32_t g_lastBatMs = 0;
uint32_t g_lastSerialMs = 0;
uint32_t g_frameCounter = 0;

struct BatteryState {
  uint16_t raw = 0;
  uint16_t rawMin = 0;
  uint16_t rawMax = 0;
  float vadc = 0.0f;
  float vbat = 0.0f;
  int percent = 0;
  bool charging = false;
  bool valid = false;
};

BatteryState g_bat;
BatteryState g_lastGoodBat;
bool g_haveLastGoodBat = false;
const char *g_batFilterState = "BOOT";

bool g_chgRawLow = false;
bool g_chgState = false;
uint8_t g_chgHighStreak = 0;
uint32_t g_lastChgLowMs = 0;
bool g_chgRawInitialized = false;
uint32_t g_lastChgRawChangeMs = 0;

enum BatteryPresenceState {
  BAT_BOOT = 0,
  BAT_USB_ONLY,
  BAT_INSERT_CANDIDATE,
  BAT_PRESENT,
  BAT_REMOVE_CANDIDATE
};

BatteryPresenceState g_batState = BAT_BOOT;
bool g_batPhysicallyConfirmed = false;
bool g_usbBaselineValid = false;
float g_usbBaselineVbat = 0.0f;
uint16_t g_usbBaselineRaw = 0;
uint16_t g_usbBaselineSpread = 0;
bool g_prevChgRawLow = false;
uint8_t g_insertCandidateStreak = 0;
uint8_t g_removeCandidateStreak = 0;

// ========================= UI layout =========================

static constexpr int ROW_TITLE  = 3;
static constexpr int ROW_SUB    = 22;

// Flat compact layout for 80x160: no card borders, only section labels + dividers.
static constexpr int SECTION_X0 = 4;
static constexpr int SECTION_X1 = 75;

static constexpr int Y_SYS_LABEL = 35;
static constexpr int Y_SYS_ROW1  = 47;
static constexpr int Y_SYS_ROW2  = 60;
static constexpr int Y_SYS_DIV   = 70;

static constexpr int Y_MOTION_LABEL = 76;
static constexpr int Y_TAP          = 76;
static constexpr int Y_ACC          = 90;
static constexpr int Y_GYR          = 103;
static constexpr int Y_MOTION_DIV   = 112;

// MIC returns to the 1.47-style compact layout:
// title and volume bar on the same row, Raw below.
static constexpr int Y_MIC_LABEL = 119;
static constexpr int Y_RAW       = 134;

static constexpr int ROW_FOOT = 151;

static constexpr int VU_X = 33;
static constexpr int VU_Y = Y_MIC_LABEL;
static constexpr int VU_W = 41;
static constexpr int VU_H = 9;
static constexpr int VU_SEG_COUNT = 8;
static constexpr int VU_GAP = 1;
static constexpr int VU_SEG_W = (VU_W - 2 - (VU_SEG_COUNT - 1) * VU_GAP) / VU_SEG_COUNT;

String cache_header = "";
String cache_sys = "";
String cache_bl = "";
String cache_acc = "";
String cache_gyr = "";
String cache_tap = "";
String cache_btn = "";
String cache_raw = "";

// Battery UI snapshot. The raw battery state can jitter because USB-only VBAT
// floats and charge-status can transition. Do not redraw the BAT row on every
// minor measurement change.
bool g_batUiInit = false;
bool g_batUiUsb = true;
bool g_batUiValid = false;
bool g_batUiCharging = false;
int g_batUiPct = -1;
uint32_t g_lastBatUiCommitMs = 0;


// ========================= Helpers =========================

static String padRight(const String &s, int width) {
  String out = s;
  while ((int)out.length() < width) out += ' ';
  if ((int)out.length() > width) out = out.substring(0, width);
  return out;
}

static uint16_t colorByPercent(int pct) {
  if (pct <= 15) return TFT_RED;
  if (pct <= 35) return TFT_YELLOW;
  return C_GREEN;
}

static int lipoPercent(float v) {
  struct Point { float v; int p; };
  static const Point table[] = {
    {4.20f, 100}, {4.10f, 90}, {4.00f, 80}, {3.92f, 70}, {3.85f, 60},
    {3.79f, 50},  {3.72f, 40}, {3.66f, 30}, {3.58f, 20}, {3.50f, 10}, {3.30f, 0}
  };
  if (v >= table[0].v) return 100;
  if (v <= table[10].v) return 0;
  for (int i = 0; i < 10; i++) {
    if (v <= table[i].v && v >= table[i + 1].v) {
      float t = (v - table[i + 1].v) / (table[i].v - table[i + 1].v);
      return table[i + 1].p + (int)roundf(t * (table[i].p - table[i + 1].p));
    }
  }
  return 0;
}

static const char *batStateName(BatteryPresenceState s) {
  switch (s) {
    case BAT_BOOT: return "BOOT";
    case BAT_USB_ONLY: return "USB";
    case BAT_INSERT_CANDIDATE: return "INS?";
    case BAT_PRESENT: return "BAT";
    case BAT_REMOVE_CANDIDATE: return "RM?";
    default: return "?";
  }
}

static void printTextFixed(int x, int y, uint16_t color, const String &s, int widthChars) {
  if (!g_lcdOk) return;
  display.setTextSize(1);
  display.setTextColor(color, C_BLACK);
  display.setCursor(x, y);
  display.print(padRight(s, widthChars));
}

static void cleanScreenEdges() {
  if (!g_lcdOk) return;
  display.drawFastVLine(0, 0, LCD_H, C_BLACK);
  display.drawFastVLine(1, 0, LCD_H, C_BLACK);
  display.drawFastHLine(0, 0, LCD_W, C_BLACK);
}

// ========================= Backlight =========================

static void applyBacklight() {
  pinMode(LCD_BL_PIN, OUTPUT);
  digitalWrite(LCD_BL_PIN, g_blOn ? HIGH : LOW);
}

static void toggleBacklight() {
  g_blOn = !g_blOn;
  applyBacklight();
  cache_bl = "";
}

// ========================= Button IRQ / queue =========================

static void queueButtonA(uint32_t now) {
  if (!g_btnAArmed) return;
  if (now - g_btnALastQueueMs < BTN_DEBOUNCE_MS) return;
  g_btnALastQueueMs = now;
  g_btnAPendingAction = true;
  g_btnAArmed = false;
}

static void queueButtonB(uint32_t now) {
  if (!g_btnBArmed) return;
  if (now - g_btnBLastQueueMs < BTN_DEBOUNCE_MS) return;
  g_btnBLastQueueMs = now;
  g_btnBPendingAction = true;
  g_btnBArmed = false;
}

void btnAIrqIsr() {
  if (g_btnAIrqCount < 250) g_btnAIrqCount++;
}

void btnBIrqIsr() {
  if (g_btnBIrqCount < 250) g_btnBIrqCount++;
}

static void updateButtons() {
  uint32_t now = millis();
  uint8_t irqA = 0;
  uint8_t irqB = 0;
  noInterrupts();
  irqA = g_btnAIrqCount;
  irqB = g_btnBIrqCount;
  g_btnAIrqCount = 0;
  g_btnBIrqCount = 0;
  interrupts();

  g_btnARaw = digitalRead(BTN_A_PIN);
  g_btnBRaw = digitalRead(BTN_B_PIN);
  bool rawA = (g_btnARaw == LOW);
  bool rawB = (g_btnBRaw == LOW);

  if (irqA) {
    queueButtonA(now);
    g_btnALastRawPressed = rawA;
    g_btnALastChangeMs = now;
  }
  if (irqB) {
    queueButtonB(now);
    g_btnBLastRawPressed = rawB;
    g_btnBLastChangeMs = now;
  }

  if (rawA != g_btnALastRawPressed) {
    g_btnALastRawPressed = rawA;
    g_btnALastChangeMs = now;
  }
  if (rawB != g_btnBLastRawPressed) {
    g_btnBLastRawPressed = rawB;
    g_btnBLastChangeMs = now;
  }

  if ((now - g_btnALastChangeMs) >= BTN_DEBOUNCE_MS && rawA != g_btnA) {
    bool old = g_btnA;
    g_btnA = rawA;
    if (g_btnA && !old) queueButtonA(now);
  }
  if ((now - g_btnBLastChangeMs) >= BTN_DEBOUNCE_MS && rawB != g_btnB) {
    bool old = g_btnB;
    g_btnB = rawB;
    if (g_btnB && !old) queueButtonB(now);
  }

  if (!rawA && g_btnA && (now - g_btnALastChangeMs) >= BTN_DEBOUNCE_MS) {
    g_btnA = false;
    g_btnAArmed = true;
  }
  if (!rawB && g_btnB && (now - g_btnBLastChangeMs) >= BTN_DEBOUNCE_MS) {
    g_btnB = false;
    g_btnBArmed = true;
  }

  // If an IRQ captured a very short tap that has already been released before
  // polling sees it, arm the next physical press again.
  if (!rawA && !g_btnA) g_btnAArmed = true;
  if (!rawB && !g_btnB) g_btnBArmed = true;
}

static void handleButtonActions() {
  if (!g_btnAPendingAction && !g_btnBPendingAction) return;

  uint32_t now = millis();
  if (now - g_lastBtnActionMs < BTN_ACTION_LOCKOUT_MS) return;

  if (g_btnBPendingAction) {
    g_btnBPendingAction = false;
    g_btnAPendingAction = false;

    // USR2: only toggle Hello / Seeed.
    // Do not reset Tap here.
    g_headerSeeed = !g_headerSeeed;
    cache_header = "";
    g_lastBtnActionMs = now;

    Serial.println("[BTN] USR2 header Hello/XIAO toggle");
    return;
  }

  if (g_btnAPendingAction) {
    g_btnAPendingAction = false;

    // USR1: backlight ON/OFF.
    toggleBacklight();
    g_lastBtnActionMs = now;

    Serial.print("[BTN] USR1 backlight=");
    Serial.println(g_blOn ? "ON" : "OFF");
  }
}

// ========================= Battery =========================

static void enableBatteryDivider() {
  nrf_gpio_cfg_output(READ_BAT_P0_PIN);
  nrf_gpio_pin_clear(READ_BAT_P0_PIN);
}

static void disableBatteryDivider() {
  nrf_gpio_cfg_output(READ_BAT_P0_PIN);
  nrf_gpio_pin_set(READ_BAT_P0_PIN);
}

static bool sampleChargingRawLow() {
  uint8_t lowCount = 0;
  for (uint8_t i = 0; i < CHG_SAMPLE_COUNT; i++) {
    if ((NRF_P0->IN & (1UL << CHG_P0_PIN)) == 0) lowCount++;
    delayMicroseconds(400);
  }
  bool newRawLow = (lowCount >= ((CHG_SAMPLE_COUNT / 2) + 1));
  uint32_t now = millis();
  if (!g_chgRawInitialized) {
    g_chgRawInitialized = true;
    g_lastChgRawChangeMs = now;
  } else if (newRawLow != g_chgRawLow) {
    g_lastChgRawChangeMs = now;
  }
  g_chgRawLow = newRawLow;
  return g_chgRawLow;
}

static bool updateChargingState(bool batValid, float vbat) {
  (void)vbat;
  bool rawLow = sampleChargingRawLow();
  if (!batValid) {
    g_chgState = false;
    g_chgHighStreak = 0;
    return false;
  }
  if (rawLow) {
    g_chgState = true;
    g_lastChgLowMs = millis();
    g_chgHighStreak = 0;
    return true;
  }
  if (g_chgHighStreak < 255) g_chgHighStreak++;
  if (g_chgHighStreak >= CHG_HIGH_CLEAR_COUNT) g_chgState = false;
  return g_chgState;
}

static uint16_t readBatteryRawAvg(uint8_t samples, uint16_t &rawMin, uint16_t &rawMax) {
  if (samples > 32) samples = 32;
  uint16_t buf[32];
  for (uint8_t i = 0; i < 6; i++) { (void)analogRead(PIN_VBAT); delay(2); }
  for (uint8_t i = 0; i < samples; i++) { buf[i] = analogRead(PIN_VBAT); delay(2); }
  for (uint8_t i = 0; i < samples; i++) {
    for (uint8_t j = i + 1; j < samples; j++) {
      if (buf[j] < buf[i]) { uint16_t t = buf[i]; buf[i] = buf[j]; buf[j] = t; }
    }
  }
  uint8_t trim = samples >= 16 ? 4 : 1;
  rawMin = buf[trim];
  rawMax = buf[samples - 1 - trim];
  uint32_t sum = 0;
  uint8_t count = 0;
  for (uint8_t i = trim; i < samples - trim; i++) { sum += buf[i]; count++; }
  return count ? (uint16_t)(sum / count) : buf[samples / 2];
}

static void setBatteryAbsentUsb(const char *filterState) {
  g_batState = BAT_USB_ONLY;
  g_batPhysicallyConfirmed = false;
  g_haveLastGoodBat = false;
  g_chgState = false;
  g_chgHighStreak = 0;
  g_batFilterState = filterState;
}

static void confirmBatteryPresent(const BatteryState &measured, const char *filterState) {
  g_batState = BAT_PRESENT;
  g_batPhysicallyConfirmed = true;
  g_insertCandidateStreak = 0;
  g_removeCandidateStreak = 0;
  g_batFilterState = filterState;
  g_bat = measured;
  g_bat.valid = true;
  g_lastGoodBat = g_bat;
  g_haveLastGoodBat = true;
}

static void updateUsbOnlyBaseline(const BatteryState &m, uint16_t spread) {
  if (!g_usbBaselineValid) {
    g_usbBaselineValid = true;
    g_usbBaselineVbat = m.vbat;
    g_usbBaselineRaw = m.raw;
    g_usbBaselineSpread = spread;
    return;
  }
  g_usbBaselineVbat = g_usbBaselineVbat * 0.85f + m.vbat * 0.15f;
  g_usbBaselineRaw = (uint16_t)((float)g_usbBaselineRaw * 0.85f + (float)m.raw * 0.15f);
  g_usbBaselineSpread = (uint16_t)((float)g_usbBaselineSpread * 0.85f + (float)spread * 0.15f);
}

static void updateBattery() {
  BatteryState measured;
  enableBatteryDivider();
  delay(30);
  measured.raw = readBatteryRawAvg(24, measured.rawMin, measured.rawMax);
  measured.vadc = ((float)measured.raw * ADC_FULL_SCALE_V) / (float)ADC_MAX;
  measured.vbat = measured.vadc * BAT_DIVIDER_RATIO * BAT_CAL_FACTOR;
  measured.percent = lipoPercent(measured.vbat);

  uint16_t spread = measured.rawMax - measured.rawMin;
  bool voltagePlausible = (measured.raw > BAT_PRESENT_MIN_RAW && measured.vbat > BAT_VALID_MIN_V && measured.vbat < BAT_VALID_MAX_V);
  bool stableBatch = voltagePlausible && (spread <= BAT_FLOAT_RANGE_RAW);
  bool veryStableBatch = voltagePlausible && (spread <= BAT_STABLE_PRESENT_SPREAD_RAW);
  bool chargingNow = updateChargingState(voltagePlausible, measured.vbat);
  bool chgEdgeLow = (!g_prevChgRawLow && g_chgRawLow);
  g_prevChgRawLow = g_chgRawLow;
  bool closeToLastGood = g_haveLastGoodBat && fabsf(measured.vbat - g_lastGoodBat.vbat) <= BAT_NOISY_CLOSE_DELTA_V;
  bool farFromLastGood = g_haveLastGoodBat && fabsf(measured.vbat - g_lastGoodBat.vbat) >= BAT_REMOVE_DELTA_V;
  disableBatteryDivider();

  if (!voltagePlausible) {
    if (g_batPhysicallyConfirmed) {
      g_removeCandidateStreak++;
      if (g_removeCandidateStreak < BAT_REMOVE_CONFIRM_COUNT) {
        g_bat = g_lastGoodBat;
        g_bat.valid = true;
        g_bat.charging = chargingNow;
        g_batFilterState = "HOLD";
        return;
      }
    }
    setBatteryAbsentUsb("MISS");
    g_bat = measured;
    g_bat.valid = false;
    g_bat.charging = false;
    return;
  }

  if (g_batPhysicallyConfirmed) {
    uint32_t nowMs = millis();
    bool recentChgTransition = (nowMs - g_lastChgRawChangeMs) < BAT_CHG_TRANSIENT_HOLD_MS;
    bool noisyOrJump = (!stableBatch) || farFromLastGood;
    bool chgHighNow = !g_chgRawLow;
    bool likelyRemoved = noisyOrJump && chgHighNow;

    if (noisyOrJump) {
      if (likelyRemoved) {
        if (g_removeCandidateStreak < 255) g_removeCandidateStreak++;
        g_batState = BAT_REMOVE_CANDIDATE;
        g_bat = measured;
        g_bat.valid = false;
        g_bat.charging = false;
        if (g_removeCandidateStreak >= BAT_REMOVE_CONFIRM_COUNT) {
          setBatteryAbsentUsb("REMOVED");
          g_batFilterState = "REMOVED";
        } else {
          g_batFilterState = "RM_UI";
        }
        return;
      }

      if (closeToLastGood && (recentChgTransition || g_removeCandidateStreak < BAT_PRESENT_NOISY_HOLD_COUNT)) {
        if (g_removeCandidateStreak < 255) g_removeCandidateStreak++;
        g_batState = BAT_PRESENT;
        BatteryState filtered = g_lastGoodBat;
        filtered.valid = true;
        filtered.charging = chargingNow;
        g_bat = filtered;
        g_batFilterState = recentChgTransition ? "USB_TR" : "NOISY_H";
        return;
      }

      if (g_removeCandidateStreak < 255) g_removeCandidateStreak++;
      g_batState = BAT_REMOVE_CANDIDATE;
      g_bat = measured;
      g_bat.valid = false;
      g_bat.charging = false;
      g_batFilterState = "RM_UI";
      return;
    }

    g_removeCandidateStreak = 0;
    if (stableBatch) {
      measured.valid = true;
      measured.charging = chargingNow;
      confirmBatteryPresent(measured, "STABLE");
      return;
    }
  }

  if (stableBatch && !g_chgRawLow) {
    measured.valid = true;
    measured.charging = false;
    confirmBatteryPresent(measured, "BAT_ONLY");
    return;
  }

  bool baselineDelta = g_usbBaselineValid && fabsf(measured.vbat - g_usbBaselineVbat) >= BAT_INSERT_DELTA_V;
  bool spreadImproved = g_usbBaselineValid && (g_usbBaselineSpread > BAT_FLOAT_RANGE_RAW) && veryStableBatch;
  bool stableLowAfterNoisyUsb = g_usbBaselineValid && (g_usbBaselineSpread > BAT_STABLE_PRESENT_SPREAD_RAW) && stableBatch && g_chgRawLow;
  bool recentLowEdge = g_chgRawLow && ((millis() - g_lastChgRawChangeMs) < 2500);

  // Important USB-insert path:
  // If we are in USB-only state and the charger CHG pin is LOW, that is strong
  // evidence that a battery was inserted and is accepting charge. The previous
  // algorithm could miss this when baselineDelta was small or when the initial
  // charging samples were a bit noisy, so the UI stayed at USB PWR.
  bool chargeLowInsertBatch =
      voltagePlausible &&
      g_chgRawLow &&
      (spread <= BAT_USB_INSERT_CHG_SPREAD_RAW) &&
      (measured.vbat >= 3.05f) &&
      (measured.vbat <= 4.35f);

  bool normalInsertCandidate =
      stableBatch &&
      (chgEdgeLow || recentLowEdge || spreadImproved || baselineDelta || stableLowAfterNoisyUsb);

  bool insertCandidate = normalInsertCandidate || chargeLowInsertBatch;

  if (insertCandidate) {
    if (g_insertCandidateStreak < 255) g_insertCandidateStreak++;
    g_batState = BAT_INSERT_CANDIDATE;

    if (chargeLowInsertBatch && !normalInsertCandidate) {
      g_batFilterState = "INS_CHG";
    } else {
      g_batFilterState = stableLowAfterNoisyUsb ? "INS_ST" : "INS?";
    }

    if (g_insertCandidateStreak >= BAT_INSERT_CONFIRM_COUNT) {
      measured.valid = true;
      measured.charging = chargingNow || g_chgRawLow;
      confirmBatteryPresent(measured, (chargingNow || g_chgRawLow) ? "INSERT_CHG" : "INSERT");
      return;
    }
  } else {
    if (g_insertCandidateStreak > 0) g_insertCandidateStreak--;
    g_batState = BAT_USB_ONLY;
    g_batFilterState = "USBVBAT";
    updateUsbOnlyBaseline(measured, spread);
  }

  g_bat = measured;
  g_bat.valid = false;
  g_bat.charging = false;
}

static String batteryTinyText() {
  if (!g_bat.valid) {
    if (g_batState == BAT_USB_ONLY || g_batState == BAT_INSERT_CANDIDATE || g_batState == BAT_REMOVE_CANDIDATE) return "USB";
    return "NO";
  }
  char buf[8];
  snprintf(buf, sizeof(buf), "%d%%", g_bat.percent);
  return String(buf);
}


// ========================= Tiny text + battery icons =========================

static uint8_t tinyGlyphRow(char ch, uint8_t row) {
  // 3x5 tiny font. Bit2 is left pixel, bit0 is right pixel.
  // Deliberately supports lowercase n/l/u/s so the footer can show "nRF".
  switch (ch) {
    case '0': { static const uint8_t g[5] = {0b111,0b101,0b101,0b101,0b111}; return g[row]; }
    case '1': { static const uint8_t g[5] = {0b010,0b110,0b010,0b010,0b111}; return g[row]; }
    case '2': { static const uint8_t g[5] = {0b111,0b001,0b111,0b100,0b111}; return g[row]; }
    case '3': { static const uint8_t g[5] = {0b111,0b001,0b111,0b001,0b111}; return g[row]; }
    case '4': { static const uint8_t g[5] = {0b101,0b101,0b111,0b001,0b001}; return g[row]; }
    case '5': { static const uint8_t g[5] = {0b111,0b100,0b111,0b001,0b111}; return g[row]; }
    case '6': { static const uint8_t g[5] = {0b111,0b100,0b111,0b101,0b111}; return g[row]; }
    case '7': { static const uint8_t g[5] = {0b111,0b001,0b010,0b010,0b010}; return g[row]; }
    case '8': { static const uint8_t g[5] = {0b111,0b101,0b111,0b101,0b111}; return g[row]; }
    case '9': { static const uint8_t g[5] = {0b111,0b101,0b111,0b001,0b111}; return g[row]; }
    case '.': { static const uint8_t g[5] = {0b000,0b000,0b000,0b000,0b010}; return g[row]; }

    case 'A': { static const uint8_t g[5] = {0b111,0b101,0b111,0b101,0b101}; return g[row]; }
    case 'B': { static const uint8_t g[5] = {0b110,0b101,0b110,0b101,0b110}; return g[row]; }
    case 'C': { static const uint8_t g[5] = {0b111,0b100,0b100,0b100,0b111}; return g[row]; }
    case 'D': { static const uint8_t g[5] = {0b110,0b101,0b101,0b101,0b110}; return g[row]; }
    case 'E': { static const uint8_t g[5] = {0b111,0b100,0b110,0b100,0b111}; return g[row]; }
    case 'F': { static const uint8_t g[5] = {0b111,0b100,0b111,0b100,0b100}; return g[row]; }
    case 'H': { static const uint8_t g[5] = {0b101,0b101,0b111,0b101,0b101}; return g[row]; }
    case 'I': { static const uint8_t g[5] = {0b111,0b010,0b010,0b010,0b111}; return g[row]; }
    case 'L': { static const uint8_t g[5] = {0b100,0b100,0b100,0b100,0b111}; return g[row]; }
    case 'N': { static const uint8_t g[5] = {0b101,0b111,0b111,0b111,0b101}; return g[row]; }
    case 'O': { static const uint8_t g[5] = {0b111,0b101,0b101,0b101,0b111}; return g[row]; }
    case 'P': { static const uint8_t g[5] = {0b111,0b101,0b111,0b100,0b100}; return g[row]; }
    case 'R': { static const uint8_t g[5] = {0b110,0b101,0b110,0b101,0b101}; return g[row]; }
    case 'S': { static const uint8_t g[5] = {0b111,0b100,0b111,0b001,0b111}; return g[row]; }
    case 'T': { static const uint8_t g[5] = {0b111,0b010,0b010,0b010,0b010}; return g[row]; }
    case 'U': { static const uint8_t g[5] = {0b101,0b101,0b101,0b101,0b111}; return g[row]; }
    case 'X': { static const uint8_t g[5] = {0b101,0b101,0b010,0b101,0b101}; return g[row]; }
    case 'Y': { static const uint8_t g[5] = {0b101,0b101,0b010,0b010,0b010}; return g[row]; }

    case 'a': { static const uint8_t g[5] = {0b000,0b111,0b001,0b111,0b111}; return g[row]; }
    case 'c': { static const uint8_t g[5] = {0b000,0b111,0b100,0b100,0b111}; return g[row]; }
    case 'd': { static const uint8_t g[5] = {0b001,0b001,0b111,0b101,0b111}; return g[row]; }
    case 'e': { static const uint8_t g[5] = {0b000,0b111,0b111,0b100,0b111}; return g[row]; }
    case 'h': { static const uint8_t g[5] = {0b100,0b100,0b111,0b101,0b101}; return g[row]; }
    case 'i': { static const uint8_t g[5] = {0b010,0b000,0b010,0b010,0b010}; return g[row]; }
    case 'l': { static const uint8_t g[5] = {0b110,0b010,0b010,0b010,0b111}; return g[row]; }
    case 'n': { static const uint8_t g[5] = {0b000,0b110,0b101,0b101,0b101}; return g[row]; }
    case 'p': { static const uint8_t g[5] = {0b000,0b110,0b101,0b110,0b100}; return g[row]; }
    case 's': { static const uint8_t g[5] = {0b011,0b100,0b110,0b001,0b110}; return g[row]; }
    case 'u': { static const uint8_t g[5] = {0b000,0b101,0b101,0b101,0b111}; return g[row]; }
    case 'y': { static const uint8_t g[5] = {0b000,0b101,0b111,0b001,0b110}; return g[row]; }

    case '+': { static const uint8_t g[5] = {0b000,0b010,0b111,0b010,0b000}; return g[row]; }
    case ' ': default: return 0;
  }
}

static void drawTinyText(int x, int y, const char *text, uint16_t color) {
  if (!g_lcdOk) return;

  display.fillRect(0, y - 1, LCD_W, 8, C_BLACK);

  int cx = x;
  for (const char *p = text; *p; ++p) {
    for (uint8_t row = 0; row < 5; row++) {
      uint8_t bits = tinyGlyphRow(*p, row);
      for (uint8_t col = 0; col < 3; col++) {
        if (bits & (1 << (2 - col))) {
          display.drawPixel(cx + col, y + row, color);
        }
      }
    }
    cx += 4;
    if (cx > LCD_W - 3) break;
  }
}

static void drawBatteryIconTiny(int x, int y, bool valid, int pct, bool charging) {
  if (!g_lcdOk) return;

  uint16_t c = valid ? (charging ? TFT_CYAN : colorByPercent(pct)) : TFT_RED;

  display.fillRect(x - 1, y - 1, 15, 10, C_BLACK);
  display.drawRect(x, y, 11, 7, valid ? C_WHITE : TFT_RED);
  display.fillRect(x + 11, y + 2, 2, 3, valid ? C_WHITE : TFT_RED);

  if (!valid) {
    display.drawLine(x + 2, y + 1, x + 8, y + 6, TFT_RED);
    display.drawLine(x + 8, y + 1, x + 2, y + 6, TFT_RED);
    return;
  }

  int fillW = map(constrain(pct, 0, 100), 0, 100, 0, 9);
  if (fillW > 0) {
    display.fillRect(x + 1, y + 1, fillW, 5, c);
  }
}

static void drawChargeIconTiny(int x, int y, bool show) {
  if (!g_lcdOk) return;

  display.fillRect(x - 1, y - 1, 9, 11, C_BLACK);
  if (!show) return;

  display.fillTriangle(x + 4, y + 0, x + 0, y + 5, x + 4, y + 5, TFT_YELLOW);
  display.fillTriangle(x + 3, y + 4, x + 7, y + 4, x + 2, y + 10, TFT_YELLOW);
  display.drawLine(x + 4, y + 0, x + 0, y + 5, TFT_YELLOW);
  display.drawLine(x + 7, y + 4, x + 2, y + 10, TFT_YELLOW);
}

static bool batteryUsbTextMode() {
  // 1.47 style on the 0.96 UI:
  // If the battery has not been physically confirmed, display red USB PWR.
  // Do not display the measured floating VBAT voltage as a fake battery percent.
  return !g_bat.valid;
}

static void updateBatteryUiSnapshot() {
  uint32_t now = millis();

  bool newUsb = batteryUsbTextMode();
  bool newValid = g_bat.valid && !newUsb;
  bool newCharging = newValid && g_bat.charging;
  int newPct = newValid ? constrain(g_bat.percent, 0, 100) : -1;

  if (!g_batUiInit) {
    g_batUiInit = true;
    g_batUiUsb = newUsb;
    g_batUiValid = newValid;
    g_batUiCharging = newCharging;
    g_batUiPct = newPct;
    g_lastBatUiCommitMs = now;
    return;
  }

  // USB <-> BAT is a real state transition; update immediately.
  if (newUsb != g_batUiUsb || newValid != g_batUiValid) {
    g_batUiUsb = newUsb;
    g_batUiValid = newValid;
    g_batUiCharging = newCharging;
    g_batUiPct = newPct;
    g_lastBatUiCommitMs = now;
    return;
  }

  if (newUsb) {
    // Keep USB PWR stable. Nothing else needs to flicker here.
    return;
  }

  // Percent hysteresis:
  // - ignore +/-1% jitter
  // - commit >=2% only every 1.5s
  // - commit >=4% immediately enough for real insert/charge changes
  int pctDiff = abs(newPct - g_batUiPct);
  if (pctDiff >= 4 || (pctDiff >= 2 && (now - g_lastBatUiCommitMs) > 1500UL)) {
    g_batUiPct = newPct;
    g_lastBatUiCommitMs = now;
  }

  // Charging icon/color debounce. Avoid frequent flash caused by charger status
  // line transitions, but still update within about 1.2s.
  if (newCharging != g_batUiCharging && (now - g_lastBatUiCommitMs) > 1200UL) {
    g_batUiCharging = newCharging;
    g_lastBatUiCommitMs = now;
  }
}

static void drawBatteryRowTiny() {
  if (!g_lcdOk) return;

  bool usbMode = g_batUiUsb;
  bool batValid = g_batUiValid;
  bool charging = g_batUiCharging;
  int pct = g_batUiPct;

  // Clear SYS dynamic area.
  display.fillRect(SECTION_X0, Y_SYS_ROW1 - 2, 72, 18, C_BLACK);

  display.setTextSize(1);
  if (usbMode) {
    display.setTextColor(TFT_RED, C_BLACK);
    display.setCursor(SECTION_X0 + 2, Y_SYS_ROW1);
    display.print("USB PWR");
  } else {
    display.setTextColor(C_WHITE, C_BLACK);
    display.setCursor(SECTION_X0 + 2, Y_SYS_ROW1);
    display.print("BAT");

    drawBatteryIconTiny(SECTION_X0 + 24, Y_SYS_ROW1 - 1, batValid, pct, charging);

    uint16_t pc = batValid ? (charging ? TFT_CYAN : colorByPercent(pct)) : TFT_RED;
    display.setTextColor(pc, C_BLACK);
    display.setCursor(SECTION_X0 + 41, Y_SYS_ROW1);
    if (batValid) {
      display.print(pct);
      display.print("%");
    } else {
      display.print("NO");
    }

    drawChargeIconTiny(SECTION_X1 - 8, Y_SYS_ROW1 - 2, batValid && charging);
  }

  // BL row is independent and always shown.
  display.fillRect(SECTION_X0, Y_SYS_ROW2 - 1, 48, 9, C_BLACK);
  display.setTextColor(g_blOn ? TFT_CYAN : TFT_RED, C_BLACK);
  display.setCursor(SECTION_X0 + 2, Y_SYS_ROW2);
  display.print("BL ");
  display.print(g_blOn ? "ON" : "OFF");
}

// ========================= LCD/UI =========================

static bool initLcd() {
  applyBacklight();

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
  Serial.println("[LCD] OK 0.96 ST7789, flat dashboard UI");
  return true;
}

static void drawSectionLabel(int x, int y, uint16_t color, const char *label) {
  if (!g_lcdOk) return;
  display.setTextSize(1);
  display.setTextColor(color, C_BLACK);
  display.setCursor(x, y);
  display.print(label);
}

static void drawSectionDivider(int y, uint16_t color) {
  if (!g_lcdOk) return;
  display.drawFastHLine(SECTION_X0 + 1, y, 70, color);
}

static void drawVuFrame() {
  if (!g_lcdOk) return;
  display.drawRoundRect(VU_X, VU_Y, VU_W, VU_H, 2, C_GREEN);
}

static void drawHeader() {
  if (!g_lcdOk) return;

  display.fillRect(0, 0, LCD_W, 32, C_BLACK);

  display.setTextSize(2);
  display.setTextColor(C_GREEN, C_BLACK);
  display.setCursor(6, ROW_TITLE);
  display.print(g_headerSeeed ? "XIAO" : "Hello");

  display.setTextSize(1);
  display.setTextColor(TFT_CYAN, C_BLACK);
  display.setCursor(4, ROW_SUB);
  display.print("0.96 Display");

  display.drawFastHLine(5, 31, 70, C_WHITE);
}

static void drawStaticLayout() {
  if (!g_lcdOk) return;

  display.fillScreen(C_BLACK);
  cleanScreenEdges();
  drawHeader();

  drawSectionLabel(SECTION_X0 + 1, Y_SYS_LABEL, TFT_CYAN, "SYSTEM");
  drawSectionDivider(Y_SYS_DIV, C_WHITE);

  drawSectionLabel(SECTION_X0 + 1, Y_MOTION_LABEL, TFT_YELLOW, "MOTION");
  drawSectionDivider(Y_MOTION_DIV, C_WHITE);

  drawSectionLabel(SECTION_X0 + 1, Y_MIC_LABEL, C_GREEN, "MIC");
  drawVuFrame();

  display.setTextSize(1);
  display.setTextColor(TFT_YELLOW, C_BLACK);
  display.setCursor(SECTION_X0 + 1, Y_ACC); display.print("A");
  display.setCursor(SECTION_X0 + 1, Y_GYR); display.print("G");

  display.drawFastHLine(5, 147, 70, C_WHITE);
  drawTinyText(6, ROW_FOOT, "XIAO nRF52840+", TFT_YELLOW);

  cache_header = "";
  cache_sys = "";
  cache_bl = "";
  cache_acc = "";
  cache_gyr = "";
  cache_tap = "";
  cache_btn = "";
  cache_raw = "";
  g_cachedVuSegments = -1;
  g_cachedVuWidth = -1;
  g_cachedVuColor = 0xFFFF;
}

// ========================= IMU =========================

static constexpr uint8_t LSM6DS3_ADDR        = 0x6A;
static constexpr uint8_t REG_TAP_SRC        = 0x1C;
static constexpr uint8_t REG_CTRL1_XL       = 0x10;
static constexpr uint8_t REG_TAP_CFG        = 0x58;
static constexpr uint8_t REG_TAP_THS_6D     = 0x59;
static constexpr uint8_t REG_INT_DUR2       = 0x5A;
static constexpr uint8_t REG_WAKE_UP_THS    = 0x5B;
static constexpr uint8_t REG_MD1_CFG        = 0x5E;

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

void imuIntIsr() { g_imuIntFlag = true; }

static bool initImuDoubleTap() {
  pinMode(IMU_INT_PIN, INPUT_PULLDOWN);
  int beginResult = myIMU.begin();
  bool ok = (beginResult == 0);
  ok &= imuWriteReg(REG_CTRL1_XL, 0x60);    // accel 416Hz, +/-2g
  ok &= imuWriteReg(REG_TAP_CFG, 0x8E);     // embedded interrupt + X/Y/Z tap
  ok &= imuWriteReg(REG_TAP_THS_6D, 0x0C);  // threshold
  ok &= imuWriteReg(REG_INT_DUR2, 0x7F);    // timing
  ok &= imuWriteReg(REG_WAKE_UP_THS, 0x80); // single/double tap mode
  ok &= imuWriteReg(REG_MD1_CFG, 0x08);     // double tap to INT1
  uint8_t dummy = 0;
  imuReadReg(REG_TAP_SRC, dummy);
  attachInterrupt(digitalPinToInterrupt(IMU_INT_PIN), imuIntIsr, RISING);
  g_imuOk = ok;
  Serial.print("[IMU] LSM6DS3 double tap D9 ");
  Serial.println(ok ? "OK" : "FAILED");
  return ok;
}

static void updateImu() {
  if (!g_imuOk) return;
  g_ax = myIMU.readFloatAccelX();
  g_ay = myIMU.readFloatAccelY();
  g_az = myIMU.readFloatAccelZ();
  g_gx = myIMU.readFloatGyroX();
  g_gy = myIMU.readFloatGyroY();
  g_gz = myIMU.readFloatGyroZ();
}

static void handleImuTapEvent() {
  bool shouldCheck = false;
  noInterrupts();
  if (g_imuIntFlag) { g_imuIntFlag = false; shouldCheck = true; }
  interrupts();
  if (digitalRead(IMU_INT_PIN) == HIGH) shouldCheck = true;
  if (!shouldCheck) return;
  uint8_t src = 0;
  if (!imuReadReg(REG_TAP_SRC, src)) return;
  if (src & 0x10) {
    uint32_t now = millis();
    if (now - g_lastTapMs > TAP_DEBOUNCE_MS) {
      g_lastTapMs = now;
      g_doubleTapCount++;
      cache_tap = "";
      Serial.print("[TAP] double tap count=");
      Serial.print(g_doubleTapCount);
      Serial.print(" src=0x");
      Serial.println(src, HEX);
    }
  }
}

// ========================= MIC =========================

static uint16_t currentMicPeak() {
  uint32_t now = millis();
  if (now - g_micLastUpdateMs > MIC_DECAY_MS) g_micPeak = (uint16_t)(g_micPeak * 0.75f);
  return g_micPeak;
}

void onPdmReceive() {
  int bytesAvailable = PDM.available();
  if (bytesAvailable <= 0) return;
  if (bytesAvailable > (int)sizeof(g_pdmBuf)) bytesAvailable = sizeof(g_pdmBuf);
  int bytesRead = PDM.read((void *)g_pdmBuf, bytesAvailable);
  if (bytesRead <= 0) return;
  uint32_t peak = 0;
  uint64_t sumSq = 0;
  int samples = bytesRead / 2;
  for (int i = 0; i < samples; i++) {
    int32_t v = g_pdmBuf[i];
    int32_t a = abs(v);
    if ((uint32_t)a > peak) peak = (uint32_t)a;
    sumSq += (uint64_t)((int64_t)v * (int64_t)v);
  }
  uint32_t rms = 0;
  if (samples > 0) rms = (uint32_t)sqrt((double)sumSq / (double)samples);
  g_micPeak = (uint16_t)min<uint32_t>(peak, 65535);
  g_micRms = rms;
  g_micBlocks++;
  g_micLastUpdateMs = millis();
}

static bool initMic() {
  PDM.setPins(PDM_DATA_PIN, PDM_CLK_PIN, -1);
  PDM.onReceive(onPdmReceive);
  PDM.setBufferSize(sizeof(g_pdmBuf));
  PDM.setGain(MIC_GAIN);
  if (!PDM.begin(MIC_CHANNELS, MIC_SAMPLE_RATE_HZ)) {
    Serial.println("[MIC] PDM.begin failed");
    g_micOk = false;
    return false;
  }
  g_micOk = true;
  Serial.println("[MIC] OK PDM DATA=D1 CLK=D0");
  return true;
}

static uint16_t vuColorForSegment(int idx) {
  // 1.14-style block colors: green -> yellow -> red.
  if (idx >= 6) return TFT_RED;
  if (idx >= 4) return TFT_YELLOW;
  return C_GREEN;
}

static int vuLevelToSegments(float level) {
  // v1.2.6: thresholds are intentionally wider than v1.2.
  // This prevents 1-block jitter when the signal sits around a boundary.
  if (level < 0.13f) return 0;
  if (level < 0.25f) return 1;
  if (level < 0.38f) return 2;
  if (level < 0.51f) return 3;
  if (level < 0.64f) return 4;
  if (level < 0.77f) return 5;
  if (level < 0.88f) return 6;
  if (level < 0.96f) return 7;
  return 8;
}

static void drawVuBlocks(int active) {
  if (!g_lcdOk) return;

  active = constrain(active, 0, VU_SEG_COUNT);

  // First draw: clear and draw the frame once.
  if (g_cachedVuSegments < 0) {
    display.fillRect(VU_X, VU_Y, VU_W + 1, VU_H + 1, C_BLACK);
    display.drawRoundRect(VU_X, VU_Y, VU_W, VU_H, 2, C_GREEN);

    for (int i = 0; i < VU_SEG_COUNT; i++) {
      int x = VU_X + 1 + i * (VU_SEG_W + VU_GAP);
      uint16_t c = (i < active) ? vuColorForSegment(i) : C_BLACK;
      display.fillRect(x, VU_Y + 2, VU_SEG_W, VU_H - 4, c);
    }

    g_cachedVuSegments = active;
    g_cachedVuWidth = active;
    g_cachedVuColor = (active > 0) ? vuColorForSegment(active - 1) : C_GREEN;
    g_lastVuRenderMs = millis();
    return;
  }

  // Delta update only. Do not clear/redraw the whole VU each time.
  // This is the key fix for visible flicker.
  if (active > g_cachedVuSegments) {
    for (int i = g_cachedVuSegments; i < active; i++) {
      int x = VU_X + 1 + i * (VU_SEG_W + VU_GAP);
      display.fillRect(x, VU_Y + 2, VU_SEG_W, VU_H - 4, vuColorForSegment(i));
    }
  } else if (active < g_cachedVuSegments) {
    for (int i = active; i < g_cachedVuSegments; i++) {
      int x = VU_X + 1 + i * (VU_SEG_W + VU_GAP);
      display.fillRect(x, VU_Y + 2, VU_SEG_W, VU_H - 4, C_BLACK);
    }
  }

  g_cachedVuSegments = active;
  g_cachedVuWidth = active;
  g_cachedVuColor = (active > 0) ? vuColorForSegment(active - 1) : C_GREEN;
  g_lastVuRenderMs = millis();
}

static int debounceVuDesired(int desired, uint32_t now) {
  desired = constrain(desired, 0, VU_SEG_COUNT);

  int current = g_vuDisplaySegments;
  int delta = desired - current;

  if (desired == current) {
    g_vuCandidateSegments = desired;
    g_vuCandidateCount = 0;
    return current;
  }

  // During decay, raw/RMS may bounce upward by 1~2 blocks.
  // Visually this is the reverse pulse: falling -> suddenly grows -> falls.
  // Suppress only small rebounds. A real loud input can still break through.
  if (delta > 0 && now < g_vuFallGuardUntilMs) {
    // v1.2.6: suppress only tiny +1 rebounds during decay.
    // +2 or larger is treated as a new intentional sound and can rise.
    if (delta <= 1) {
      return current;
    }
  }

  if (desired != g_vuCandidateSegments) {
    g_vuCandidateSegments = desired;
    g_vuCandidateCount = 1;
  } else if (g_vuCandidateCount < 255) {
    g_vuCandidateCount++;
  }

  bool bigJump = abs(delta) >= 3;
  bool strongRise = delta >= 2;
  uint32_t minInterval = (delta > 0) ? 45UL : 80UL;

  if ((now - g_lastVuStepMs) < minInterval && !bigJump && !strongRise) {
    return current;
  }

  uint8_t neededCount = (bigJump || strongRise) ? 1 : 2;
  if (g_vuCandidateCount < neededCount) {
    return current;
  }

  int next = current;

  if (delta > 0) {
    // v1.2.6: rise up to 3 blocks per commit for a snappier response.
    next += min(3, delta);
    g_vuFallGuardUntilMs = 0;
  } else if (delta < 0) {
    next -= 1;
    g_vuFallGuardUntilMs = now + 260UL;
  }

  next = constrain(next, 0, VU_SEG_COUNT);
  g_lastVuStepMs = now;
  return next;
}

static void updateVu() {
  if (!g_lcdOk) return;

  uint16_t peak = currentMicPeak();

  // v1.2.6 block VU:
  // RMS remains the main source. Peak only helps attack slightly.
  float metric = (float)g_micRms * 0.82f + (float)peak * 0.012f;
  if (metric < 0.0f) metric = 0.0f;
  if (metric > 240.0f) metric = 240.0f;

  uint32_t now = millis();

  if (!g_micNoiseReady) {
    if (g_micNoiseStartMs == 0) g_micNoiseStartMs = now;
    g_micNoiseFloor = g_micNoiseFloor * 0.84f + metric * 0.16f;
    if (now - g_micNoiseStartMs > 1200UL) g_micNoiseReady = true;
  } else {
    // Track ambient floor. Do not chase short upward speech spikes.
    if (metric < g_micNoiseFloor) {
      g_micNoiseFloor = g_micNoiseFloor * 0.84f + metric * 0.16f;
    } else if (metric < g_micNoiseFloor + 5.0f) {
      g_micNoiseFloor = g_micNoiseFloor * 0.992f + metric * 0.008f;
    }
  }

  float signal = metric - g_micNoiseFloor - 26.0f;
  if (signal < 0.0f) signal = 0.0f;

  float target = signal / 108.0f;
  if (target > 1.0f) target = 1.0f;

  // v1.2.6: faster attack while keeping fast release.
  if (target > g_vuFast) {
    g_vuFast = g_vuFast * 0.35f + target * 0.65f;
  } else {
    g_vuFast = g_vuFast * 0.22f + target * 0.78f;
  }

  if (g_vuFast > g_vuSmooth) {
    // More responsive visible rise than v1.2.6.
    g_vuSmooth = g_vuSmooth * 0.58f + g_vuFast * 0.42f;
  } else {
    // Keep falling quick.
    g_vuSmooth = g_vuSmooth * 0.34f + g_vuFast * 0.66f;
  }

  if (g_vuSmooth < 0.055f) g_vuSmooth = 0.0f;

  int desired = vuLevelToSegments(g_vuSmooth);
  int next = debounceVuDesired(desired, now);

  if (next == g_cachedVuSegments) return;

  g_vuDisplaySegments = next;
  drawVuBlocks(next);
}

// ========================= UI updates =========================

static String fmtAxisInt(float v) {
  int iv = (int)roundf(v);
  if (iv > 99) iv = 99;
  if (iv < -99) iv = -99;
  char buf[6];
  snprintf(buf, sizeof(buf), "%+d", iv);
  return String(buf);
}

static void updateUiFast() {
  if (!g_lcdOk) return;

  String header = g_headerSeeed ? "XIAO" : "Hello";
  if (header != cache_header) {
    cache_header = header;
    drawHeader();
  }

  updateBatteryUiSnapshot();

  String sys = g_batUiUsb ? String("USB") :
               (String("BAT:") +
                String(g_batUiValid ? g_batUiPct : -1) +
                String(g_batUiValid ? ":V" : ":X") +
                String(g_batUiCharging ? ":C" : ":N"));
  String bl = String(g_blOn ? "ON" : "OFF");

  if (sys != cache_sys || bl != cache_bl) {
    cache_sys = sys;
    cache_bl = bl;
    drawBatteryRowTiny();
  }

  String tap = String("Tap ") + String(g_doubleTapCount);
  if (tap != cache_tap) {
    cache_tap = tap;
    printTextFixed(48, Y_TAP, TFT_YELLOW, tap, 6);
  }

  uint16_t peak = currentMicPeak();
  String raw = String("Raw ") + String((unsigned)peak);
  if (raw != cache_raw) {
    cache_raw = raw;
    uint16_t c = C_WHITE;
    if (g_vuDisplaySegments >= 6) c = TFT_RED;
    else if (g_vuDisplaySegments >= 4) c = TFT_YELLOW;
    printTextFixed(SECTION_X0 + 1, Y_RAW, c, raw, 10);
  }
}

static void updateUiSlow() {
  if (!g_lcdOk) return;
  cleanScreenEdges();

  if (g_imuOk) {
    String acc = String("A ") + fmtAxisInt(g_ax) + " " + fmtAxisInt(g_ay) + " " + fmtAxisInt(g_az);
    if (acc != cache_acc) {
      cache_acc = acc;
      printTextFixed(SECTION_X0 + 1, Y_ACC, C_WHITE, acc, 11);
    }

    String gyr = String("G ") + fmtAxisInt(g_gx / 10.0f) + " " + fmtAxisInt(g_gy / 10.0f) + " " + fmtAxisInt(g_gz / 10.0f);
    if (gyr != cache_gyr) {
      cache_gyr = gyr;
      printTextFixed(SECTION_X0 + 1, Y_GYR, C_WHITE, gyr, 11);
    }
  } else {
    if (cache_acc != "NOIMU") {
      cache_acc = "NOIMU";
      cache_gyr = "";
      printTextFixed(SECTION_X0 + 1, Y_ACC, TFT_RED, "IMU NO", 11);
      printTextFixed(SECTION_X0 + 1, Y_GYR, TFT_RED, "", 11);
    }
  }
}

static void printSerialStatus() {
  Serial.print("[DASH096] bat="); Serial.print(g_bat.vbat, 3);
  Serial.print("V pct="); Serial.print(g_bat.percent);
  Serial.print(" valid="); Serial.print(g_bat.valid ? "Y" : "N");
  Serial.print(" chg="); Serial.print(g_bat.charging ? "Y" : "N");
  Serial.print(" batState="); Serial.print(batStateName(g_batState));
  Serial.print(" filt="); Serial.print(g_batFilterState);
  Serial.print(" raw="); Serial.print(g_bat.raw);
  Serial.print(" spread="); Serial.print((int)(g_bat.rawMax - g_bat.rawMin));
  Serial.print(" chgRaw="); Serial.print(g_chgRawLow ? "L" : "H");
  Serial.print(" ins="); Serial.print(g_insertCandidateStreak);
  Serial.print(" imu="); Serial.print(g_imuOk ? "OK" : "NO");
  Serial.print(" acc=("); Serial.print(g_ax, 2); Serial.print(","); Serial.print(g_ay, 2); Serial.print(","); Serial.print(g_az, 2); Serial.print(")");
  Serial.print(" gyr=("); Serial.print(g_gx, 2); Serial.print(","); Serial.print(g_gy, 2); Serial.print(","); Serial.print(g_gz, 2); Serial.print(")");
  Serial.print(" micPeak="); Serial.print((unsigned)currentMicPeak());
  Serial.print(" micRms="); Serial.print((unsigned long)g_micRms);
  Serial.print(" micFloor="); Serial.print(g_micNoiseFloor, 1);
  Serial.print(" vu="); Serial.print(g_vuSmooth, 2);
  Serial.print(" fast="); Serial.print(g_vuFast, 2);
  Serial.print(" vuSeg="); Serial.print(g_vuDisplaySegments);
  Serial.print(" vuCand="); Serial.print(g_vuCandidateSegments);
  Serial.print("/"); Serial.print(g_vuCandidateCount);
  Serial.print(" fallGuard="); Serial.print((long)(g_vuFallGuardUntilMs - millis()));
  Serial.print(" usr1="); Serial.print(g_btnA ? "P" : "R");
  Serial.print(" usr2="); Serial.print(g_btnB ? "P" : "R");
  Serial.print(" tap="); Serial.print((unsigned long)g_doubleTapCount);
  Serial.print(" bl="); Serial.print(g_blOn ? "ON" : "OFF");
  Serial.print(" frame="); Serial.println((unsigned long)g_frameCounter++);
}

// ========================= Arduino =========================

void setup() {
  Serial.begin(115200);
  delay(800);
  pinMode(BTN_A_PIN, INPUT_PULLUP);
  pinMode(BTN_B_PIN, INPUT_PULLUP);
  pinMode(LCD_BL_PIN, OUTPUT);
  g_btnARaw = digitalRead(BTN_A_PIN);
  g_btnBRaw = digitalRead(BTN_B_PIN);
  g_btnALastRawPressed = (g_btnARaw == LOW);
  g_btnBLastRawPressed = (g_btnBRaw == LOW);
  g_btnA = g_btnALastRawPressed;
  g_btnB = g_btnBLastRawPressed;
  g_btnAArmed = !g_btnA;
  g_btnBArmed = !g_btnB;
  g_btnALastChangeMs = millis();
  g_btnBLastChangeMs = millis();
  attachInterrupt(digitalPinToInterrupt(BTN_A_PIN), btnAIrqIsr, FALLING);
  attachInterrupt(digitalPinToInterrupt(BTN_B_PIN), btnBIrqIsr, FALLING);
  Wire.begin();
  nrf_gpio_cfg_input(CHG_P0_PIN, NRF_GPIO_PIN_PULLUP);
  sampleChargingRawLow();
  analogReadResolution(ADC_BITS);
  disableBatteryDivider();

  Serial.println();
  Serial.println("=== XIAO nRF52840 Plus 0.96 Factory Dashboard v1.2.6.1.1 ===");
  Serial.println("[LCD] ST7789 80x160 BGR rot2 (offset/invert by Config)");
  Serial.println("[BTN] USR1=BL ON/OFF, USR2=Header Hello/XIAO");
  Serial.print("[PIN] PIN_VBAT="); Serial.println(PIN_VBAT);

  g_micNoiseStartMs = millis();
  g_micNoiseReady = false;
  g_micNoiseFloor = 65.0f;
  g_vuSmooth = 0.0f;
  g_vuFast = 0.0f;
  g_vuDisplaySegments = 0;
  g_vuCandidateSegments = 0;
  g_vuCandidateCount = 0;
  g_vuFallGuardUntilMs = 0;
  g_cachedVuSegments = -1;
  g_cachedVuWidth = -1;
  g_cachedVuColor = 0xFFFF;
  g_lastVuStepMs = millis();
  g_lastVuRenderMs = millis();

  initLcd();
  drawStaticLayout();
  initMic();
  initImuDoubleTap();
  updateBattery();
  updateBatteryUiSnapshot();
  cache_sys = "";
  drawBatteryRowTiny();
  updateImu();
  updateUiFast();
  updateUiSlow();
  updateVu();
  printSerialStatus();
}

void loop() {
  uint32_t now = millis();
  handleImuTapEvent();
  updateButtons();
  handleButtonActions();
  if (now - g_lastBatMs >= BAT_REFRESH_MS) { g_lastBatMs = now; updateBattery(); }
  if (now - g_lastVuMs >= UI_VU_MS) { g_lastVuMs = now; updateVu(); }
  if (now - g_lastFastMs >= UI_FAST_MS) { g_lastFastMs = now; updateUiFast(); }
  if (now - g_lastSlowMs >= UI_SLOW_MS) { g_lastSlowMs = now; updateImu(); updateUiSlow(); }
  if (now - g_lastSerialMs >= SERIAL_MS) { g_lastSerialMs = now; printSerialStatus(); }
  delay(2);
}
