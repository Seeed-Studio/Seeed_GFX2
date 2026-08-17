/**
 * Product: XIAO 1.47 inch Touch Display (JD9853A 172x320 + AXS5106L touch)
 * Display: JD9853A 172x320, BGR, capacitive touch
 * Target:  XIAO nRF52840 Plus (RST=38, BL=37). For ESP32-S3 Plus use the sibling
 *          ESP32-S3 Plus folder (RST=13, BL=12).
 * Wiring:  CS=D2, DC=D3, MOSI=D10, SCLK=D8; RST=38, BL=37. Touch SDA=D4, SCL=D5, INT=D7
 *          (I2C 0x63, RST shared with LCD).
 * Demo:    nRF52840 factory dashboard v2.7: SD/touch/IMU/mic/battery/backlight cards + VU +
 *          double-tap counter; event-driven battery, BL PWM cycle, LSM6DS3 double-tap IRQ on D14.
 *
 * Ported from XIAO-Display-Board-main (0522_DashBoard_147_nRF52840, Arduino_GFX). The Seeed_GFX
 * Board/Config templates replace the original Arduino_SWSPI+Arduino_ST7789 bus/panel setup, the
 * manual MADCTL 0x36/0x48 fix, and the manual hard-reset/backlight-on sequence. axs5106l_device
 * (touch_init/get_touch_data) is replaced by Touch_AXS5106L + display.attachTouch; the
 * axs5106l_device companions are dropped. Kept verbatim: PDM (PDM.setPins(D1,D0,-1)), SdFat
 * (SdSpiConfig SHARED_SPI) + acquireForLcd/acquireForSd shared-SPI arbitration, SparkFunLSM6DS3
 * double-tap on D14, nRF battery (NRF_P0->/PIN_VBAT), analogWrite BL, attachInterrupt. Note:
 * RGB565_LIGHTGREEN has no TFT_ equivalent in Seeed_GFX, so it is kept as raw 0x9792.
 */

#include <Seeed_GFX.h>
#include "board/boards/XIAO_LCD_Board.h"
#include "driver/tft/Driver_JD9853A.h"
#include "panel/Panel_TFT.h"
#include "touch/Touch_AXS5106L.h"
#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <SdFat.h>
#include <PDM.h>
#include "SparkFunLSM6DS3.h"
#include <nrf.h>
#include <nrf_gpio.h>
#include <stdarg.h>
#include <math.h>

Seeed_GFX display;
Touch_AXS5106L touch(-1, D7, Wire, 172, 320);   // RST=-1: shared with LCD, reset by display.begin<>; INT=D7

// ========================= Pin map =========================

// static constexpr uint8_t PDM_CLK_PIN   = D0;
// static constexpr uint8_t PDM_DATA_PIN  = D1;

static constexpr uint8_t LCD_CS_PIN    = D2;
static constexpr uint8_t LCD_DC_PIN    = D3;
static constexpr uint8_t I2C_SDA_PIN   = D4;
static constexpr uint8_t I2C_SCL_PIN   = D5;
static constexpr uint8_t SD_CS_PIN     = D6;
static constexpr uint8_t TOUCH_INT_PIN = D7;
static constexpr uint8_t LCD_SCK_PIN   = D8;
static constexpr uint8_t IMU_INT_PIN   = D14; // LSM6DS3 INT1, used for double-tap wake/count.
static constexpr uint8_t LCD_MOSI_PIN  = D10;

// Seeed_GFX Board template drives RST+BL on the XIAO nRF52840 Plus (RST=38, BL=37).
static constexpr int8_t  LCD_RST_PIN   = 38;  // XIAO nRF52840 Plus
static constexpr int8_t  LCD_BL_PIN    = 37;  // BL PWM via analogWrite (runtime backlight control)

// User buttons on the 1.47 touch carrier (kept from the verified v0.2 wiring).
static constexpr uint8_t BTN_B_PIN     = D15;
static constexpr uint8_t BTN_A_PIN     = D19;

// nRF52840 Plus internal battery measurement pins from schematic.
static constexpr uint8_t READ_BAT_P0_PIN = 14; // P0.14 / ~READ_BAT, active-low sink enable.
static constexpr uint8_t CHG_P0_PIN      = 17; // P0.17 / ~CHG, active-low charging indication.

#ifndef PIN_VBAT
// In the tested Seeeduino nRF52 BSP, PIN_VBAT was printed as 35.
// Keep fallback so the file still compiles if the macro is missing.
#define PIN_VBAT 35
#endif

// ========================= Timing =========================

static constexpr uint32_t UI_TEXT_FAST_MS   = 130;
static constexpr uint32_t UI_TEXT_SLOW_MS   = 220;
static constexpr uint32_t UI_VU_MS          = 75;
static constexpr uint32_t UI_MIC_TEXT_MS    = 360;
static constexpr uint32_t SERIAL_MS         = 500;
static constexpr uint32_t SD_REFRESH_MS     = 1200;
static constexpr uint32_t BAT_REFRESH_MS    = 350;
static constexpr uint32_t TAP_DEBOUNCE_MS   = 220;
// Button debounce:
 // - BTN_DEBOUNCE_MS filters contact bounce.
 // - BTN_ACTION_LOCKOUT_MS prevents accidental double action from one long/noisy press.
static constexpr uint32_t BTN_DEBOUNCE_MS = 18;
static constexpr uint32_t BTN_ACTION_LOCKOUT_MS = 70;

// Backlight control:
//   USR1 short press: cycle brightness levels: 100% -> 75% -> 50% -> 25% -> 0% -> 100%
//   USR2 short press: toggle screen off / restore previous brightness
static const uint8_t BL_LEVELS[] = {255, 191, 128, 64, 0};
static constexpr uint8_t BL_LEVEL_COUNT = sizeof(BL_LEVELS) / sizeof(BL_LEVELS[0]);
static constexpr uint32_t MIC_DECAY_MS      = 60;

// ========================= Mic =========================

static constexpr int MIC_SAMPLE_RATE_HZ = 16000;
static constexpr int MIC_CHANNELS       = 1;
static constexpr int MIC_GAIN           = 30;

// ========================= Buttons =========================

static constexpr int BTN_A_ACTIVE_LEVEL = LOW;
static constexpr int BTN_B_ACTIVE_LEVEL = LOW;

// ========================= Battery =========================
// XIAO nRF52840 Plus schematic:
//   R16 = 1M, R17 = 499K.
//   ratio = (1000K + 499K) / 499K = 3.004.
// We verified in diagnostics:
//   PIN_VBAT pin=35, VADC around 1.28V, VBAT around 3.85V.

static constexpr float BAT_DIVIDER_RATIO = (1000.0f + 499.0f) / 499.0f;
static constexpr float BAT_CAL_FACTOR    = 1.000f;
static constexpr int ADC_BITS            = 12;
static constexpr int ADC_MAX             = (1 << ADC_BITS) - 1;
static constexpr float ADC_FULL_SCALE_V  = 3.600f;

// A real Li-ion cell is a low-impedance source, so samples are usually stable.
// With no battery inserted but USB plugged in, the charger/VBAT node can float near
// 4V and still produce a fake battery voltage. Detect that case by sample spread.
static constexpr uint16_t BAT_PRESENT_MIN_RAW = 80;
static constexpr uint16_t BAT_FLOAT_RANGE_RAW = 80;

