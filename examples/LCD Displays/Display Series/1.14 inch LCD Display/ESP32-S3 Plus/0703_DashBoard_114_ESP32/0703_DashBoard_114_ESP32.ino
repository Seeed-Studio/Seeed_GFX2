/**
 * Product: XIAO 1.14 inch LCD Board (ST7789 135x240 IPS, no touch)
 * Display: ST7789 135x240, RGB, no touch
 * Target:  XIAO ESP32-S3 Plus (RST=13, BL=12). For nRF52840 Plus use the sibling
 *          nRF52840 Plus folder (RST=38, BL=37).
 * Wiring:  CS=D2, DC=D3, MOSI=D10, SCLK=D8; RST=13, BL=12.
 * Demo:    1.14 ESP32-S3 factory dashboard: I2S PDM mic VU, IMU auto-detect (QMI8658 or
 *          LSM6DS3) with double-tap, Grove I2C step-scan, 3 user buttons, D16 ADC voltage sense.
 *
 * Ported from XIAO-Display-Board-main (0703_DashBoard_114_ESP32, Arduino_GFX). The Seeed_GFX
 * Board/Config templates replace the original bus+panel setup, manual MADCTL/invert/
 * swapbytes, and (for TFT_eSPI) the driver.h User_Setup. Keep ESP32 I2S PDM
 * (driver/i2s_pdm.h, IDF v5 i2s_pdm_rx), I2C IMU auto-detect, Grove step-scan,
 * analogReadMilliVolts, and WiFi.mode(WIFI_OFF) under ESP_IDF_VERSION_MAJOR guards;
 * backlight PWM stays on analogWrite(LCD_BL_PIN).
 */

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <WiFi.h>
#include <Seeed_GFX.h>
#include "board/boards/XIAO_LCD_Board.h"
#include "driver/tft/Driver_ST7789.h"
#include "panel/Panel_TFT.h"
#include "esp_idf_version.h"

#if ESP_IDF_VERSION_MAJOR >= 5
  #include <driver/i2s_pdm.h>
#else
  #include <driver/i2s.h>
#endif

#include <driver/gpio.h>
#include <math.h>

// ========================= Build switches =========================

#define REDUCE_PDM_CLK_DRIVE 1

// ========================= Timing =========================

static constexpr uint32_t UI_FAST_MS = 120;
static constexpr uint32_t UI_SLOW_MS = 260;
static constexpr uint32_t UI_VU_MS = 60;
static constexpr uint32_t SERIAL_MS = 600;
static constexpr uint32_t I2C_SCAN_MS = 120;   // step-scan interval, avoids long I2C bursts during Grove hot-plug
static constexpr uint32_t BAT_REFRESH_MS = 350;
static constexpr uint32_t VSENSE_UI_MS = 1000;      // D16/Calc display check interval
static constexpr float VSENSE_D16_DELTA_V = 0.02f; // avoid 0.01V ADC jitter redraw
static constexpr float VSENSE_CALC_DELTA_V = 0.05f;
static constexpr uint32_t BTN_DEBOUNCE_MS = 35;
static constexpr uint32_t BTN_ACTION_LOCKOUT_MS = 150;
static constexpr uint32_t TAP_DEBOUNCE_MS = 220;

// ========================= LCD parameters =========================
// Rotation/IPS/offsets are baked into Config_Seeed_1inch14_LCD_ST7789 (135x240 RGB).

static constexpr int LCD_W = 135;
static constexpr int LCD_H = 240;

// ========================= MIC parameters =========================

static constexpr int MIC_SAMPLE_RATE_HZ = 16000;
static constexpr bool MIC_CLK_INVERT = false;
static constexpr size_t MIC_SAMPLES_PER_READ = 256;

// Display-only scaling, tuned for the 1.14 block VU.
static constexpr float MIC_DISPLAY_SCALE = 2400.0f;

// ========================= Battery =========================
// BAT monitor policy:
//   1. Is a real battery connected?
//   2. If connected, what is the approximate battery percentage?
//   3. Do NOT infer charging state.
// Divider:
//   VBAT -- 316K -- ADC -- 160K -- GND
//   ratio = 476K / 160K = 2.975

static constexpr float BAT_R_TOP_KOHM        = 316.0f;
static constexpr float BAT_R_BOTTOM_KOHM     = 160.0f;
static constexpr float BAT_DIVIDER_RATIO     = (BAT_R_TOP_KOHM + BAT_R_BOTTOM_KOHM) / BAT_R_BOTTOM_KOHM;
static constexpr float BAT_CAL_FACTOR        = 1.000f;

static constexpr float BAT_VALID_MIN_V       = 2.80f;
static constexpr float BAT_VALID_MAX_V       = 4.60f;
static constexpr uint16_t BAT_PRESENT_MIN_MV = 850;
static constexpr uint16_t BAT_FLOAT_RANGE_MV = 90;

static constexpr uint8_t BAT_VALID_CONFIRM_COUNT = 3;
static constexpr uint8_t BAT_INVALID_CONFIRM_COUNT = 3;

// USB-only cold boot guard.
// Observed on 1.47: USB-only can show a stable fake ~70%.
// Trade-off: a real high-SOC battery-only cold boot can also be rejected.
static constexpr bool BAT_COLD_BOOT_HIGH_AS_USB = true;
static constexpr int  BAT_COLD_BOOT_SUSPECT_PCT = 60;

// Reject impossible high jump after a known real battery.
static constexpr float BAT_IMPOSSIBLE_JUMP_V     = 0.28f;
static constexpr int   BAT_IMPOSSIBLE_JUMP_PCT   = 22;
static constexpr uint8_t BAT_FLOAT_REJECT_STREAK = 2;

// Reinsert unlock after FLOAT_LOCK.
static constexpr float BAT_REINSERT_DROP_V       = 0.16f;

// USB-first then battery-insert compensation.
// Observed on 1.47: true 27%, USB-first insertion showed 39%, USB unplug returned to 27%.
static constexpr float BAT_USB_INSERT_SURFACE_COMP_V = 0.085f;
static constexpr float BAT_USB_INSERT_RELEASE_DROP_V = 0.045f;
static constexpr float BAT_EFFECTIVE_MIN_V           = 3.25f;

// UI percentage limiter.
static constexpr uint32_t BAT_PCT_RISE_INTERVAL_MS = 90000;

// ADC sampling.
static constexpr uint8_t BAT_SAMPLE_COUNT = 12;
static constexpr uint16_t BAT_SAMPLE_DELAY_US = 700;

// ========================= Pins =========================

static constexpr uint8_t MIC_CLK_PIN   = D0;
static constexpr uint8_t MIC_DATA_PIN  = D1;
static constexpr uint8_t LCD_CS_PIN    = D2;
static constexpr uint8_t LCD_DC_PIN    = D3;
static constexpr uint8_t I2C_SDA_PIN   = D4;
static constexpr uint8_t I2C_SCL_PIN   = D5;
static constexpr uint8_t BTN_A_PIN     = D6;   // USR1
static constexpr uint8_t BTN_B_PIN     = D7;   // USR2
static constexpr uint8_t LCD_SCK_PIN   = D8;
static constexpr uint8_t LCD_MISO_PIN  = D9;   // NC, unused by LCD
static constexpr uint8_t LCD_MOSI_PIN  = D10;
static constexpr uint8_t I2S_SD_PAD    = D11;
static constexpr uint8_t I2S_SCK_PAD   = D12;
static constexpr uint8_t I2S_WS_PAD    = D13;
static constexpr uint8_t IMU_INT_PIN   = D14;
static constexpr uint8_t BAT_ADC_PIN   = D16;
static constexpr uint8_t BTN_C_PIN     = D19;  // USR3