// Avoid high-impedance ADC noise causing visible NO BAT flicker.
// Once a real battery has been detected, noisy-but-plausible readings are held/filtered.
// Missing battery is declared only after many consecutive bad batches.
static constexpr uint8_t BAT_INVALID_CONFIRM_COUNT = 3;
static constexpr uint8_t BAT_NOISY_HOLD_CONFIRM_COUNT = 3;
static constexpr float BAT_VALID_MIN_V = 2.80f;
static constexpr float BAT_VALID_MAX_V = 4.60f;
// If spread is noisy but the averaged voltage is close to the last good cell voltage,
// keep it as a real battery. If the average jumps far while spread is huge, treat it
// as battery removed / charger floating.
static constexpr float BAT_NOISY_CLOSE_DELTA_V = 0.08f;

// Event-driven battery presence detection.
// We do NOT trust static VBAT voltage under USB-C, because the charger BAT node can
// look like a real 3.7V Li-ion cell even with no battery attached.
// Instead, under USB-C we first learn a USB-only baseline, then confirm battery insert
// only after a CHG edge, spread improvement, or a meaningful VBAT shift.
static constexpr uint16_t BAT_STABLE_PRESENT_SPREAD_RAW = 30;
static constexpr uint8_t BAT_INSERT_CONFIRM_COUNT = 2;
static constexpr uint8_t BAT_REMOVE_CONFIRM_COUNT = 4;
static constexpr float BAT_INSERT_DELTA_V = 0.10f;
static constexpr float BAT_REMOVE_DELTA_V = 0.14f;

// When USB-C is plugged into an already battery-powered board, charger STAT and
// VBAT ADC can glitch for a few refresh cycles. Do not classify this as battery removal.
static constexpr uint32_t BAT_CHG_TRANSIENT_HOLD_MS = 900;
static constexpr uint8_t BAT_PRESENT_NOISY_HOLD_COUNT = 4;

// Charger status filter.
// ~CHG is active-low. Set charging immediately when LOW is stable.
// Clear it after a few stable HIGH samples so unplugging USB-C is reflected quickly.
static constexpr uint8_t CHG_SAMPLE_COUNT = 9;
static constexpr uint8_t CHG_HIGH_CLEAR_COUNT = 1;

// ========================= Colors =========================

static constexpr uint16_t C_BLACK   = TFT_BLACK;
static constexpr uint16_t C_WHITE   = TFT_WHITE;
static constexpr uint16_t C_GREEN   = 0x9792;  // orig RGB565_LIGHTGREEN (144,240,144); no TFT_LIGHTGREEN in Seeed_GFX
static constexpr uint16_t C_RED     = TFT_RED;
static constexpr uint16_t C_CYAN    = TFT_CYAN;
static constexpr uint16_t C_YELLOW  = TFT_YELLOW;
static constexpr uint16_t C_GRAY    = 0x8410;
static constexpr uint16_t C_ORANGE  = 0xFD20;
static constexpr uint16_t C_DIM     = 0x2104;
static constexpr uint16_t C_PANEL   = 0x0841;
static constexpr uint16_t C_LINE    = 0x39E7;
static constexpr uint16_t C_BLUE    = TFT_BLUE;

// ========================= Devices =========================

SdFat SD;
LSM6DS3 myIMU(I2C_MODE, 0x6A);

// ========================= Runtime state =========================

volatile uint16_t g_micPeak = 0;
volatile uint32_t g_micLastUpdateMs = 0;
int16_t g_pdmBuf[256];

bool g_sdMounted = false;
uint32_t g_sdOkFreq = 0;

float g_ax = 0, g_ay = 0, g_az = 0;
float g_gx = 0, g_gy = 0, g_gz = 0;

bool g_touchValid = false;
int g_touchX = -1;
int g_touchY = -1;

bool g_btnA = false;
bool g_btnB = false;
int g_btnARaw = HIGH;
int g_btnBRaw = HIGH;

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
uint8_t g_batInvalidStreak = 0;
uint8_t g_batValidStreak = 0;
uint8_t g_batNoisyStreak = 0;
const char *g_batFilterState = "BOOT";

bool g_chgRawLow = false;
bool g_chgState = false;
uint8_t g_chgHighStreak = 0;
uint32_t g_lastChgLowMs = 0;

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
bool g_chgRawInitialized = false;
uint32_t g_lastChgRawChangeMs = 0;
uint8_t g_insertCandidateStreak = 0;
uint8_t g_removeCandidateStreak = 0;

uint32_t g_lastFastUiMs = 0;
uint32_t g_lastSlowUiMs = 0;
uint32_t g_lastVuMs = 0;
uint32_t g_lastMicTextMs = 0;
uint32_t g_lastSerialMs = 0;
uint32_t g_lastSdMs = 0;
uint32_t g_lastBatMs = 0;

String cache_sd = "";
String cache_touch = "";
String cache_bat = "";
int cache_chargeIcon = -1;
int cache_batIconState = -1;
String cache_acc = "";
String cache_gyr = "";
String cache_btn1 = "";
String cache_btn2 = "";
String cache_tap = "";
String cache_bl = "";
String cache_mic = "";
String cache_footer = "";

float g_vuSmooth = 0.0f;
int cache_vuSegments = -1;

volatile bool g_imuIntFlag = false;
uint32_t g_doubleTapCount = 0;
uint32_t g_lastTapMs = 0;

uint8_t g_blIndex = 0;          // 0 = 255, full brightness
uint8_t g_blRestoreIndex = 0;   // brightness restored after screen-off

bool g_btnALastRawPressed = false;
bool g_btnBLastRawPressed = false;
uint32_t g_btnALastChangeMs = 0;
uint32_t g_btnBLastChangeMs = 0;
bool g_btnAPressEvent = false;
bool g_btnBPressEvent = false;
uint32_t g_lastBtnActionMs = 0;

// Button responsiveness:
// Polling alone can miss a short press while SD/Battery/LCD tasks are blocking.
// Use FALLING edge IRQ as a latch, then updateButtons() consumes that latch safely.
volatile uint8_t g_btnAIrqCount = 0;
volatile uint8_t g_btnBIrqCount = 0;

// Pending action latches. A press that arrives during SD/BAT/LCD work or during
// the action lockout window is kept here and executed as soon as it is safe.
// This avoids the "pressed but no response, press again" feeling.
bool g_btnAPendingAction = false;
bool g_btnBPendingAction = false;

uint32_t g_btnALastQueueMs = 0;
uint32_t g_btnBLastQueueMs = 0;
uint32_t g_btnALastStateEmitMs = 0;
uint32_t g_btnBLastStateEmitMs = 0;

// ========================= UI layout =========================

static constexpr int CARD_X = 7;
static constexpr int CARD_W = 158;

// Header
static constexpr int Y_TITLE = 8;
static constexpr int Y_SUB1  = 32;

// Cards
static constexpr int Y_SYS  = 54;
static constexpr int H_SYS  = 66;

static constexpr int Y_MOTION = 125;
static constexpr int H_MOTION = 58;

static constexpr int Y_MIC  = 189;
static constexpr int H_MIC  = 62;

static constexpr int Y_BTN  = 258;
static constexpr int H_BTN  = 48;

static constexpr int Y_PRODUCT = 311;

// Dynamic text rows
static constexpr int ROW_SD     = Y_SYS + 23;
static constexpr int ROW_TOUCH  = Y_SYS + 38;
static constexpr int ROW_BAT    = Y_SYS + 53;

// Small charging icon on the BAT row.
// It is drawn as pixels/triangles instead of a Unicode character because
// the built-in GFX font does not reliably support the ⚡ glyph.
static constexpr int BAT_ICON_X = 62;
static constexpr int BAT_ICON_Y = ROW_BAT - 2;
static constexpr int BAT_ICON_W = 22;
static constexpr int BAT_ICON_H = 12;