// ========================= Colors =========================

static constexpr uint16_t C_BLACK   = TFT_BLACK;
static constexpr uint16_t C_WHITE   = TFT_WHITE;
static constexpr uint16_t C_GREEN   = TFT_GREEN;
static constexpr uint16_t C_RED     = TFT_RED;
static constexpr uint16_t C_CYAN    = TFT_CYAN;
static constexpr uint16_t C_YELLOW  = TFT_YELLOW;
static constexpr uint16_t C_ORANGE  = 0xFD20;
static constexpr uint16_t C_GRAY    = 0x8410;
static constexpr uint16_t C_DIM     = 0x2104;
static constexpr uint16_t C_BLUE    = TFT_BLUE;
static constexpr uint16_t C_PANEL   = 0x0841;
static constexpr uint16_t C_LINE    = 0x39E7;

// ========================= LCD object =========================

Seeed_GFX display;

static constexpr int8_t LCD_RST_PIN = 13;  // XIAO ESP32-S3 Plus
static constexpr int8_t LCD_BL_PIN  = 12;

// ========================= I2S object =========================

#if ESP_IDF_VERSION_MAJOR >= 5
static i2s_chan_handle_t g_i2sRxChan = nullptr;
#else
static constexpr i2s_port_t I2S_PORT = I2S_NUM_0;
#endif

// ========================= State =========================

bool g_lcdOk = false;
bool g_micOk = false;

struct AudioStats {
  int32_t mean;
  uint32_t peak;
  uint32_t rms;
};

AudioStats g_micStats = {};
int16_t g_pdmBuf[MIC_SAMPLES_PER_READ];
uint32_t g_micLastUpdateMs = 0;

enum ImuType {
  IMU_NONE = 0,
  IMU_QMI8658,
  IMU_LSM6DS3
};

ImuType g_imuType = IMU_NONE;
uint8_t g_imuAddr = 0;
bool g_imuOk = false;
float g_ax = 0, g_ay = 0, g_az = 0;
float g_gx = 0, g_gy = 0, g_gz = 0;

// Buttons
bool g_btnA = false;
bool g_btnB = false;
bool g_btnC = false;
int g_btnARaw = HIGH;
int g_btnBRaw = HIGH;
int g_btnCRaw = HIGH;
bool g_btnALastRawPressed = false;
bool g_btnBLastRawPressed = false;
bool g_btnCLastRawPressed = false;
uint32_t g_btnALastChangeMs = 0;
uint32_t g_btnBLastChangeMs = 0;
uint32_t g_btnCLastChangeMs = 0;
bool g_btnAPressEvent = false;
bool g_btnBPressEvent = false;
bool g_btnCPressEvent = false;
uint32_t g_lastBtnActionMs = 0;

// Backlight control
static const uint8_t BL_LEVELS[] = {255, 192, 128, 64, 0};
static const int BL_LABELS[] = {100, 75, 50, 25, 0};
static constexpr uint8_t BL_LEVEL_COUNT = sizeof(BL_LEVELS) / sizeof(BL_LEVELS[0]);
uint8_t g_blIndex = 0;
uint8_t g_blRestoreIndex = 0;

// I2C scan
uint8_t g_i2cDevices[12] = {};
uint8_t g_i2cCount = 0;
uint8_t g_i2cNextDevices[12] = {};
uint8_t g_i2cNextCount = 0;
uint8_t g_i2cScanAddr = 0x08;
bool g_i2cScanFault = false;
uint8_t g_i2cScanFaultCount = 0;

// Battery
struct BatteryState {
  uint16_t raw = 0;
  uint16_t rawMin = 0;
  uint16_t rawMax = 0;

  uint16_t mv = 0;
  uint16_t mvMin = 0;
  uint16_t mvMax = 0;
  uint16_t spreadMv = 0;

  float vadc = 0.0f;
  float rawVbat = 0.0f;
  float vbat = 0.0f;
  int percent = 0;

  bool valid = false;
  const char *state = "BOOT";
};

BatteryState g_bat;
BatteryState g_vsense;       // raw D16 ADC measurement before BAT/USB filtering
BatteryState g_lastGoodBat;

bool g_haveLastGoodBat = false;
uint8_t g_batValidStreak = 0;
uint8_t g_batInvalidStreak = 0;
const char *g_batFilterState = "BOOT";

int g_batDisplayPercent = -1;
uint32_t g_batLastPctRiseMs = 0;

bool g_batFloatReject = false;
uint8_t g_batFloatRejectStreak = 0;
uint8_t g_batReinsertStreak = 0;
float g_batLastRealV = 0.0f;
int g_batLastRealPercent = -1;

uint8_t g_batColdUsbSuspectStreak = 0;

bool g_batUsbOnlySeen = false;
bool g_batUsbInsertCompActive = false;
float g_batUsbInsertRawStartV = 0.0f;

// Tap
volatile bool g_imuIntFlag = false;
bool g_imuTapConfigured = false;
uint32_t g_doubleTapCount = 0;
uint32_t g_lastTapMs = 0;

// UI/cache/timing
uint32_t g_lastFastUiMs = 0;
uint32_t g_lastSlowUiMs = 0;
uint32_t g_lastVuMs = 0;
uint32_t g_lastSerialMs = 0;
uint32_t g_lastI2cMs = 0;
uint32_t g_lastBatMs = 0;
uint32_t g_lastVsenseUiMs = 0;
uint32_t g_frameCounter = 0;

float g_vuSmooth = 0.0f;
int g_cachedVuSegments = -1;

bool g_headerShowSeeed = false;

String cache_micRaw = "";
String cache_acc = "";
String cache_gyr = "";
String cache_tap = "";
String cache_bat = "";
String cache_d16_adc = "";
String cache_d16_calc = "";
float g_d16ShownVadc = -1.0f;
float g_d16ShownCalc = -1.0f;
String cache_grove = "";
String cache_key = "";
String cache_bl = "";
int cache_batIconState = -1;

// ========================= UI layout =========================

static constexpr int ROW_TITLE = 2;
static constexpr int ROW_SUB1  = 21;

static constexpr int CARD_X = 3;
static constexpr int CARD_W = 129;

static constexpr int Y_SYS = 36;
static constexpr int H_SYS = 38;
static constexpr int ROW_SYS_TITLE = Y_SYS + 9;
static constexpr int ROW_BAT = ROW_SYS_TITLE;
static constexpr int ROW_GROVE = Y_SYS + 25;

static constexpr int Y_IMU = 79;
static constexpr int H_IMU = 50;
static constexpr int ROW_IMU_TITLE = Y_IMU + 9;
static constexpr int ROW_TAP = Y_IMU + 9;
static constexpr int ROW_ACC = Y_IMU + 25;
static constexpr int ROW_GYR = Y_IMU + 39;

static constexpr int Y_MIC = 134;
static constexpr int H_MIC = 45;
static constexpr int ROW_MIC_TITLE = Y_MIC + 9;
static constexpr int ROW_MIC_RAW   = Y_MIC + 32;

static constexpr int VU_X = CARD_X + 14;
static constexpr int VU_Y = Y_MIC + 21;
static constexpr int VU_W = 110;
static constexpr int VU_H = 10;
static constexpr int VU_SEG_COUNT = 12;
static constexpr int VU_GAP = 2;
static constexpr int VU_SEG_W = (VU_W - 4 - (VU_SEG_COUNT - 1) * VU_GAP) / VU_SEG_COUNT;

static constexpr int Y_BL = 182;
static constexpr int H_BL = 31;
static constexpr int ROW_BL = Y_BL + 19;
static constexpr int ROW_PRODUCT = 222;