static constexpr int CHG_ICON_X = 148;
static constexpr int CHG_ICON_Y = ROW_BAT - 2;
static constexpr int CHG_ICON_W = 12;
static constexpr int CHG_ICON_H = 14;

static constexpr int ROW_ACC    = Y_MOTION + 23;
static constexpr int ROW_GYR    = Y_MOTION + 39;

static constexpr int ROW_MIC_RAW = Y_MIC + 42;

static constexpr int ROW_BTN1   = Y_BTN + 20;
static constexpr int ROW_BTN2   = Y_BTN + 20;
static constexpr int ROW_BL     = Y_BTN + 34;
static constexpr int ROW_TAP_MOTION = Y_MOTION + 8;

// VU meter
static constexpr int VU_X = CARD_X + 16;
static constexpr int VU_Y = Y_MIC + 27;
static constexpr int VU_W = 124;
static constexpr int VU_H = 14;
static constexpr int VU_SEG_COUNT = 12;
static constexpr int VU_GAP = 2;
static constexpr int VU_SEG_W = (VU_W - 4 - (VU_SEG_COUNT - 1) * VU_GAP) / VU_SEG_COUNT;

// ========================= Basic helpers =========================

static void logf(const char *fmt, ...) {
  char buf[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  Serial.print(buf);
}

static String padRight(const String &s, int width) {
  String out = s;
  while ((int)out.length() < width) out += ' ';
  if ((int)out.length() > width) out = out.substring(0, width);
  return out;
}

static uint16_t colorByPercent(int pct) {
  if (pct <= 15) return C_RED;
  if (pct <= 35) return C_YELLOW;
  return C_GREEN;
}

static int lipoPercent(float v) {
  struct Point { float v; int p; };
  static const Point table[] = {
    {4.20f, 100},
    {4.10f, 90},
    {4.00f, 80},
    {3.92f, 70},
    {3.85f, 60},
    {3.79f, 50},
    {3.72f, 40},
    {3.66f, 30},
    {3.58f, 20},
    {3.50f, 10},
    {3.30f, 0}
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

// ========================= Shared SPI helpers =========================

static void acquireForLcd() {
  // Bus_SPI owns LCD_CS (D2) and parks it HIGH between transactions; only SD_CS is
  // idled here to keep the SD card off the shared SPI bus before an LCD op. The old
  // case-layer pinMode/digitalWrite on LCD_CS before every draw broke nRF52840 SPIM
  // transactions (rectify #72 pattern — screen lights but no image). Do not re-add it.
  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH);
  delayMicroseconds(2);
}

static void acquireForSd() {
  // Bus_SPI parks LCD_CS HIGH after each LCD transaction; only SD_CS needs idling.
  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH);
  delayMicroseconds(2);
}

// ========================= LCD low-level =========================
// Seeed_GFX Board_XIAO_1inch47_Touch_Display<38,37> + Config_XIAO_1inch47_Touch_JD9853A
// own the panel: hard-reset (RST), backlight-on (BL), MADCTL, and BGR/invert are baked by
// the Config. Only runtime backlight PWM and the clear screen remain here.

static bool initLcd() {
  if (!display.begin<Board_XIAO_1inch47_Touch_Display<LCD_RST_PIN, LCD_BL_PIN>,
                     Config_XIAO_1inch47_Touch_JD9853A>()) {
    Serial.println(display.lastResult().message);
    return false;
  }

  // Board template turned BL on; reapply the runtime PWM level (starts at 255 = full).
  applyBacklight();
  display.fillScreen(C_BLACK);
  return true;
}

static void printTextFixed(int x, int y, uint16_t color, const String &text, int widthChars) {
  acquireForLcd();
  display.setCursor(x, y);
  display.setTextSize(1);
  display.setTextColor(color, C_BLACK);
  display.print(padRight(text, widthChars));
}


static void updateUiFast();

// ========================= Button IRQ / debounce =========================

static void queueButtonA(uint32_t now) {
  if (now - g_btnALastQueueMs < BTN_DEBOUNCE_MS) return;
  g_btnALastQueueMs = now;
  g_btnAPendingAction = true;
}

static void queueButtonB(uint32_t now) {
  if (now - g_btnBLastQueueMs < BTN_DEBOUNCE_MS) return;
  g_btnBLastQueueMs = now;
  g_btnBPendingAction = true;
}

void btnAIrqIsr() {
  // Saturating counter: one or more IRQs before loop() resumes still become one
  // queued action after debounce. Count is used instead of a bool so an IRQ cannot
  // be lost if it arrives while updateButtons() is clearing the previous flag.
  if (g_btnAIrqCount < 250) g_btnAIrqCount++;
}

void btnBIrqIsr() {
  if (g_btnBIrqCount < 250) g_btnBIrqCount++;
}

// ========================= Backlight =========================

static uint8_t currentBacklightPwm() {
  return BL_LEVELS[g_blIndex];
}

static int currentBacklightPercent() {
  return (int)roundf((float)currentBacklightPwm() * 100.0f / 255.0f);
}

static void applyBacklight() {
  pinMode(LCD_BL_PIN, OUTPUT);
  analogWrite(LCD_BL_PIN, currentBacklightPwm());
}

static void cycleBacklightLevel() {
  g_blIndex = (g_blIndex + 1) % BL_LEVEL_COUNT;
  if (currentBacklightPwm() > 0) {
    g_blRestoreIndex = g_blIndex;
  }
  applyBacklight();
}

static void toggleBacklightOffRestore() {
  if (currentBacklightPwm() == 0) {
    if (BL_LEVELS[g_blRestoreIndex] == 0) g_blRestoreIndex = 0;
    g_blIndex = g_blRestoreIndex;
  } else {
    g_blRestoreIndex = g_blIndex;
    g_blIndex = BL_LEVEL_COUNT - 1; // last level is 0
  }
  applyBacklight();
}

static void handleBacklightButtons() {
  // Fold compatibility press events into the new pending-action latches.
  if (g_btnAPressEvent) {
    g_btnAPressEvent = false;
    g_btnAPendingAction = true;
  }
  if (g_btnBPressEvent) {
    g_btnBPressEvent = false;
    g_btnBPendingAction = true;
  }

  if (!g_btnAPendingAction && !g_btnBPendingAction) return;

  uint32_t now = millis();

  // Do NOT clear pending actions during lockout. v2.4 still felt like it missed
  // because legitimate short presses could arrive while the previous action was
  // still cooling down. Here we keep the action pending and execute it next loop.
  if (now - g_lastBtnActionMs < BTN_ACTION_LOCKOUT_MS) return;

  // If both somehow arrive together, prefer USR2 power toggle and clear USR1.
  if (g_btnBPendingAction) {
    g_btnBPendingAction = false;
    g_btnAPendingAction = false;

    toggleBacklightOffRestore();
    g_lastBtnActionMs = now;
    cache_bl = ""; // force immediate UI update for perceived responsiveness
    updateUiFast();

    Serial.print("[BL] USR2 toggle, pwm=");
    Serial.print(currentBacklightPwm());
    Serial.print(" pct=");
    Serial.print(currentBacklightPercent());
    Serial.println(" src=pending");
    return;
  }

  if (g_btnAPendingAction) {
    g_btnAPendingAction = false;

    cycleBacklightLevel();
    g_lastBtnActionMs = now;
    cache_bl = ""; // force immediate UI update for perceived responsiveness
    updateUiFast();

    Serial.print("[BL] USR1 cycle, pwm=");
    Serial.print(currentBacklightPwm());
    Serial.print(" pct=");
    Serial.print(currentBacklightPercent());
    Serial.println(" src=pending");
  }
}

// ========================= UI drawing =========================

static void drawCard(int x, int y, int w, int h, uint16_t accent, const char *title) {
  acquireForLcd();
  display.drawRoundRect(x, y, w, h, 6, accent);
  display.drawRoundRect(x + 1, y + 1, w - 2, h - 2, 6, C_LINE);
  display.fillRect(x + 5, y + 12, 4, h - 24, accent);
  display.setTextSize(1);
  display.setTextColor(accent, C_BLACK);
  display.setCursor(x + 14, y + 8);
  display.print(title);
}

static void drawVuFrame() {
  acquireForLcd();
  display.drawRoundRect(VU_X, VU_Y, VU_W, VU_H, 3, C_GREEN);
  for (int i = 0; i < VU_SEG_COUNT; ++i) {
    int x = VU_X + 2 + i * (VU_SEG_W + VU_GAP);
    display.fillRect(x, VU_Y + 2, VU_SEG_W, VU_H - 4, C_BLACK);
  }
}

static void drawChargeIcon(bool charging) {
  acquireForLcd();

  // Clear icon area first. This also removes the icon when charging stops.
  display.fillRect(CHG_ICON_X, CHG_ICON_Y, CHG_ICON_W, CHG_ICON_H, C_BLACK);

  if (!charging) return;

  // Lightning symbol, compact 12x14 px.
  // Shape:
  //    /|
  //   / |
  //  /__|
  //     /
  //    /
  const int x = CHG_ICON_X;
  const int y = CHG_ICON_Y;

  display.fillTriangle(x + 6, y + 0,  x + 1, y + 7,  x + 6, y + 7,  C_YELLOW);
  display.fillTriangle(x + 5, y + 6,  x + 11, y + 6, x + 4, y + 13, C_YELLOW);

  // Thin orange edge makes it visible on bright blue/cyan BAT text.
  display.drawLine(x + 6, y + 0, x + 1, y + 7, C_ORANGE);
  display.drawLine(x + 1, y + 7, x + 6, y + 7, C_ORANGE);
  display.drawLine(x + 11, y + 6, x + 4, y + 13, C_ORANGE);
}

static void drawBatteryIcon(bool valid, int percent, bool charging) {
  acquireForLcd();

  const int x = BAT_ICON_X;
  const int y = BAT_ICON_Y;

  // Clear the full icon area.
  display.fillRect(x - 1, y - 1, BAT_ICON_W + 5, BAT_ICON_H + 2, C_BLACK);

  uint16_t outline = valid ? C_WHITE : C_GRAY;
  uint16_t fillColor = valid ? colorByPercent(percent) : C_RED;
  if (charging && valid) fillColor = C_CYAN;

  // Battery body and head.
  display.drawRoundRect(x, y, BAT_ICON_W, BAT_ICON_H, 2, outline);
  display.fillRect(x + BAT_ICON_W, y + 4, 3, 4, outline);

  if (!valid) {
    // Cross mark for no battery.
    display.drawLine(x + 4, y + 3, x + BAT_ICON_W - 4, y + BAT_ICON_H - 4, C_RED);
    display.drawLine(x + BAT_ICON_W - 4, y + 3, x + 4, y + BAT_ICON_H - 4, C_RED);
    return;
  }

  int fillW = map(percent, 0, 100, 0, BAT_ICON_W - 4);
  if (fillW < 0) fillW = 0;
  if (fillW > BAT_ICON_W - 4) fillW = BAT_ICON_W - 4;

  if (fillW > 0) {
    display.fillRect(x + 2, y + 2, fillW, BAT_ICON_H - 4, fillColor);
  }
}

static void drawStaticLayout() {
  acquireForLcd();
  display.fillScreen(C_BLACK);

  display.setCursor(8, Y_TITLE);
  display.setTextSize(2);
  display.setTextColor(C_GREEN, C_BLACK);
  display.println("Hello,XIAO!");

  display.setTextSize(1);
  display.setTextColor(C_CYAN, C_BLACK);
  display.setCursor(10, Y_SUB1);
  display.print("1.47 Inch Touch Display");

  display.drawFastHLine(8, 49, 156, C_LINE);

  drawCard(CARD_X, Y_SYS, CARD_W, H_SYS, C_CYAN, "SYSTEM");
  display.setTextColor(C_WHITE, C_BLACK);
  display.setCursor(20, ROW_SD);    display.print("SD");
  display.setCursor(20, ROW_TOUCH); display.print("Touch");
  display.setCursor(20, ROW_BAT);   display.print("BAT");
  drawBatteryIcon(false, 0, false);
  drawChargeIcon(false);

  drawCard(CARD_X, Y_MOTION, CARD_W, H_MOTION, C_YELLOW, "MOTION");
  display.setTextColor(C_YELLOW, C_BLACK);
  display.setCursor(110, ROW_TAP_MOTION); display.print("Tap 0");
  display.setTextColor(C_WHITE, C_BLACK);
  display.setCursor(20, ROW_ACC); display.print("Acc");
  display.setCursor(20, ROW_GYR); display.print("Gyr");

  drawCard(CARD_X, Y_MIC, CARD_W, H_MIC, C_GREEN, "MIC LEVEL");
  drawVuFrame();

  drawCard(CARD_X, Y_BTN, CARD_W, H_BTN, C_BLUE, "BACKLIGHT / BUTTON");
  display.setTextColor(C_WHITE, C_BLACK);
  display.setCursor(20, ROW_BTN1); display.print("Brightness");
  display.setTextColor(C_CYAN, C_BLACK);
  display.setCursor(20, ROW_BL); display.print("USR1 level  USR2 off");

  display.setTextSize(1);
  display.setTextColor(C_YELLOW, C_BLACK);
  display.setCursor(31, Y_PRODUCT);
  display.print("XIAO nRF52840 Plus");

  cache_sd = "";
  cache_touch = "";
  cache_bat = "";
  cache_chargeIcon = -1;
  cache_batIconState = -1;
  cache_acc = "";
  cache_gyr = "";
  cache_btn1 = "";
  cache_btn2 = "";
  cache_tap = "";
  cache_bl = "";
  cache_mic = "";
  cache_footer = "";
  cache_vuSegments = -1;
}

// ========================= Touch + IMU =========================

static bool initTouchAndImu() {
  Wire.begin();
  myIMU.begin();
  return true;
}

static void updateTouch() {
  int32_t x = 0, y = 0;
  if (display.getTouch(&x, &y)) {
    g_touchValid = true;
    g_touchX = (int)x;
    g_touchY = (int)y;
  } else {
    g_touchValid = false;
    g_touchX = -1;
    g_touchY = -1;
  }
}

static void updateImu() {
  g_ax = myIMU.readFloatAccelX();
  g_ay = myIMU.readFloatAccelY();
  g_az = myIMU.readFloatAccelZ();
  g_gx = myIMU.readFloatGyroX();
  g_gy = myIMU.readFloatGyroY();
  g_gz = myIMU.readFloatGyroZ();
}

// ========================= IMU double-tap interrupt =========================
// LSM6DS3 key registers used here:
//   TAP_SRC       0x1C: read to clear tap event; bit4 is DOUBLE_TAP on LSM6DS3 family.
//   TAP_CFG       0x58: enable tap recognition on X/Y/Z and embedded interrupts.
//   TAP_THS_6D    0x59: tap threshold.
//   INT_DUR2      0x5A: shock/quiet/duration timing.
//   WAKE_UP_THS   0x5B: bit7 enables single/double-tap mode.
//   MD1_CFG       0x5E: route double-tap event to INT1.
// If sensitivity is too high/low, tune TAP_THS_6D first.

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

void imuIntIsr() {
  g_imuIntFlag = true;
}

static bool initImuDoubleTap() {
  pinMode(IMU_INT_PIN, INPUT_PULLDOWN);

  // Accelerometer on, 416Hz, +/-2g. SparkFun begin() also configures the IMU,
  // but set it again here so tap detection has a known ODR.
  bool ok = true;
  ok &= imuWriteReg(REG_CTRL1_XL, 0x60);

  // Enable embedded interrupt recognition + tap on X/Y/Z.
  // 0x8E is a common LSM6DS3 tap config: interrupts enabled, X/Y/Z tap axes enabled.
  ok &= imuWriteReg(REG_TAP_CFG, 0x8E);

  // Threshold and timing. These are deliberately medium values for a small handheld board.
  // Lower TAP_THS_6D if double taps are hard to trigger; raise it if false triggers occur.
  ok &= imuWriteReg(REG_TAP_THS_6D, 0x0C);
  ok &= imuWriteReg(REG_INT_DUR2, 0x7F);

  // Enable single/double-tap recognition mode.
  ok &= imuWriteReg(REG_WAKE_UP_THS, 0x80);

  // Route double-tap event to INT1.
  ok &= imuWriteReg(REG_MD1_CFG, 0x08);

  uint8_t dummy = 0;
  imuReadReg(REG_TAP_SRC, dummy); // clear stale event.

  attachInterrupt(digitalPinToInterrupt(IMU_INT_PIN), imuIntIsr, RISING);

  Serial.print("[IMU] double-tap INT1 on D14 init ");
  Serial.println(ok ? "OK" : "FAILED");

  return ok;
}

static void handleImuTapEvent() {
  bool shouldCheck = false;

  noInterrupts();
  if (g_imuIntFlag) {
    g_imuIntFlag = false;
    shouldCheck = true;
  }
  interrupts();

  // Polling fallback: if the interrupt edge was missed, read source while INT is high.
  if (digitalRead(IMU_INT_PIN) == HIGH) {
    shouldCheck = true;
  }

  if (!shouldCheck) return;

  uint8_t src = 0;
  if (!imuReadReg(REG_TAP_SRC, src)) return;

  // TAP_SRC bit4 = double tap. Count one event for one double-tap gesture.
  if (src & 0x10) {
    uint32_t now = millis();
    if (now - g_lastTapMs > TAP_DEBOUNCE_MS) {
      g_lastTapMs = now;
      g_doubleTapCount++;
      Serial.print("[TAP] double tap count=");
      Serial.print(g_doubleTapCount);
      Serial.print(" src=0x");
      Serial.println(src, HEX);
    }
  }
}

// ========================= Mic =========================

static bool initMic() {
  PDM.setPins(D1, D0, -1);
  PDM.onReceive([]() {
    int bytesAvailable = PDM.available();
    if (bytesAvailable <= 0) return;
    if (bytesAvailable > (int)sizeof(g_pdmBuf)) bytesAvailable = sizeof(g_pdmBuf);
    int bytesRead = PDM.read((void *)g_pdmBuf, bytesAvailable);
    if (bytesRead <= 0) return;

    uint16_t peak = 0;
    int samples = bytesRead / 2;
    for (int i = 0; i < samples; ++i) {
      int32_t a = abs((int32_t)g_pdmBuf[i]);
      if (a > peak) peak = (uint16_t)((a > 65535) ? 65535 : a);
    }

    g_micPeak = peak;
    g_micLastUpdateMs = millis();
  });

  PDM.setBufferSize(sizeof(g_pdmBuf));
  PDM.setGain(MIC_GAIN);

  if (!PDM.begin(MIC_CHANNELS, MIC_SAMPLE_RATE_HZ)) {
    Serial.println("[MIC] PDM.begin failed");
    return false;
  }

  return true;
}

static uint16_t currentMicPeak() {
  uint32_t now = millis();
  if (now - g_micLastUpdateMs > MIC_DECAY_MS) {
    g_micPeak = (uint16_t)(g_micPeak * 0.75f);
  }
  return g_micPeak;
}

static uint16_t vuColorForIndex(int idx) {
  if (idx >= 10) return C_RED;
  if (idx >= 7) return C_ORANGE;
  return C_GREEN;
}

static void updateVuMeter() {
  uint16_t micPeak = currentMicPeak();

  float target = (float)micPeak / 2200.0f;
  if (target < 0.0f) target = 0.0f;
  if (target > 1.0f) target = 1.0f;

  if (target > g_vuSmooth) g_vuSmooth = g_vuSmooth * 0.55f + target * 0.45f;
  else g_vuSmooth = g_vuSmooth * 0.86f + target * 0.14f;

  int activeSegs = (int)roundf(g_vuSmooth * VU_SEG_COUNT);
  if (activeSegs < 0) activeSegs = 0;
  if (activeSegs > VU_SEG_COUNT) activeSegs = VU_SEG_COUNT;

  if (activeSegs == cache_vuSegments) return;

  acquireForLcd();
  for (int i = 0; i < VU_SEG_COUNT; ++i) {
    int x = VU_X + 2 + i * (VU_SEG_W + VU_GAP);
    uint16_t color = (i < activeSegs) ? vuColorForIndex(i) : C_BLACK;
    display.fillRect(x, VU_Y + 2, VU_SEG_W, VU_H - 4, color);
  }

  cache_vuSegments = activeSegs;
}

static void updateMicText() {
  uint16_t micPeak = currentMicPeak();
  String micText = String("Raw ") + String((unsigned)micPeak);

  if (micText != cache_mic) {
    cache_mic = micText;
    printTextFixed(20, ROW_MIC_RAW, C_WHITE, micText, 17);
  }
}

// ========================= SD =========================

static bool probeSdMount(uint32_t &okFreq) {
  const uint32_t freqs[] = {400000, 1000000, 4000000, 8000000};

  for (size_t i = 0; i < sizeof(freqs) / sizeof(freqs[0]); ++i) {
    acquireForSd();
    SPI.begin();
    delay(5);

    SdSpiConfig cfg(SD_CS_PIN, SHARED_SPI, freqs[i], &SPI);
    if (SD.begin(cfg)) {
      okFreq = freqs[i];
      return true;
    }
  }

  return false;
}

static void updateSdStatus() {
  uint32_t freq = 0;
  g_sdMounted = probeSdMount(freq);
  if (g_sdMounted) g_sdOkFreq = freq;
}

// ========================= Buttons =========================

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

  bool rawAPressed = (g_btnARaw == BTN_A_ACTIVE_LEVEL);
  bool rawBPressed = (g_btnBRaw == BTN_B_ACTIVE_LEVEL);

  // IRQ path: catches short taps that happen while SD/BAT/LCD work blocks polling.
  // Do not require the button to still be held by the time loop() resumes.
  if (irqA) {
    queueButtonA(now);
    g_btnA = rawAPressed ? true : g_btnA; // keep display state sane if still held
    g_btnALastRawPressed = rawAPressed;
    g_btnALastChangeMs = now;
  }

  if (irqB) {
    queueButtonB(now);
    g_btnB = rawBPressed ? true : g_btnB;
    g_btnBLastRawPressed = rawBPressed;
    g_btnBLastChangeMs = now;
  }

  // Polling path: keeps button state display correct and acts as fallback if IRQ
  // is unavailable for a board/core variant.
  if (rawAPressed != g_btnALastRawPressed) {
    g_btnALastRawPressed = rawAPressed;
    g_btnALastChangeMs = now;
  }

  if (rawBPressed != g_btnBLastRawPressed) {
    g_btnBLastRawPressed = rawBPressed;
    g_btnBLastChangeMs = now;
  }

  if ((now - g_btnALastChangeMs) >= BTN_DEBOUNCE_MS && rawAPressed != g_btnA) {
    bool old = g_btnA;
    g_btnA = rawAPressed;
    if (g_btnA && !old) {
      queueButtonA(now);
    }
  }

  if ((now - g_btnBLastChangeMs) >= BTN_DEBOUNCE_MS && rawBPressed != g_btnB) {
    bool old = g_btnB;
    g_btnB = rawBPressed;
    if (g_btnB && !old) {
      queueButtonB(now);
    }
  }

  // Release state must still be reflected even if the press was IRQ-latched.
  if (!rawAPressed && g_btnA && (now - g_btnALastChangeMs) >= BTN_DEBOUNCE_MS) {
    g_btnA = false;
  }
  if (!rawBPressed && g_btnB && (now - g_btnBLastChangeMs) >= BTN_DEBOUNCE_MS) {
    g_btnB = false;
  }
}

// ========================= Battery =========================

static void enableBatteryDivider() {
  // READ_BAT is P0.14. Enable by sinking it to GND.
  NRF_P0->OUTCLR = (1UL << READ_BAT_P0_PIN);
  NRF_P0->DIRSET = (1UL << READ_BAT_P0_PIN);
}

static void disableBatteryDivider() {
  // High impedance = divider off, reduce leakage.
  NRF_P0->DIRCLR = (1UL << READ_BAT_P0_PIN);
}

static bool sampleChargingRawLow() {
  uint8_t lowCount = 0;

  for (uint8_t i = 0; i < CHG_SAMPLE_COUNT; i++) {
    if ((NRF_P0->IN & (1UL << CHG_P0_PIN)) == 0) {
      lowCount++;
    }
    delayMicroseconds(400);
  }

  // Majority vote. A single noisy low should not trigger charging.
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

  // Quick clear: when USB-C is removed, ~CHG should remain HIGH.
  // Three battery refreshes are enough to avoid one-sample flicker but still update fast.
  if (g_chgHighStreak >= CHG_HIGH_CLEAR_COUNT) {
    g_chgState = false;
  }

  return g_chgState;
}

static const char *batteryPresenceStateName(BatteryPresenceState s) {
  switch (s) {
    case BAT_BOOT:             return "BOOT";
    case BAT_USB_ONLY:         return "USB_ONLY";
    case BAT_INSERT_CANDIDATE: return "INSERT?";
    case BAT_PRESENT:          return "PRESENT";
    case BAT_REMOVE_CANDIDATE: return "REMOVE?";
    default:                   return "UNKNOWN";
  }
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
  // Slow baseline tracking while unconfirmed. This represents the charger/BAT node
  // behavior without a confirmed cell attached.
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

static uint16_t readBatteryRawAvg(uint8_t samples, uint16_t &rawMin, uint16_t &rawMax) {
  if (samples > 32) samples = 32;

  uint16_t buf[32];

  for (uint8_t i = 0; i < 6; i++) {
    (void)analogRead(PIN_VBAT);
    delay(2);
  }

  for (uint8_t i = 0; i < samples; i++) {
    buf[i] = analogRead(PIN_VBAT);
    delay(2);
  }

  // Sort small buffer and use a trimmed average to reject charger/floating spikes.
  for (uint8_t i = 0; i < samples; i++) {
    for (uint8_t j = i + 1; j < samples; j++) {
      if (buf[j] < buf[i]) {
        uint16_t t = buf[i];
        buf[i] = buf[j];
        buf[j] = t;
      }
    }
  }

  uint8_t trim = samples >= 16 ? 4 : 1;
  uint32_t sum = 0;
  uint8_t count = 0;

  // Report a robust range, not the absolute min/max, so one spike does not
  // make a real battery look like NO BAT.
  rawMin = buf[trim];
  rawMax = buf[samples - 1 - trim];

  for (uint8_t i = trim; i < samples - trim; i++) {
    sum += buf[i];
    count++;
  }

  return count ? (uint16_t)(sum / count) : buf[samples / 2];
}

static void updateBattery() {
  BatteryState measured;

  enableBatteryDivider();
  delay(30);

  measured.raw = readBatteryRawAvg(28, measured.rawMin, measured.rawMax);
  measured.vadc = ((float)measured.raw * ADC_FULL_SCALE_V) / (float)ADC_MAX;
  measured.vbat = measured.vadc * BAT_DIVIDER_RATIO * BAT_CAL_FACTOR;
  measured.percent = lipoPercent(measured.vbat);

  uint16_t spread = measured.rawMax - measured.rawMin;
  bool voltagePlausible = (measured.raw > BAT_PRESENT_MIN_RAW &&
                           measured.vbat > BAT_VALID_MIN_V &&
                           measured.vbat < BAT_VALID_MAX_V);
  bool stableBatch = voltagePlausible && (spread <= BAT_FLOAT_RANGE_RAW);
  bool veryStableBatch = voltagePlausible && (spread <= BAT_STABLE_PRESENT_SPREAD_RAW);

  bool chgWasLow = g_chgRawLow;
  bool chargingNow = updateChargingState(voltagePlausible, measured.vbat);
  bool chgEdgeLow = (!g_prevChgRawLow && g_chgRawLow);
  g_prevChgRawLow = g_chgRawLow;

  bool closeToLastGood = g_haveLastGoodBat &&
                         fabsf(measured.vbat - g_lastGoodBat.vbat) <= BAT_NOISY_CLOSE_DELTA_V;
  bool farFromLastGood = g_haveLastGoodBat &&
                         fabsf(measured.vbat - g_lastGoodBat.vbat) >= BAT_REMOVE_DELTA_V;

  disableBatteryDivider();

  // Case 1: not even a plausible Li-ion voltage.
  if (!voltagePlausible) {
    g_batValidStreak = 0;
    g_batNoisyStreak = 0;
    if (g_batInvalidStreak < 255) g_batInvalidStreak++;

    if (g_batPhysicallyConfirmed && g_batInvalidStreak < BAT_INVALID_CONFIRM_COUNT) {
      g_bat = g_lastGoodBat;
      g_bat.valid = true;
      g_bat.charging = chargingNow;
      g_batFilterState = "HOLD";
      return;
    }

    setBatteryAbsentUsb("MISS");
    g_bat = measured;
    g_bat.valid = false;
    g_bat.charging = false;
    return;
  }

  // Case 2: already confirmed battery present.
  if (g_batPhysicallyConfirmed) {
    uint32_t nowMs = millis();
    bool recentChgTransition = (nowMs - g_lastChgRawChangeMs) < BAT_CHG_TRANSIENT_HOLD_MS;

    // In a real USB unplug case:
    //   CHG goes HIGH quickly, but VBAT remains stable because the real battery is still attached.
    //   => stableBatch is true, so we should NOT remove the battery.
    // In a real battery removal while USB is present:
    //   CHG usually goes HIGH and the high-impedance VBAT ADC becomes noisy.
    //   => !stableBatch + CHG HIGH persists for several refreshes.
    // In USB plug-in transient:
    //   spread may glitch briefly. Hold PRESENT for a short guard window.
    bool noisyOrJump = (!stableBatch) || farFromLastGood;
    bool chgHighNow = !g_chgRawLow;
    bool likelyRemoved = noisyOrJump && chgHighNow;

    if (noisyOrJump) {
      if (likelyRemoved) {
        if (g_removeCandidateStreak < 255) g_removeCandidateStreak++;
        g_batState = BAT_REMOVE_CANDIDATE;

        // Product UI fix:
        // As soon as we see the clear removal signature
        //   CHG HIGH + noisy/jumped VBAT
        // do NOT show the stale battery percentage with charging cleared.
        // That one-frame state looked like "battery inserted but not charging".
        // Instead, switch UI to USB PWR immediately while the internal confirm
        // counter decides whether to fully clear batConf.
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

      // Noisy ADC while CHG is still LOW: usually charging startup / LCD/SD ADC glitch.
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

      // Last fallback for noisy state that does not look like a clean USB unplug.
      if (g_removeCandidateStreak < 255) g_removeCandidateStreak++;
      g_batState = BAT_REMOVE_CANDIDATE;

      if (g_removeCandidateStreak >= BAT_REMOVE_CONFIRM_COUNT) {
        setBatteryAbsentUsb("REMOVED");
        g_bat = measured;
        g_bat.valid = false;
        g_bat.charging = false;
        return;
      }

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

    if (closeToLastGood) {
      BatteryState filtered = measured;
      filtered.valid = true;
      filtered.vbat = g_lastGoodBat.vbat * 0.80f + measured.vbat * 0.20f;
      filtered.vadc = filtered.vbat / (BAT_DIVIDER_RATIO * BAT_CAL_FACTOR);
      filtered.percent = lipoPercent(filtered.vbat);
      filtered.charging = chargingNow;
      confirmBatteryPresent(filtered, "NOISY");
      return;
    }
  }

  // Case 3: battery not confirmed yet.
  // If CHG is high / not charging and VBAT is stable, the board is effectively on a real cell path.
  // This handles "battery only" and "USB removed after battery inserted".
  if (stableBatch && !g_chgRawLow) {
    measured.valid = true;
    measured.charging = false;
    confirmBatteryPresent(measured, "BAT_ONLY");
    return;
  }

  // Under USB-C with no confirmed battery, do not trust static VBAT.
  // Learn a USB-only baseline, then confirm hot-plug battery only by a state change:
  //   - CHG edge HIGH->LOW
  //   - spread improves from noisy baseline to very stable
  //   - VBAT shifts meaningfully from USB-only baseline
  updateUsbOnlyBaseline(measured, spread);

  bool baselineDelta = g_usbBaselineValid &&
                       fabsf(measured.vbat - g_usbBaselineVbat) >= BAT_INSERT_DELTA_V;
  bool spreadImproved = g_usbBaselineValid &&
                        (g_usbBaselineSpread > BAT_FLOAT_RANGE_RAW) &&
                        veryStableBatch;
  bool insertCandidate = stableBatch && (chgEdgeLow || spreadImproved || baselineDelta);

  if (insertCandidate) {
    if (g_insertCandidateStreak < 255) g_insertCandidateStreak++;
    g_batState = BAT_INSERT_CANDIDATE;
    g_batFilterState = "INS?";

    if (g_insertCandidateStreak >= BAT_INSERT_CONFIRM_COUNT) {
      measured.valid = true;
      measured.charging = chargingNow;
      confirmBatteryPresent(measured, chargingNow ? "INSERT_CHG" : "INSERT");
      return;
    }
  } else {
    if (g_insertCandidateStreak > 0) g_insertCandidateStreak--;
    g_batState = BAT_USB_ONLY;
    g_batFilterState = "USBVBAT";
  }

  g_bat = measured;
  g_bat.valid = false;
  g_bat.charging = false;
}

static String batteryShortText() {
  if (!g_bat.valid) {
    if (g_batState == BAT_USB_ONLY ||
        g_batState == BAT_INSERT_CANDIDATE ||
        g_batState == BAT_REMOVE_CANDIDATE) {
      return "USB PWR";
    }
    return "NO BAT";
  }

  char buf[32];
  snprintf(buf, sizeof(buf), "%d%%", g_bat.percent);
  return String(buf);
}

// ========================= UI update =========================

static void updateUiFast() {
  String sdText = g_sdMounted ? (String("OK ") + (g_sdOkFreq / 1000) + "k") : "Unplugged";
  if (sdText != cache_sd) {
    cache_sd = sdText;
    printTextFixed(62, ROW_SD, g_sdMounted ? C_GREEN : C_RED, sdText, 15);
  }

  String touchText = g_touchValid ? (String("(") + g_touchX + "," + g_touchY + ")") : String("release");
  if (touchText != cache_touch) {
    cache_touch = touchText;
    printTextFixed(62, ROW_TOUCH, C_CYAN, touchText, 15);
  }

  String batText = batteryShortText();
  if (batText != cache_bat) {
    cache_bat = batText;
    uint16_t c = g_bat.valid ? colorByPercent(g_bat.percent) : C_RED;
    if (g_bat.charging) c = C_CYAN;
    printTextFixed(90, ROW_BAT, c, batText, 9);
  }

  int batIconState = (g_bat.valid ? 1000 : 0) + (g_bat.charging ? 500 : 0) + constrain(g_bat.percent, 0, 100);
  if (batIconState != cache_batIconState) {
    cache_batIconState = batIconState;
    drawBatteryIcon(g_bat.valid, g_bat.percent, g_bat.charging);
  }

  int chargeIconState = (g_bat.valid && g_bat.charging) ? 1 : 0;
  if (chargeIconState != cache_chargeIcon) {
    cache_chargeIcon = chargeIconState;
    drawChargeIcon(chargeIconState == 1);
  }

  String blText = String(currentBacklightPercent()) + "%";
  if (blText != cache_bl) {
    cache_bl = blText;
    uint16_t c = currentBacklightPwm() == 0 ? C_RED : C_CYAN;

    // Keep the value away from the "Brightness" label. v2.5 had both too close
    // on the same line, which made this card look cramped.
    printTextFixed(118, ROW_BTN1, c, blText, 4);
  }

  String tapText = String("Tap ") + String(g_doubleTapCount);
  if (tapText != cache_tap) {
    cache_tap = tapText;
    printTextFixed(110, ROW_TAP_MOTION, C_YELLOW, tapText, 7);
  }

}

static void updateUiSlow() {
  char buf[64];

  snprintf(buf, sizeof(buf), "%.1f %.1f %.1f", g_ax, g_ay, g_az);
  String accText(buf);
  if (accText != cache_acc) {
    cache_acc = accText;
    printTextFixed(50, ROW_ACC, C_WHITE, accText, 16);
  }

  snprintf(buf, sizeof(buf), "%.1f %.1f %.1f", g_gx, g_gy, g_gz);
  String gyrText(buf);
  if (gyrText != cache_gyr) {
    cache_gyr = gyrText;
    printTextFixed(50, ROW_GYR, C_WHITE, gyrText, 16);
  }
}

// ========================= Serial =========================

static void printSerialStatus() {
  // v1.6: use typed Serial.print instead of a long printf-style vararg log.
  // The v1.5 log string/argument list was mismatched after adding CHG fields,
  // which could corrupt the stack and make the dashboard appear frozen.
  Serial.print("[DASH] sd=");
  Serial.print(g_sdMounted ? "Inserted" : "Unplugged");

  Serial.print(" freq=");
  Serial.print((unsigned long)g_sdOkFreq);

  Serial.print(" touch=");
  Serial.print(g_touchValid ? "Y" : "N");
  Serial.print(" x=");
  Serial.print(g_touchX);
  Serial.print(" y=");
  Serial.print(g_touchY);

  Serial.print(" bat=");
  Serial.print(g_bat.vbat, 3);
  Serial.print("V pct=");
  Serial.print(g_bat.percent);
  Serial.print(" valid=");
  Serial.print(g_bat.valid ? "Y" : "N");
  Serial.print(" chg=");
  Serial.print(g_bat.charging ? "Y" : "N");
  Serial.print(" chgRaw=");
  Serial.print(g_chgRawLow ? "LOW" : "HIGH");
  Serial.print(" chgHi=");
  Serial.print((unsigned)g_chgHighStreak);

  Serial.print(" raw=");
  Serial.print(g_bat.raw);
  Serial.print(" range=");
  Serial.print(g_bat.rawMin);
  Serial.print("-");
  Serial.print(g_bat.rawMax);
  Serial.print(" spread=");
  Serial.print((unsigned)(g_bat.rawMax - g_bat.rawMin));
  Serial.print(" inv=");
  Serial.print((unsigned)g_batInvalidStreak);
  Serial.print(" noisy=");
  Serial.print((unsigned)g_batNoisyStreak);
  Serial.print(" filt=");
  Serial.print(g_batFilterState);
  Serial.print(" batConf=");
  Serial.print(g_batPhysicallyConfirmed ? "Y" : "N");
  Serial.print(" batState=");
  Serial.print(batteryPresenceStateName(g_batState));
  Serial.print(" chgAge=");
  Serial.print((unsigned long)(millis() - g_lastChgRawChangeMs));
  Serial.print("ms");
  Serial.print(" ins=");
  Serial.print((unsigned)g_insertCandidateStreak);
  Serial.print(" rm=");
  Serial.print((unsigned)g_removeCandidateStreak);
  Serial.print(" base=");
  Serial.print(g_usbBaselineVbat, 3);
  Serial.print("V/");
  Serial.print((unsigned)g_usbBaselineSpread);

  Serial.print(" tap=");
  Serial.print((unsigned long)g_doubleTapCount);

  Serial.print(" bl=");
  Serial.print((unsigned)currentBacklightPwm());
  Serial.print("/");
  Serial.print(currentBacklightPercent());
  Serial.print("%");

  Serial.print(" acc=(");
  Serial.print(g_ax, 2);
  Serial.print(",");
  Serial.print(g_ay, 2);
  Serial.print(",");
  Serial.print(g_az, 2);
  Serial.print(")");

  Serial.print(" gyr=(");
  Serial.print(g_gx, 2);
  Serial.print(",");
  Serial.print(g_gy, 2);
  Serial.print(",");
  Serial.print(g_gz, 2);
  Serial.print(")");

  Serial.print(" mic=");
  Serial.print((unsigned)currentMicPeak());

  Serial.print(" usr1=");
  Serial.print(g_btnA ? "Pressed" : "Released");
  Serial.print(" raw1=");
  Serial.print(g_btnARaw);

  Serial.print(" usr2=");
  Serial.print(g_btnB ? "Pressed" : "Released");
  Serial.print(" raw2=");
  Serial.print(g_btnBRaw);
  Serial.print(" pendA=");
  Serial.print(g_btnAPendingAction ? "Y" : "N");
  Serial.print(" pendB=");
  Serial.println(g_btnBPendingAction ? "Y" : "N");
}

// ========================= Setup / loop =========================

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(BTN_A_PIN, INPUT_PULLUP);
  pinMode(BTN_B_PIN, INPUT_PULLUP);
  g_btnARaw = digitalRead(BTN_A_PIN);
  g_btnBRaw = digitalRead(BTN_B_PIN);
  g_btnALastRawPressed = (g_btnARaw == BTN_A_ACTIVE_LEVEL);
  g_btnBLastRawPressed = (g_btnBRaw == BTN_B_ACTIVE_LEVEL);
  g_btnA = g_btnALastRawPressed;
  g_btnB = g_btnBLastRawPressed;
  g_btnALastChangeMs = millis();
  g_btnBLastChangeMs = millis();
  g_btnAPressEvent = false;
  g_btnBPressEvent = false;
  g_btnAIrqCount = 0;
  g_btnBIrqCount = 0;
  g_btnAPendingAction = false;
  g_btnBPendingAction = false;
  g_btnALastQueueMs = 0;
  g_btnBLastQueueMs = 0;
  g_btnALastStateEmitMs = 0;
  g_btnBLastStateEmitMs = 0;

  attachInterrupt(digitalPinToInterrupt(BTN_A_PIN), btnAIrqIsr, FALLING);
  attachInterrupt(digitalPinToInterrupt(BTN_B_PIN), btnBIrqIsr, FALLING);

  // P0.17 / ~CHG is an open-drain style status line on the charger side.
  // Enable internal pull-up to avoid floating-low false charging reports.
  nrf_gpio_cfg_input(CHG_P0_PIN, NRF_GPIO_PIN_PULLUP);
  sampleChargingRawLow();
  if (g_chgRawLow) {
    g_chgState = true;
    g_lastChgLowMs = millis();
  }

  analogReadResolution(ADC_BITS);
  disableBatteryDivider();

  Serial.println();
  Serial.println("=== XIAO nRF52840 Plus 1.47 Factory Dashboard v2.7 ===");
  Serial.println("UI + event battery detect + queued buttons + brighter backlight hint");
  Serial.print("PIN_VBAT = "); Serial.println(PIN_VBAT);
#ifdef PIN_PDM_CLK
  Serial.print("PIN_PDM_CLK = "); Serial.println(PIN_PDM_CLK);
#endif
#ifdef PIN_PDM_DIN
  Serial.print("PIN_PDM_DIN = "); Serial.println(PIN_PDM_DIN);
#endif

  initTouchAndImu();
  initImuDoubleTap();

  if (!initLcd()) {
    Serial.println("[FAIL] LCD init failed");
    return;
  }

  // Touch: AXS5106L shares RST with the LCD (already reset by display.begin<>).
  // Wire was begun in initTouchAndImu(); attach the driver to the LCD bus.
  if (!display.attachTouch(touch, display.panel().driver().bus())) {
    Serial.println("[WARN] touch attach failed");
    Serial.println(display.lastResult().message);
  }

  if (!initMic()) {
    Serial.println("[WARN] MIC init failed, VU may stay zero");
  }

  updateSdStatus();
  updateTouch();
  updateImu();
  updateButtons();
  updateBattery();

  drawStaticLayout();
  updateUiFast();
  updateUiSlow();
  updateVuMeter();
  updateMicText();
  printSerialStatus();
}

void loop() {
  uint32_t now = millis();

  handleImuTapEvent();
  updateButtons();
  handleBacklightButtons();

  if (now - g_lastSdMs >= SD_REFRESH_MS) {
    g_lastSdMs = now;
    updateSdStatus();
  }

  if (now - g_lastBatMs >= BAT_REFRESH_MS) {
    g_lastBatMs = now;
    updateBattery();
  }

  if (now - g_lastSlowUiMs >= UI_TEXT_SLOW_MS) {
    g_lastSlowUiMs = now;
    updateImu();
    updateTouch();
    updateUiSlow();
  }

  if (now - g_lastFastUiMs >= UI_TEXT_FAST_MS) {
    g_lastFastUiMs = now;
    updateUiFast();
  }

  if (now - g_lastVuMs >= UI_VU_MS) {
    g_lastVuMs = now;
    updateVuMeter();
  }

  if (now - g_lastMicTextMs >= UI_MIC_TEXT_MS) {
    g_lastMicTextMs = now;
    updateMicText();
  }

  if (now - g_lastSerialMs >= SERIAL_MS) {
    g_lastSerialMs = now;
    printSerialStatus();
  }
}