static constexpr int BAT_ICON_X = 62;
static constexpr int BAT_ICON_Y = ROW_BAT - 2;
static constexpr int BAT_ICON_W = 18;
static constexpr int BAT_ICON_H = 10;

// D16 voltage row is narrow on the 1.14-inch UI. Use one compact full-row
// string and clear the whole inner row before drawing to avoid residual pixels.
static constexpr int VSENSE_ROW_X = 7;
static constexpr int VSENSE_ROW_W = 122;
static constexpr int VSENSE_ROW_CHARS = 20;

// ========================= Helpers =========================

static int16_t le16(const uint8_t *p) {
  return (int16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static bool i2cWrite8(uint8_t addr, uint8_t reg, uint8_t val) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

static bool i2cRead(uint8_t addr, uint8_t reg, uint8_t *buf, size_t len) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;

  size_t got = Wire.requestFrom((int)addr, (int)len);
  if (got != len) return false;

  for (size_t i = 0; i < len; i++) {
    buf[i] = Wire.read();
  }
  return true;
}

static bool i2cRead8(uint8_t addr, uint8_t reg, uint8_t *val) {
  return i2cRead(addr, reg, val, 1);
}

static String padRight(const String &s, int width) {
  String out = s;
  while ((int)out.length() < width) out += ' ';
  if ((int)out.length() > width) out = out.substring(0, width);
  return out;
}

static uint16_t colorByPercent(int p) {
  if (p < 15) return C_RED;
  if (p < 35) return C_ORANGE;
  if (p < 60) return C_YELLOW;
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

static uint8_t currentBacklightPwm() {
  return BL_LEVELS[g_blIndex];
}

static int currentBacklightPercent() {
  return BL_LABELS[g_blIndex];
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
    g_blIndex = BL_LEVEL_COUNT - 1;
  }
  applyBacklight();
}

// ========================= LCD / UI drawing =========================
// Seeed_GFX owns the SPI bus/CS via the Board template, so the former
// acquireForLcd()/LCD_CS_PIN manual arbitration is no longer needed here.

static bool initLcd() {
  if (!display.begin<Board_XIAO_1inch14_LCD<LCD_RST_PIN, LCD_BL_PIN>,
                   Config_Seeed_1inch14_LCD_ST7789>()) {
    g_lcdOk = false;
    Serial.println(display.lastResult().message);
    return false;
  }

  applyBacklight();  // establish initial PWM brightness (Board template already drove RST+BL on)

  display.fillScreen(C_BLACK);
  display.setTextWrap(false);

  g_lcdOk = true;
  Serial.println("[LCD] OK 1.14 ST7789 135x240");
  return true;
}

static bool printTextFixed(int x, int y, uint16_t color, const String &s, int widthChars) {
  if (!g_lcdOk) return false;
  display.setTextSize(1);
  display.setTextColor(color, C_BLACK);
  display.setCursor(x, y);
  display.print(padRight(s, widthChars));
  return true;
}

static void drawCard(int x, int y, int w, int h, uint16_t accent, const char *title) {
  if (!g_lcdOk) return;
  display.drawRoundRect(x, y, w, h, 5, accent);
  display.drawRoundRect(x + 1, y + 1, w - 2, h - 2, 5, C_LINE);
  display.fillRect(x + 4, y + 9, 3, h - 18, accent);
  display.setTextSize(1);
  display.setTextColor(accent, C_BLACK);
  display.setCursor(x + 12, y + 8);
  display.print(title);
}

static void drawVuFrame() {
  if (!g_lcdOk) return;
  display.drawRoundRect(VU_X, VU_Y, VU_W, VU_H, 3, C_GREEN);
  for (int i = 0; i < VU_SEG_COUNT; ++i) {
    int x = VU_X + 2 + i * (VU_SEG_W + VU_GAP);
    display.fillRect(x, VU_Y + 2, VU_SEG_W, VU_H - 4, C_BLACK);
  }
}

static bool drawBatteryIcon(bool valid, int percent) {
  if (!g_lcdOk) return false;

  int x = BAT_ICON_X;
  int y = BAT_ICON_Y;
  display.fillRect(x - 1, y - 1, BAT_ICON_W + 5, BAT_ICON_H + 2, C_BLACK);

  uint16_t outline = valid ? C_WHITE : C_GRAY;
  uint16_t fillColor = valid ? colorByPercent(percent) : C_RED;

  display.drawRoundRect(x, y, BAT_ICON_W, BAT_ICON_H, 2, outline);
  display.fillRect(x + BAT_ICON_W, y + 3, 3, 4, outline);

  if (!valid) {
    display.drawLine(x + 3, y + 2, x + BAT_ICON_W - 3, y + BAT_ICON_H - 3, C_RED);
    display.drawLine(x + BAT_ICON_W - 3, y + 2, x + 3, y + BAT_ICON_H - 3, C_RED);
    return true;
  }

  int fillW = map(percent, 0, 100, 0, BAT_ICON_W - 4);
  fillW = constrain(fillW, 0, BAT_ICON_W - 4);
  if (fillW > 0) display.fillRect(x + 2, y + 2, fillW, BAT_ICON_H - 4, fillColor);
  return true;
}

static void drawHeaderTitle() {
  if (!g_lcdOk) return;
  display.fillRect(0, 0, LCD_W, 20, C_BLACK);
  display.setTextColor(C_GREEN, C_BLACK);

  if (g_headerShowSeeed) {
    display.setTextSize(2);
    display.setCursor(37, ROW_TITLE);
    display.print("Seeed");
  } else {
    display.setTextSize(2);
    display.setCursor(1, ROW_TITLE);
    display.print("Hello,XIAO!");
  }
}

static void drawStaticLayout() {
  if (!g_lcdOk) return;
  display.fillScreen(C_BLACK);

  drawHeaderTitle();

  display.setTextSize(1);
  display.setTextColor(C_CYAN, C_BLACK);
  display.setCursor(6, ROW_SUB1);
  display.print("1.14 Inch Display");
  display.drawFastHLine(6, 31, 123, C_LINE);

  drawCard(CARD_X, Y_SYS, CARD_W, H_SYS, C_CYAN, "SYS");
  display.setTextColor(C_WHITE, C_BLACK);
  // D16 row is drawn as one compact dynamic line by updateUiFast().
  display.setCursor(14, ROW_GROVE);
  display.print("I2C");

  drawCard(CARD_X, Y_IMU, CARD_W, H_IMU, C_YELLOW, "MOTION");
  display.setTextColor(C_YELLOW, C_BLACK);
  display.setCursor(88, ROW_TAP);
  display.print("Tap 0");
  display.setTextColor(C_WHITE, C_BLACK);
  display.setCursor(14, ROW_ACC);
  display.print("A");
  display.setCursor(14, ROW_GYR);
  display.print("G");

  drawCard(CARD_X, Y_MIC, CARD_W, H_MIC, C_GREEN, "MIC LEVEL");
  drawVuFrame();

  drawCard(CARD_X, Y_BL, CARD_W, H_BL, C_BLUE, "BACKLIGHT");

  display.fillRect(0, ROW_PRODUCT - 1, LCD_W, 12, C_BLACK);
  display.setTextColor(C_YELLOW, C_BLACK);
  display.setCursor(13, ROW_PRODUCT);
  display.print("XIAO ESP32-S3 Plus");

  cache_micRaw = "";
  cache_acc = "";
  cache_gyr = "";
  cache_tap = "";
  cache_bat = "";
  cache_d16_adc = "";
  cache_d16_calc = "";
  g_d16ShownVadc = -1.0f;
  g_d16ShownCalc = -1.0f;
  cache_grove = "";
  cache_key = "";
  cache_bl = "";
  cache_batIconState = -1;
  g_cachedVuSegments = -1;
}

// ========================= I2C / IMU =========================

static void printI2cScanResult() {
  Serial.print("[I2C] scan:");
  if (!g_i2cCount) {
    Serial.print(" none");
  } else {
    for (uint8_t i = 0; i < g_i2cCount; i++) {
      Serial.printf(" 0x%02X", g_i2cDevices[i]);
    }
  }
  if (g_i2cScanFaultCount) {
    Serial.printf(" faultCount=%u", g_i2cScanFaultCount);
  }
  Serial.println();
}

static void abortI2cStepScan(uint8_t addr, uint8_t err) {
  // Do not call Wire.end() / pinMode(SDA/SCL) during runtime on ESP32.
  // That can disturb the I2C pin matrix and may make the onboard IMU disappear
  // or cause visible LCD/backlight glitches during Grove hot-plug.
  g_i2cNextCount = 0;
  g_i2cScanAddr = 0x08;
  g_i2cScanFault = true;
  if (g_i2cScanFaultCount < 255) g_i2cScanFaultCount++;

  Serial.printf("[I2C] step scan abort addr=0x%02X err=%u, keep previous result, faultCount=%u\n",
                addr, err, g_i2cScanFaultCount);
}

static void scanI2cBus() {
  // Step scan instead of full-range burst scan.
  // Each call probes a small address batch, so Grove hot-plug cannot block or
  // disturb the UI for a long continuous I2C transaction window.
  static constexpr uint8_t I2C_SCAN_BATCH = 8;

  for (uint8_t n = 0; n < I2C_SCAN_BATCH; n++) {
    if (g_i2cScanAddr > 0x77) {
      g_i2cCount = g_i2cNextCount;
      for (uint8_t i = 0; i < g_i2cCount; i++) {
        g_i2cDevices[i] = g_i2cNextDevices[i];
      }

      g_i2cNextCount = 0;
      g_i2cScanAddr = 0x08;
      g_i2cScanFault = false;

      printI2cScanResult();
      return;
    }

    uint8_t addr = g_i2cScanAddr++;

    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();

    if (err == 0) {
      if (g_i2cNextCount < sizeof(g_i2cNextDevices)) {
        g_i2cNextDevices[g_i2cNextCount++] = addr;
      }
    } else if (err == 4 || err == 5) {
      // ESP32 Wire may return 4/5 for bus error or timeout.
      // Abort this scan cycle and keep the previous stable result.
      // No bus recovery is done here to avoid black-screen flicker on hot-plug.
      abortI2cStepScan(addr, err);
      return;
    }

    delayMicroseconds(80);
  }
}

static bool initQmi(uint8_t addr) {
  uint8_t who = 0;
  if (!i2cRead8(addr, 0x00, &who)) return false;
  if (who == 0x00 || who == 0xFF) return false;

  i2cWrite8(addr, 0x02, 0x60);
  i2cWrite8(addr, 0x03, 0x03);
  i2cWrite8(addr, 0x04, 0x53);
  i2cWrite8(addr, 0x08, 0x03);
  delay(20);

  uint8_t data[12] = {};
  if (!i2cRead(addr, 0x35, data, sizeof(data))) return false;

  g_imuType = IMU_QMI8658;
  g_imuAddr = addr;
  g_imuOk = true;
  Serial.printf("[IMU] QMI8658-compatible at 0x%02X, WHO=0x%02X\n", addr, who);
  return true;
}

static bool initLsm(uint8_t addr) {
  uint8_t who = 0;
  if (!i2cRead8(addr, 0x0F, &who)) return false;
  if (who != 0x69 && who != 0x6A && who != 0x6C) return false;

  i2cWrite8(addr, 0x10, 0x60);
  i2cWrite8(addr, 0x11, 0x60);
  delay(20);

  g_imuType = IMU_LSM6DS3;
  g_imuAddr = addr;
  g_imuOk = true;
  Serial.printf("[IMU] LSM6-compatible at 0x%02X, WHO=0x%02X\n", addr, who);
  return true;
}

// ========================= IMU double-tap interrupt =========================

static constexpr uint8_t LSM_REG_TAP_SRC     = 0x1C;
static constexpr uint8_t LSM_REG_CTRL1_XL    = 0x10;
static constexpr uint8_t LSM_REG_TAP_CFG     = 0x58;
static constexpr uint8_t LSM_REG_TAP_THS_6D  = 0x59;
static constexpr uint8_t LSM_REG_INT_DUR2    = 0x5A;
static constexpr uint8_t LSM_REG_WAKE_UP_THS = 0x5B;
static constexpr uint8_t LSM_REG_MD1_CFG     = 0x5E;

void IRAM_ATTR imuIntIsr() {
  g_imuIntFlag = true;
}

static bool initImuDoubleTap() {
  g_imuTapConfigured = false;

  if (g_imuType != IMU_LSM6DS3 || g_imuAddr == 0) {
    Serial.println("[IMU] double-tap INT skipped: non-LSM IMU");
    return false;
  }

  pinMode(IMU_INT_PIN, INPUT_PULLDOWN);

  bool ok = true;
  ok &= i2cWrite8(g_imuAddr, LSM_REG_CTRL1_XL, 0x60);
  ok &= i2cWrite8(g_imuAddr, LSM_REG_TAP_CFG, 0x8E);
  ok &= i2cWrite8(g_imuAddr, LSM_REG_TAP_THS_6D, 0x0C);
  ok &= i2cWrite8(g_imuAddr, LSM_REG_INT_DUR2, 0x7F);
  ok &= i2cWrite8(g_imuAddr, LSM_REG_WAKE_UP_THS, 0x80);
  ok &= i2cWrite8(g_imuAddr, LSM_REG_MD1_CFG, 0x08);

  uint8_t dummy = 0;
  (void)i2cRead8(g_imuAddr, LSM_REG_TAP_SRC, &dummy);

  attachInterrupt(digitalPinToInterrupt(IMU_INT_PIN), imuIntIsr, RISING);

  g_imuTapConfigured = ok;
  Serial.print("[IMU] double-tap INT1 on D14 init ");
  Serial.println(ok ? "OK" : "FAILED");

  return ok;
}

static void initImu() {
  pinMode(IMU_INT_PIN, INPUT_PULLUP);

  g_imuType = IMU_NONE;
  g_imuAddr = 0;
  g_imuOk = false;
  g_imuTapConfigured = false;

  if (initQmi(0x6B)) {
    initImuDoubleTap();
    return;
  }
  if (initQmi(0x6A)) {
    initImuDoubleTap();
    return;
  }
  if (initLsm(0x6A)) {
    initImuDoubleTap();
    return;
  }
  if (initLsm(0x6B)) {
    initImuDoubleTap();
    return;
  }

  Serial.println("[IMU] not found");
}

static void updateImu() {
  if (g_imuType == IMU_QMI8658) {
    uint8_t d[12] = {};
    if (!i2cRead(g_imuAddr, 0x35, d, sizeof(d))) {
      g_imuOk = false;
      return;
    }

    g_ax = le16(&d[0]) / 16384.0f;
    g_ay = le16(&d[2]) / 16384.0f;
    g_az = le16(&d[4]) / 16384.0f;

    g_gx = le16(&d[6]) / 64.0f;
    g_gy = le16(&d[8]) / 64.0f;
    g_gz = le16(&d[10]) / 64.0f;

    g_imuOk = true;
  } else if (g_imuType == IMU_LSM6DS3) {
    uint8_t g[6] = {};
    uint8_t a[6] = {};

    if (!i2cRead(g_imuAddr, 0x22, g, sizeof(g)) ||
        !i2cRead(g_imuAddr, 0x28, a, sizeof(a))) {
      g_imuOk = false;
      return;
    }

    g_gx = le16(&g[0]) * 0.00875f;
    g_gy = le16(&g[2]) * 0.00875f;
    g_gz = le16(&g[4]) * 0.00875f;

    g_ax = le16(&a[0]) * 0.000061f;
    g_ay = le16(&a[2]) * 0.000061f;
    g_az = le16(&a[4]) * 0.000061f;

    g_imuOk = true;
  } else {
    g_imuOk = false;
  }
}

static void handleImuTapEvent() {
  if (!g_imuTapConfigured || g_imuType != IMU_LSM6DS3) return;

  bool shouldCheck = false;

  noInterrupts();
  if (g_imuIntFlag) {
    g_imuIntFlag = false;
    shouldCheck = true;
  }
  interrupts();

  if (digitalRead(IMU_INT_PIN) == HIGH) {
    shouldCheck = true;
  }

  if (!shouldCheck) return;

  uint8_t src = 0;
  if (!i2cRead8(g_imuAddr, LSM_REG_TAP_SRC, &src)) return;

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

// ========================= Buttons =========================

static void updateButtons() {
  uint32_t now = millis();

  g_btnARaw = digitalRead(BTN_A_PIN);
  g_btnBRaw = digitalRead(BTN_B_PIN);
  g_btnCRaw = digitalRead(BTN_C_PIN);

  bool rawA = (g_btnARaw == LOW);
  bool rawB = (g_btnBRaw == LOW);
  bool rawC = (g_btnCRaw == LOW);

  if (rawA != g_btnALastRawPressed) { g_btnALastRawPressed = rawA; g_btnALastChangeMs = now; }
  if (rawB != g_btnBLastRawPressed) { g_btnBLastRawPressed = rawB; g_btnBLastChangeMs = now; }
  if (rawC != g_btnCLastRawPressed) { g_btnCLastRawPressed = rawC; g_btnCLastChangeMs = now; }

  if ((now - g_btnALastChangeMs) >= BTN_DEBOUNCE_MS && rawA != g_btnA) {
    bool old = g_btnA;
    g_btnA = rawA;
    if (g_btnA && !old) g_btnAPressEvent = true;
  }
  if ((now - g_btnBLastChangeMs) >= BTN_DEBOUNCE_MS && rawB != g_btnB) {
    bool old = g_btnB;
    g_btnB = rawB;
    if (g_btnB && !old) g_btnBPressEvent = true;
  }
  if ((now - g_btnCLastChangeMs) >= BTN_DEBOUNCE_MS && rawC != g_btnC) {
    bool old = g_btnC;
    g_btnC = rawC;
    if (g_btnC && !old) g_btnCPressEvent = true;
  }
}

static void handleButtonActions() {
  bool doA = false;
  bool doB = false;
  bool doC = false;

  if (g_btnAPressEvent) { g_btnAPressEvent = false; doA = true; }
  if (g_btnBPressEvent) { g_btnBPressEvent = false; doB = true; }
  if (g_btnCPressEvent) { g_btnCPressEvent = false; doC = true; }

  if (!doA && !doB && !doC) return;

  uint32_t now = millis();
  if (now - g_lastBtnActionMs < BTN_ACTION_LOCKOUT_MS) return;
  g_lastBtnActionMs = now;

  if (doB) {
    toggleBacklightOffRestore();
    Serial.print("[BTN] USR2 toggle brightness pwm=");
    Serial.print((unsigned)currentBacklightPwm());
    Serial.print(" label=");
    Serial.print(currentBacklightPercent());
    Serial.println("%");
    return;
  }

  if (doA) {
    cycleBacklightLevel();
    Serial.print("[BTN] USR1 cycle brightness pwm=");
    Serial.print((unsigned)currentBacklightPwm());
    Serial.print(" label=");
    Serial.print(currentBacklightPercent());
    Serial.println("%");
    return;
  }

  if (doC) {
    g_headerShowSeeed = !g_headerShowSeeed;
    drawHeaderTitle();
    Serial.print("[BTN] USR3 header=");
    Serial.println(g_headerShowSeeed ? "Seeed" : "Hello,XIAO!");
  }
}

// ========================= Battery =========================

static uint16_t readBatteryMilliVolts() {
  return (uint16_t)analogReadMilliVolts(BAT_ADC_PIN);
}

static BatteryState readBatteryMeasured() {
  BatteryState r;

  uint32_t rawSum = 0;
  uint32_t mvSum = 0;
  uint16_t rawMin = 65535;
  uint16_t rawMax = 0;
  uint16_t mvMin = 65535;
  uint16_t mvMax = 0;

  for (uint8_t i = 0; i < BAT_SAMPLE_COUNT; i++) {
    uint16_t raw = (uint16_t)analogRead(BAT_ADC_PIN);
    uint16_t mv = readBatteryMilliVolts();

    rawSum += raw;
    mvSum += mv;

    if (raw < rawMin) rawMin = raw;
    if (raw > rawMax) rawMax = raw;
    if (mv < mvMin) mvMin = mv;
    if (mv > mvMax) mvMax = mv;

    delayMicroseconds(BAT_SAMPLE_DELAY_US);
  }

  r.raw = rawSum / BAT_SAMPLE_COUNT;
  r.rawMin = rawMin;
  r.rawMax = rawMax;

  r.mv = mvSum / BAT_SAMPLE_COUNT;
  r.mvMin = mvMin;
  r.mvMax = mvMax;
  r.spreadMv = mvMax - mvMin;

  r.vadc = r.mv / 1000.0f;
  r.rawVbat = r.vadc * BAT_DIVIDER_RATIO * BAT_CAL_FACTOR;
  r.vbat = r.rawVbat;
  r.percent = lipoPercent(r.vbat);
  r.valid = false;
  r.state = "MEASURED";

  return r;
}

static int updatePresencePercent(int measuredPercent, bool freshBattery) {
  measuredPercent = constrain(measuredPercent, 0, 100);
  uint32_t now = millis();

  if (freshBattery || g_batDisplayPercent < 0) {
    g_batDisplayPercent = measuredPercent;
    g_batLastPctRiseMs = now;
    return g_batDisplayPercent;
  }

  if (measuredPercent < g_batDisplayPercent) {
    g_batDisplayPercent = measuredPercent;
    return g_batDisplayPercent;
  }

  if (measuredPercent > g_batDisplayPercent &&
      (now - g_batLastPctRiseMs >= BAT_PCT_RISE_INTERVAL_MS)) {
    g_batDisplayPercent++;
    g_batLastPctRiseMs = now;
  }

  return g_batDisplayPercent;
}

static bool isImpossibleHighJump(const BatteryState &m) {
  if (!g_haveLastGoodBat) return false;

  float dv = m.rawVbat - g_lastGoodBat.rawVbat;
  int dp = m.percent - g_lastGoodBat.percent;

  return (dv >= BAT_IMPOSSIBLE_JUMP_V) || (dp >= BAT_IMPOSSIBLE_JUMP_PCT);
}

static bool isColdBootUsbSuspect(const BatteryState &m, bool stable) {
  if (!BAT_COLD_BOOT_HIGH_AS_USB) return false;
  if (g_haveLastGoodBat) return false;
  if (!stable) return false;
  return (m.percent >= BAT_COLD_BOOT_SUSPECT_PCT);
}

static void markBatteryAbsentLikeUsb(const BatteryState &measured, const char *stateName) {
  g_batUsbOnlySeen = true;
  g_batUsbInsertCompActive = false;
  g_batUsbInsertRawStartV = 0.0f;

  g_bat = measured;
  g_bat.valid = false;
  g_bat.percent = 0;
  g_bat.vbat = measured.rawVbat;
  g_bat.state = stateName;
  g_batFilterState = stateName;

  cache_bat = "";
  cache_batIconState = -1;
}

static float batteryEffectiveVoltageForPercent(float rawV, bool freshBattery) {
  if (freshBattery && g_batUsbOnlySeen && !g_batUsbInsertCompActive) {
    g_batUsbInsertCompActive = true;
    g_batUsbInsertRawStartV = rawV;

    Serial.print("[BAT] USB-first battery insert compensation ON rawStart=");
    Serial.println(g_batUsbInsertRawStartV, 3);
  }

  if (g_batUsbInsertCompActive) {
    if (g_batUsbInsertRawStartV > 0.0f &&
        rawV <= (g_batUsbInsertRawStartV - BAT_USB_INSERT_RELEASE_DROP_V)) {
      g_batUsbInsertCompActive = false;

      Serial.print("[BAT] USB-insert compensation OFF raw=");
      Serial.println(rawV, 3);

      return rawV;
    }

    float compensated = rawV - BAT_USB_INSERT_SURFACE_COMP_V;
    if (compensated < BAT_EFFECTIVE_MIN_V) compensated = BAT_EFFECTIVE_MIN_V;
    return compensated;
  }

  return rawV;
}

static void updateBattery() {
  BatteryState measured = readBatteryMeasured();

  // UI reports voltage sense data, not battery presence.
  // Keep raw measured values before BAT/USB filtering or percentage smoothing.
  g_vsense = measured;

  bool plausible = (measured.mv >= BAT_PRESENT_MIN_MV &&
                    measured.rawVbat >= BAT_VALID_MIN_V &&
                    measured.rawVbat <= BAT_VALID_MAX_V);

  bool stable = plausible && (measured.spreadMv <= BAT_FLOAT_RANGE_MV);

  if (isColdBootUsbSuspect(measured, stable)) {
    if (g_batColdUsbSuspectStreak < 255) g_batColdUsbSuspectStreak++;

    if (g_batColdUsbSuspectStreak >= BAT_VALID_CONFIRM_COUNT) {
      g_batValidStreak = 0;
      if (g_batInvalidStreak < 255) g_batInvalidStreak++;
      markBatteryAbsentLikeUsb(measured, "USB_BOOT_HIGH");
      return;
    }
  } else {
    if (g_batColdUsbSuspectStreak > 0) g_batColdUsbSuspectStreak--;
  }

  if (g_batFloatReject && stable) {
    bool looksLikeReinsert = (g_batLastRealV > 0.0f) &&
                             (measured.rawVbat <= g_batLastRealV + BAT_REINSERT_DROP_V);

    if (!looksLikeReinsert) {
      g_batValidStreak = 0;
      if (g_batInvalidStreak < 255) g_batInvalidStreak++;
      markBatteryAbsentLikeUsb(measured, "FLOAT_LOCK");
      return;
    }

    if (g_batReinsertStreak < 255) g_batReinsertStreak++;

    if (g_batReinsertStreak < BAT_VALID_CONFIRM_COUNT) {
      markBatteryAbsentLikeUsb(measured, "REINSERT_CAND");
      return;
    }

    g_batFloatReject = false;
    g_batFloatRejectStreak = 0;
    g_batReinsertStreak = 0;
    g_batColdUsbSuspectStreak = 0;
    g_haveLastGoodBat = false;
    g_batDisplayPercent = -1;
  }

  if (stable && isImpossibleHighJump(measured)) {
    if (g_batFloatRejectStreak < 255) g_batFloatRejectStreak++;

    if (g_batFloatRejectStreak >= BAT_FLOAT_REJECT_STREAK) {
      g_batFloatReject = true;
      g_batReinsertStreak = 0;
      g_batValidStreak = 0;
      if (g_batInvalidStreak < 255) g_batInvalidStreak++;
      markBatteryAbsentLikeUsb(measured, "JUMP_FLOAT");
      return;
    }
  } else {
    if (g_batFloatRejectStreak > 0) g_batFloatRejectStreak--;
  }

  if (stable) {
    bool freshBattery = !g_haveLastGoodBat;

    if (g_batValidStreak < 255) g_batValidStreak++;
    g_batInvalidStreak = 0;

    measured.valid = true;

    float effectiveV = batteryEffectiveVoltageForPercent(measured.rawVbat, freshBattery);

    if (g_haveLastGoodBat) {
      effectiveV = g_lastGoodBat.vbat * 0.88f + effectiveV * 0.12f;
    }

    measured.vbat = effectiveV;
    measured.percent = updatePresencePercent(lipoPercent(measured.vbat), freshBattery);

    if (g_batValidStreak >= BAT_VALID_CONFIRM_COUNT) {
      g_bat = measured;
      g_bat.state = g_batUsbInsertCompActive ? "BAT_USB_COMP" : "BAT_PRESENT";
      g_batFilterState = g_bat.state;

      g_lastGoodBat = g_bat;
      g_lastGoodBat.valid = true;
      g_haveLastGoodBat = true;

      g_batLastRealV = g_bat.vbat;
      g_batLastRealPercent = g_bat.percent;

      if (!g_batUsbOnlySeen) {
        g_batUsbInsertCompActive = false;
        g_batUsbInsertRawStartV = 0.0f;
      }
    } else if (g_haveLastGoodBat) {
      g_bat = g_lastGoodBat;
      g_bat.state = "BAT_HOLD";
      g_batFilterState = g_bat.state;
    } else {
      g_bat = measured;
      g_bat.state = "BAT_CAND";
      g_batFilterState = g_bat.state;
    }

    return;
  }

  g_batValidStreak = 0;
  if (g_batInvalidStreak < 255) g_batInvalidStreak++;

  if (!g_batFloatReject && g_haveLastGoodBat && g_batInvalidStreak < BAT_INVALID_CONFIRM_COUNT) {
    g_bat = g_lastGoodBat;
    g_bat.valid = true;
    g_bat.state = "BAT_HOLD";
    g_batFilterState = g_bat.state;
    return;
  }

  markBatteryAbsentLikeUsb(measured, plausible ? "FLOAT" : "USB_PWR");
}

static bool isUsbPowerTextMode() {
  return !g_bat.valid;
}

// ========================= MIC =========================

static void deinitMic() {
#if ESP_IDF_VERSION_MAJOR >= 5
  if (g_i2sRxChan) {
    i2s_channel_disable(g_i2sRxChan);
    i2s_del_channel(g_i2sRxChan);
    g_i2sRxChan = nullptr;
  }
#else
  i2s_driver_uninstall(I2S_PORT);
#endif
  g_micOk = false;
}

static bool initMic() {
  deinitMic();

#if ESP_IDF_VERSION_MAJOR >= 5
  i2s_chan_config_t chanCfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
  chanCfg.dma_desc_num = 4;
  chanCfg.dma_frame_num = MIC_SAMPLES_PER_READ;

  esp_err_t err = i2s_new_channel(&chanCfg, nullptr, &g_i2sRxChan);
  if (err != ESP_OK) {
    Serial.printf("[MIC] i2s_new_channel failed: %d\n", (int)err);
    return false;
  }

  i2s_pdm_rx_config_t pdmCfg = {};
  pdmCfg.clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(MIC_SAMPLE_RATE_HZ);
  pdmCfg.slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
  pdmCfg.gpio_cfg.clk = (gpio_num_t)MIC_CLK_PIN;
  pdmCfg.gpio_cfg.din = (gpio_num_t)MIC_DATA_PIN;
  pdmCfg.gpio_cfg.invert_flags.clk_inv = MIC_CLK_INVERT;

  err = i2s_channel_init_pdm_rx_mode(g_i2sRxChan, &pdmCfg);
  if (err != ESP_OK) {
    Serial.printf("[MIC] init_pdm failed: %d\n", (int)err);
    deinitMic();
    return false;
  }

#if REDUCE_PDM_CLK_DRIVE
  gpio_set_drive_capability((gpio_num_t)MIC_CLK_PIN, GPIO_DRIVE_CAP_0);
#endif

  err = i2s_channel_enable(g_i2sRxChan);
  if (err != ESP_OK) {
    Serial.printf("[MIC] enable failed: %d\n", (int)err);
    deinitMic();
    return false;
  }
#else
  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM);
  cfg.sample_rate = MIC_SAMPLE_RATE_HZ;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_I2S;
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count = 4;
  cfg.dma_buf_len = MIC_SAMPLES_PER_READ;

  esp_err_t err = i2s_driver_install(I2S_PORT, &cfg, 0, nullptr);
  if (err != ESP_OK) return false;

  i2s_pin_config_t pins = {};
  pins.bck_io_num = I2S_PIN_NO_CHANGE;
  pins.ws_io_num = MIC_CLK_PIN;
  pins.data_out_num = I2S_PIN_NO_CHANGE;
  pins.data_in_num = MIC_DATA_PIN;

  if (i2s_set_pin(I2S_PORT, &pins) != ESP_OK) return false;
#endif

  g_micOk = true;
  Serial.println("[MIC] OK");
  return true;
}

static bool readMic(size_t *bytesRead, uint32_t timeoutMs) {
  *bytesRead = 0;

#if ESP_IDF_VERSION_MAJOR >= 5
  if (!g_i2sRxChan) return false;
  esp_err_t err = i2s_channel_read(g_i2sRxChan, g_pdmBuf, sizeof(g_pdmBuf), bytesRead, pdMS_TO_TICKS(timeoutMs));
#else
  esp_err_t err = i2s_read(I2S_PORT, g_pdmBuf, sizeof(g_pdmBuf), bytesRead, pdMS_TO_TICKS(timeoutMs));
#endif

  return (err == ESP_OK && *bytesRead > 0);
}

static AudioStats statsOf(const int16_t *samples, uint32_t count) {
  AudioStats s = {};
  if (!count) return s;

  int64_t sum = 0;
  for (uint32_t i = 0; i < count; i++) {
    sum += samples[i];
  }

  s.mean = (int32_t)(sum / count);

  uint32_t peak = 0;
  uint64_t sq = 0;
  for (uint32_t i = 0; i < count; i++) {
    int32_t v = (int32_t)samples[i] - s.mean;
    uint32_t a = v < 0 ? (uint32_t)(-v) : (uint32_t)v;
    if (a > peak) peak = a;
    sq += (uint64_t)a * a;
  }

  s.peak = peak;
  s.rms = (uint32_t)sqrt((double)sq / count);
  return s;
}

static void updateMic() {
  if (!g_micOk) return;

  size_t bytesRead = 0;
  if (!readMic(&bytesRead, 2)) return;

  uint32_t samples = bytesRead / sizeof(int16_t);
  if (samples > 0) {
    g_micStats = statsOf(g_pdmBuf, samples);
    g_micLastUpdateMs = millis();
  }
}

static uint32_t currentMicPeak() {
  uint32_t now = millis();

  if (now - g_micLastUpdateMs > 120) {
    g_micStats.peak = (uint32_t)(g_micStats.peak * 0.80f);
    g_micStats.rms = (uint32_t)(g_micStats.rms * 0.80f);
  }

  return g_micStats.peak;
}

// ========================= UI update =========================

static uint16_t vuColorForIndex(int i) {
  if (i >= 10) return C_RED;
  if (i >= 8) return C_ORANGE;
  if (i >= 6) return C_YELLOW;
  return C_GREEN;
}

static String voltageRowText(float d16, float calc) {
  char buf[32];
  // 20 chars exactly: compact enough to stay inside the SYS card.
  snprintf(buf, sizeof(buf), "D16 %.2fV|Calc %.2fV", d16, calc);
  return String(buf);
}

static void updateVu() {
  if (!g_lcdOk) return;

  float target = (float)currentMicPeak() / MIC_DISPLAY_SCALE;
  if (target < 0.0f) target = 0.0f;
  if (target > 1.0f) target = 1.0f;

  if (target > g_vuSmooth) {
    g_vuSmooth = g_vuSmooth * 0.50f + target * 0.50f;
  } else {
    g_vuSmooth = g_vuSmooth * 0.82f + target * 0.18f;
  }

  int segs = (int)roundf(g_vuSmooth * VU_SEG_COUNT);
  if (segs < 0) segs = 0;
  if (segs > VU_SEG_COUNT) segs = VU_SEG_COUNT;
  if (segs == g_cachedVuSegments) return;

  for (int i = 0; i < VU_SEG_COUNT; ++i) {
    int x = VU_X + 2 + i * (VU_SEG_W + VU_GAP);
    uint16_t c = (i < segs) ? vuColorForIndex(i) : C_BLACK;
    display.fillRect(x, VU_Y + 2, VU_SEG_W, VU_H - 4, c);
  }

  g_cachedVuSegments = segs;
}

static void updateUiFast() {
  char buf[64];

  // D16 voltage row:
  // Draw one compact row and clear the whole row area first. This avoids both
  // right-edge overflow and colored pixel residue from previous longer strings.
  if (millis() - g_lastVsenseUiMs >= VSENSE_UI_MS || cache_bat.length() == 0) {
    g_lastVsenseUiMs = millis();

    float d16Now = roundf(g_vsense.vadc * 100.0f) / 100.0f;
    float calcNow = roundf(g_vsense.rawVbat * 100.0f) / 100.0f;

    bool needDraw = cache_bat.length() == 0 ||
                    (g_d16ShownVadc < 0.0f) ||
                    (g_d16ShownCalc < 0.0f) ||
                    (fabsf(d16Now - g_d16ShownVadc) >= VSENSE_D16_DELTA_V) ||
                    (fabsf(calcNow - g_d16ShownCalc) >= VSENSE_CALC_DELTA_V);

    if (needDraw && g_lcdOk) {
      String rowText = voltageRowText(d16Now, calcNow);

      display.fillRect(VSENSE_ROW_X, ROW_BAT - 2, VSENSE_ROW_W, 14, C_BLACK);
      display.setTextSize(1);
      display.setTextColor(C_YELLOW, C_BLACK);
      display.setCursor(VSENSE_ROW_X, ROW_BAT);
      display.print(padRight(rowText, VSENSE_ROW_CHARS));

      cache_bat = rowText;
      cache_d16_adc = rowText;
      cache_d16_calc = rowText;
      g_d16ShownVadc = d16Now;
      g_d16ShownCalc = calcNow;
    }
  }

  // Voltage sense only: no BAT percentage, no USB PWR text, no battery icon.
  cache_batIconState = -1;

  String groveText;
  if (g_i2cCount == 0) {
    groveText = "none";
  } else {
    char t[24];
    snprintf(t, sizeof(t), "%u 0x%02X", g_i2cCount, g_i2cDevices[0]);
    groveText = String(t);
  }
  if (groveText != cache_grove) {
    cache_grove = groveText;
    printTextFixed(50, ROW_GROVE, C_CYAN, groveText, 11);
  }

  snprintf(buf, sizeof(buf), "%+.1f %+.1f %+.1f", g_ax, g_ay, g_az);
  String accText = g_imuOk ? String(buf) : String("not found");
  if (accText != cache_acc) {
    cache_acc = accText;
    printTextFixed(28, ROW_ACC, g_imuOk ? C_WHITE : C_RED, accText, 14);
  }

  snprintf(buf, sizeof(buf), "%+.0f %+.0f %+.0f", g_gx, g_gy, g_gz);
  String gyrText = g_imuOk ? String(buf) : String("not found");
  if (gyrText != cache_gyr) {
    cache_gyr = gyrText;
    printTextFixed(28, ROW_GYR, g_imuOk ? C_WHITE : C_RED, gyrText, 14);
  }

  String tapText = String("Tap ") + String(g_doubleTapCount);
  if (tapText != cache_tap) {
    cache_tap = tapText;
    printTextFixed(90, ROW_TAP, C_YELLOW, tapText, 6);
  }

  uint32_t peak = currentMicPeak();
  snprintf(buf, sizeof(buf), "Raw %u", (unsigned)peak);
  String micText = String(buf);
  if (micText != cache_micRaw) {
    cache_micRaw = micText;
    uint16_t rawColor = C_WHITE;
    if (peak > 1900) rawColor = C_RED;
    else if (peak > 1200) rawColor = C_ORANGE;
    printTextFixed(14, ROW_MIC_RAW, rawColor, micText, 18);
  }

  String blText = String("Brightness ") + String(currentBacklightPercent()) + "%";
  if (currentBacklightPwm() == 0) blText = "Brightness OFF";
  if (blText != cache_bl) {
    cache_bl = blText;
    uint16_t c = currentBacklightPwm() == 0 ? C_RED : C_CYAN;
    // Match the 1.47 Dashboard wording and keep it left-aligned in the card.
    printTextFixed(14, ROW_BL, c, blText, 18);
  }
}

static void printSerialStatus() {
  Serial.print("[DASH114_S3] mic=");
  Serial.print((unsigned)currentMicPeak());
  Serial.print(" acc=(");
  Serial.print(g_ax, 2); Serial.print(","); Serial.print(g_ay, 2); Serial.print(","); Serial.print(g_az, 2); Serial.print(")");
  Serial.print(" gyr=(");
  Serial.print(g_gx, 2); Serial.print(","); Serial.print(g_gy, 2); Serial.print(","); Serial.print(g_gz, 2); Serial.print(")");
  Serial.print(" i2c=");
  Serial.print((unsigned)g_i2cCount);
  Serial.print(" d16_adc=");
  Serial.print(g_vsense.vadc, 3);
  Serial.print("V calc=");
  Serial.print(g_vsense.rawVbat, 3);
  Serial.print("V raw=");
  Serial.print(g_vsense.raw);
  Serial.print(" mv=");
  Serial.print(g_vsense.mv);
  Serial.print(" spread=");
  Serial.print((unsigned)g_vsense.spreadMv);
  Serial.print(" vstate=");
  Serial.print(g_batFilterState);
  Serial.print(" tapCfg=");
  Serial.print(g_imuTapConfigured ? "Y" : "N");
  Serial.print(" tap=");
  Serial.print((unsigned long)g_doubleTapCount);
  Serial.print(" bl=");
  Serial.print((unsigned)currentBacklightPwm());
  Serial.print("/");
  Serial.print(currentBacklightPercent());
  Serial.print("%");
  Serial.print(" usr1=");
  Serial.print(g_btnA ? "P" : "R");
  Serial.print(" usr2=");
  Serial.print(g_btnB ? "P" : "R");
  Serial.print(" usr3=");
  Serial.println(g_btnC ? "P" : "R");
}

// ========================= Setup / loop =========================

static void printHeader() {
  Serial.println();
  Serial.println("=== XIAO ESP32-S3 Plus 1.14 Factory Dashboard v1.2.3 D16-compact-row ===");
  Serial.println("nRF52 visual parity + ESP32-S3 D16 voltage sense display.");
  Serial.printf("LCD: Seeed_GFX ST7789 %dx%d CS=D2(%d) DC=D3(%d) SCK=D8(%d) MOSI=D10(%d) RST=%d BL=%d\n",
                LCD_W, LCD_H,
                LCD_CS_PIN, LCD_DC_PIN, LCD_SCK_PIN, LCD_MOSI_PIN, LCD_RST_PIN, LCD_BL_PIN);
  Serial.printf("MIC: CLK=D0(%d), DATA=D1(%d), sample=%d\n",
                MIC_CLK_PIN, MIC_DATA_PIN, MIC_SAMPLE_RATE_HZ);
  Serial.printf("I2C: SDA=D4(%d), SCL=D5(%d), IMU_INT=D14(%d)\n",
                I2C_SDA_PIN, I2C_SCL_PIN, IMU_INT_PIN);
  Serial.printf("USR: USR1=D6(%d), USR2=D7(%d), USR3=D19(%d)\n",
                BTN_A_PIN, BTN_B_PIN, BTN_C_PIN);
}

void setup() {
  Serial.begin(115200);
  delay(900);

  WiFi.mode(WIFI_OFF);
  WiFi.disconnect(true);

  printHeader();

  pinMode(BTN_A_PIN, INPUT_PULLUP);
  pinMode(BTN_B_PIN, INPUT_PULLUP);
  pinMode(BTN_C_PIN, INPUT_PULLUP);

  g_btnARaw = digitalRead(BTN_A_PIN);
  g_btnBRaw = digitalRead(BTN_B_PIN);
  g_btnCRaw = digitalRead(BTN_C_PIN);
  g_btnALastRawPressed = (g_btnARaw == LOW);
  g_btnBLastRawPressed = (g_btnBRaw == LOW);
  g_btnCLastRawPressed = (g_btnCRaw == LOW);
  g_btnA = g_btnALastRawPressed;
  g_btnB = g_btnBLastRawPressed;
  g_btnC = g_btnCLastRawPressed;
  g_btnALastChangeMs = millis();
  g_btnBLastChangeMs = millis();
  g_btnCLastChangeMs = millis();

  pinMode(IMU_INT_PIN, INPUT_PULLUP);

  analogReadResolution(12);
#if defined(ADC_11db)
  analogSetPinAttenuation(BAT_ADC_PIN, ADC_11db);
#endif

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(400000);
  Wire.setTimeOut(8);      // short timeout: avoids long blocking during Grove hot-plug

  initLcd();
  drawStaticLayout();

  scanI2cBus();
  initImu();
  initMic();

  updateButtons();
  updateImu();
  updateMic();
  updateBattery();
  updateUiFast();
  updateVu();

  Serial.println("[BOOT] 1.14 ESP32-S3 dashboard v1.2.3 D16-compact-row ready");
}

void loop() {
  uint32_t now = millis();

  g_frameCounter++;

  handleImuTapEvent();
  updateButtons();
  handleButtonActions();
  updateMic();

  if (now - g_lastVuMs >= UI_VU_MS) {
    g_lastVuMs = now;
    updateVu();
  }

  if (now - g_lastFastUiMs >= UI_FAST_MS) {
    g_lastFastUiMs = now;
    updateUiFast();
  }

  if (now - g_lastSlowUiMs >= UI_SLOW_MS) {
    g_lastSlowUiMs = now;
    updateImu();
  }

  if (now - g_lastI2cMs >= I2C_SCAN_MS) {
    g_lastI2cMs = now;
    scanI2cBus();
  }

  if (now - g_lastBatMs >= BAT_REFRESH_MS) {
    g_lastBatMs = now;
    updateBattery();
  }

  if (now - g_lastSerialMs >= SERIAL_MS) {
    g_lastSerialMs = now;
    printSerialStatus();
  }

  delay(2);
}
