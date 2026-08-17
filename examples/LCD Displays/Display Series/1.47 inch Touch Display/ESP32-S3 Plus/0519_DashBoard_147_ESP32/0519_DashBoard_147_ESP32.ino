/**
 * Product: XIAO 1.47 inch Touch Display (JD9853A 172x320 + AXS5106L touch)
 * Display: JD9853A 172x320, BGR, capacitive touch
 * Target:  XIAO ESP32-S3 Plus (RST=13, BL=12). For nRF52840 Plus use the sibling
 *          nRF52840 Plus folder (RST=38, BL=37).
 * Wiring:  CS=D2, DC=D3, MOSI=D10, SCLK=D8; RST=13, BL=12.
 *          Touch SDA=D4, SCL=D5, INT=D7 (I2C 0x63, RST shared with LCD).
 * Demo:    1.47 ESP32-S3 factory dashboard: live SD/touch/IMU/mic/buttons rows + VU meter;
 *          SD probed in a background FreeRTOS task.
 *
 * Ported from XIAO-Display-Board-main (0519_DashBoard_147_ESP32.ino, Arduino_GFX). The
 * Seeed_GFX Board/Config templates replace the original Arduino_SWSPI bus +
 * Arduino_ST7789 panel, the manual MADCTL 0x48 fix, and the BSP
 * esp_lcd_touch_axs5106l driver. Touch is now driven by Touch_AXS5106L +
 * display.attachTouch()/display.getTouch(); the raw-I2C touch decoders and the
 * bsp_touch_init/bsp_touch_read calls were dropped. Because Seeed_GFX drives the
 * LCD over the shared hardware SPI host (Bus_SPI), the obsolete software-SPI
 * recovery (restoreLcdSwSpiPins) is a no-op and SD.end() no longer calls SPI.end()
 * (that would tear down the host the LCD reuses). The g_lcdBusy/g_sdProbeBusy
 * serialization + CS management that actually arbitrates the shared bus is kept.
 */

#include <Arduino.h>
#include <Seeed_GFX.h>
#include "board/boards/XIAO_LCD_Board.h"
#include "driver/tft/Driver_JD9853A.h"
#include "panel/Panel_TFT.h"
#include "touch/Touch_AXS5106L.h"

#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include "esp_idf_version.h"

#if ESP_IDF_VERSION_MAJOR >= 5
  #include <driver/i2s_pdm.h>
#else
  #include <driver/i2s.h>
#endif

#include <driver/gpio.h>
#include <math.h>
#include <stdarg.h>
#include <esp_log.h>

Seeed_GFX display;

#if defined(ESP32)
// SD shares the LCD's HSPI host (SPI3) with CS arbitration (LCD_CS=D2 / SD_CS=D6).
// The LCD's Bus_SPI uses its own SPIClass(HSPI); this is a second SPIClass on the
// same host (spiStartBus returns a per-host singleton; beginTransaction serializes
// via the bus lock) so the SD does NOT use the default SPI (FSPI/SPI2), which would
// fight the LCD's HSPI for the shared D8/D10 GPIO mux (rectify #74).
SPIClass sdSpi(HSPI);
#endif

static constexpr int8_t LCD_RST_PIN = 13;  // XIAO ESP32-S3 Plus
static constexpr int8_t LCD_BL_PIN  = 12;

// This dashboard shares D8/D10 with the microSD socket and probes the card in
// the background. Keep HSPI, but use a rate with adequate signal margin instead
// of the generic 50 MHz XIAO LCD rate.
struct Config_DashBoard_147_ESP32_Board {
  static const char *name() { return "XIAO 1.47 inch Touch Display (Dashboard)"; }
  static BoardPinConfig pins() {
    return Config_XIAO_1inch47_Touch_LCD_Board<LCD_RST_PIN, LCD_BL_PIN>::pins();
  }
  static SpiBusConfig spi() {
    SpiBusConfig config = xiaoLcdSpiConfig();
    config.writeFrequency = 20000000;
    return config;
  }
};
using Board_DashBoard_147_ESP32 =
    ConfiguredSpiBoard<Config_DashBoard_147_ESP32_Board>;

// The shared reset has already been handled by the display board.
Touch_AXS5106L touch(-1, D7, Wire, 172, 320);

// ========================= Build switches =========================

#define REDUCE_PDM_CLK_DRIVE 1

// Critical for the live dynamic dashboard:
// SD and LCD share D8/D10. SD.begin()/SD.open() can reconfigure or hold those pins.
// SD is probed once in a background task, then SD.end() is called immediately.
// No automatic SD file write is done at boot.
#define SD_PROBE_ONLY_MODE 1
#define SD_BUTTON_WRITE_TEST 0
#define g_sdBusy g_sdProbeBusy

// ========================= Timing =========================

static constexpr uint32_t UI_TEXT_FAST_MS   = 130;
static constexpr uint32_t UI_TEXT_SLOW_MS   = 280;
static constexpr uint32_t UI_VU_MS          = 90;
static constexpr uint32_t UI_MIC_TEXT_MS    = 420;
static constexpr uint32_t SERIAL_MS         = 450;
static constexpr uint32_t SD_REFRESH_MS     = 900;    // async SD probe request interval
static constexpr uint32_t MIC_DECAY_MS      = 80;
static constexpr uint32_t FOOTER_MS         = 500;

// ========================= MIC config =========================

static constexpr int MIC_SAMPLE_RATE_HZ = 16000;
static constexpr bool MIC_CLK_INVERT = false;

// This is only display scaling, not recording gain.
static constexpr float MIC_DISPLAY_SCALE = 2200.0f;
static constexpr size_t MIC_SAMPLES_PER_READ = 256;

// ========================= Pins =========================

static constexpr uint8_t MIC_CLK_PIN   = D0;
static constexpr uint8_t MIC_DATA_PIN  = D1;
static constexpr uint8_t LCD_CS_PIN    = D2;
static constexpr uint8_t LCD_DC_PIN    = D3;
static constexpr uint8_t I2C_SDA_PIN   = D4;
static constexpr uint8_t I2C_SCL_PIN   = D5;
static constexpr uint8_t SD_CS_PIN     = D6;
static constexpr uint8_t TOUCH_INT_PIN = D7;
static constexpr uint8_t LCD_SCK_PIN   = D8;
static constexpr uint8_t SD_MISO_PIN   = D9;
static constexpr uint8_t LCD_MOSI_PIN  = D10;
static constexpr uint8_t IMU_INT_PIN   = D14;
static constexpr uint8_t BTN_B_PIN     = D15;
static constexpr uint8_t BAT_ADC_PIN   = D16;
static constexpr uint8_t BTN_A_PIN     = D19;

// ========================= Colors =========================

static constexpr uint16_t C_BLACK   = TFT_BLACK;
static constexpr uint16_t C_WHITE   = TFT_WHITE;
static constexpr uint16_t C_GREEN   = TFT_GREEN;     // was RGB565_LIGHTGREEN -> TFT_GREEN
static constexpr uint16_t C_RED     = TFT_RED;
static constexpr uint16_t C_CYAN    = TFT_CYAN;
static constexpr uint16_t C_YELLOW  = TFT_YELLOW;
static constexpr uint16_t C_GRAY    = 0x8410;
static constexpr uint16_t C_ORANGE  = 0xFD20;
static constexpr uint16_t C_DIM     = 0x2104;
static constexpr uint16_t C_PANEL   = 0x0841;
static constexpr uint16_t C_LINE    = 0x39E7;
static constexpr uint16_t C_ACCENT  = 0x04FF;
static constexpr uint16_t C_PANEL_2 = 0x1082;

// ========================= I2S object =========================

#if ESP_IDF_VERSION_MAJOR >= 5
static i2s_chan_handle_t g_i2sRxChan = nullptr;
#else
static constexpr i2s_port_t I2S_PORT = I2S_NUM_0;
#endif

// ========================= State =========================

struct Row { int y; };
static constexpr Row ROW_SD    {64};
static constexpr Row ROW_TOUCH {82};
static constexpr Row ROW_ACC   {122};
static constexpr Row ROW_GYR   {140};
static constexpr Row ROW_MIC_L {170};
static constexpr Row ROW_MIC_V {214};
static constexpr Row ROW_BTN1  {250};
static constexpr Row ROW_BTN2  {266};
static constexpr Row ROW_HINT  {282};
static constexpr Row ROW_TICK  {294};

static constexpr int VU_X = 14;
static constexpr int VU_Y = 190;
static constexpr int VU_W = 144;
static constexpr int VU_H = 18;
static constexpr int VU_SEG_COUNT = 12;
static constexpr int VU_GAP = 2;
static constexpr int VU_SEG_W = (VU_W - (VU_SEG_COUNT - 1) * VU_GAP) / VU_SEG_COUNT;

bool g_lcdOk = false;

bool g_sdMounted = false;
bool g_sdSessionOpen = false;
uint32_t g_sdOkFreq = 0;
uint64_t g_sdCardSizeMB = 0;
bool g_sdProbeDone = false;

// Async SD probe state. Full SD.begin() is done in a background task so the
// dashboard loop does not block and the LCD stays smooth.
volatile bool g_sdProbeBusy = false;
volatile bool g_sdProbeRequest = false;
volatile bool g_sdProbeResultReady = false;
volatile bool g_sdProbePresentResult = false;
volatile uint32_t g_sdProbeFreqResult = 0;
volatile uint32_t g_sdProbeSizeMbResult = 0;
uint8_t g_sdInsertStable = 0;
uint8_t g_sdRemoveStable = 0;

enum ImuType { IMU_NONE = 0, IMU_QMI8658, IMU_LSM6DS3 };
ImuType g_imuType = IMU_NONE;
uint8_t g_imuAddr = 0;
bool g_imuOk = false;
float g_ax = 0, g_ay = 0, g_az = 0;
float g_gx = 0, g_gy = 0, g_gz = 0;

bool g_touchFound = false;
uint8_t g_touchAddr = 0;
bool g_touchValid = false;
int g_touchX = -1;
int g_touchY = -1;
int g_touchRawX = -1;
int g_touchRawY = -1;
int g_touchIntRaw = HIGH;
uint8_t g_touchRawBytes[8] = {0};
uint32_t g_touchLastSeenMs = 0;
int g_touchLastX = -1;
int g_touchLastY = -1;

bool g_btnA = false;
bool g_btnB = false;
int g_btnARaw = HIGH;
int g_btnBRaw = HIGH;

bool g_micOk = false;
uint16_t g_micPeak = 0;
uint32_t g_micRms = 0;
uint32_t g_micLastUpdateMs = 0;
int16_t g_pdmBuf[MIC_SAMPLES_PER_READ];

uint32_t g_lastFastUiMs = 0;
uint32_t g_lastSlowUiMs = 0;
uint32_t g_lastVuMs = 0;
uint32_t g_lastMicTextMs = 0;
uint32_t g_lastSerialMs = 0;
uint32_t g_lastSdMs = 0;
uint32_t g_lastFooterMs = 0;

String cache_sd = "";
String cache_touch = "";
String cache_acc = "";
String cache_gyr = "";
String cache_btn1 = "";
String cache_btn2 = "";
String cache_mic = "";
String cache_footer = "";
String cache_tick = "";

float g_vuSmooth = 0.0f;
int cache_vuSegments = -1;
uint32_t g_frameCounter = 0;

// LCD/SD share the same physical SPI pins. The SD probe runs in a background task.
// These flags prevent the background task from touching the bus while a LCD draw is
// in progress, and prevent UI cache updates when a draw is skipped.
volatile bool g_lcdBusy = false;

// ========================= Logging =========================

static void logf(const char *fmt, ...) {
  char buf[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  Serial.print(buf);
}

// ========================= Helpers =========================

static String padRight(const String &s, int width) {
  String out = s;
  while ((int)out.length() < width) out += ' ';
  if ((int)out.length() > width) out = out.substring(0, width);
  return out;
}

// Seeed_GFX drives the LCD over the shared hardware SPI host (Bus_SPI), which owns
// the SCK/MOSI/DC/CS pin routing. The original sketch bit-banged the LCD with a
// software-SPI bus and had to force D8/D10/D3/D2 back to GPIO mode after SD.begin()
// re-attached them to the ESP32 SPI peripheral. That recovery is now obsolete and
// would actively corrupt the hardware-SPI pin matrix, so this is a no-op. Kept as
// a stub so the call sites in beginLcdOp/acquireForLcd/closeSdSessionAndRestoreLcd
// remain structurally identical to the source.
static void restoreLcdSwSpiPins() {
}

static bool beginLcdOp() {
  if (g_sdProbeBusy) return false;

  g_lcdBusy = true;

  // Race guard: SD task may have started after the first check.
  if (g_sdProbeBusy) {
    g_lcdBusy = false;
    return false;
  }

  // Bus_SPI owns LCD_CS and parks it HIGH; only deselect SD here (shared FSPI bus).
  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH);
  restoreLcdSwSpiPins();
  delayMicroseconds(2);
  return true;
}

static void endLcdOp() {
  // Bus_SPI::endWrite already parked LCD_CS HIGH; just release the busy flag.
  g_lcdBusy = false;
}

static bool lcdBusAvailable() {
  return !g_sdProbeBusy;
}

static void acquireForLcd() {
  // Bus_SPI owns LCD_CS (D2) and parks it HIGH between transactions; only SD_CS
  // is idled here to keep the SD card off the shared FSPI bus before an LCD op.
  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH);
  restoreLcdSwSpiPins();
  delayMicroseconds(2);
}

static void acquireForSd() {
  // Bus_SPI parks LCD_CS HIGH after each LCD transaction; only SD_CS needs idling.
  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH);
  delayMicroseconds(2);
}

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

static bool i2cRawRead(uint8_t addr, uint8_t *buf, size_t len) {
  size_t got = Wire.requestFrom((int)addr, (int)len);
  if (got != len) return false;

  for (size_t i = 0; i < len; i++) {
    buf[i] = Wire.read();
  }
  return true;
}

static void scanI2c() {
  Serial.print("[I2C] scan:");
  int count = 0;
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      Serial.printf(" 0x%02X", a);
      count++;
    }
  }
  if (!count) Serial.print(" none");
  Serial.println();
}

// ========================= LCD =========================

static bool initLcd() {
  if (!display.begin<
          Board_DashBoard_147_ESP32,
          Config_XIAO_1inch47_Touch_JD9853A>()) {
    Serial.println("[LCD] display.begin() failed");
    Serial.println(display.lastResult().message);
    g_lcdOk = false;
    return false;
  }

  acquireForLcd();
  display.fillScreen(C_BLACK);

  g_lcdOk = true;
  Serial.println("[LCD] OK");
  return true;
}

static bool printTextFixed(int x, int y, uint16_t color, const String &text, int widthChars) {
  if (!beginLcdOp()) return false;
  display.setCursor(x, y);
  display.setTextSize(1);
  display.setTextColor(color, C_BLACK);
  display.print(padRight(text, widthChars));
  endLcdOp();
  return true;
}

static void drawVuFrame() {
  if (!beginLcdOp()) return;
  display.drawRoundRect(VU_X, VU_Y, VU_W, VU_H, 4, C_LINE);
  for (int i = 0; i < VU_SEG_COUNT; ++i) {
    int x = VU_X + 1 + i * (VU_SEG_W + VU_GAP);
    display.fillRect(x, VU_Y + 1, VU_SEG_W, VU_H - 2, C_BLACK);
  }
  endLcdOp();
}

static void drawStaticLayout() {
  if (!beginLcdOp()) return;
  display.fillScreen(C_BLACK);

  // Premium dashboard UI:
  // keep "Hello,XIAO!" as the hero title, then group data into compact cards.
  display.setTextSize(2);
  display.setTextColor(C_GREEN, C_BLACK);
  display.setCursor(8, 5);
  display.print("Hello,XIAO!");

  // Top subtitle kept simple and bright.
  display.setTextSize(1);
  display.setTextColor(C_CYAN, C_BLACK);
  display.setCursor(8, 28);
  display.print("1.47 Inch Touch Display");

  display.drawFastHLine(8, 44, 156, C_LINE);

  // SYSTEM card
  display.drawRoundRect(6, 50, 160, 48, 6, C_CYAN);
  display.fillRect(10, 57, 3, 34, C_CYAN);
  display.setTextColor(C_CYAN, C_BLACK);
  display.setCursor(18, 54);
  display.print("SYSTEM");
  display.setTextColor(C_WHITE, C_BLACK);
  display.setCursor(18, ROW_SD.y);    display.print("SD");
  display.setCursor(18, ROW_TOUCH.y); display.print("Touch");

  // MOTION card
  display.drawRoundRect(6, 102, 160, 50, 6, C_YELLOW);
  display.fillRect(10, 112, 3, 32, C_YELLOW);
  display.setTextColor(C_YELLOW, C_BLACK);
  display.setCursor(18, 108);
  display.print("MOTION");
  display.setTextColor(C_WHITE, C_BLACK);
  display.setCursor(18, ROW_ACC.y); display.print("Acc");
  display.setCursor(18, ROW_GYR.y); display.print("Gyr");

  // AUDIO card
  display.drawRoundRect(6, 158, 160, 70, 6, C_GREEN);
  display.fillRect(10, 168, 3, 52, C_GREEN);
  display.setTextColor(C_GREEN, C_BLACK);
  display.setCursor(18, ROW_MIC_L.y);
  display.print("MIC LEVEL");

  // BUTTON card
  display.drawRoundRect(6, 236, 160, 42, 6, C_ACCENT);
  display.fillRect(10, 244, 3, 26, C_ACCENT);
  display.setTextColor(C_ACCENT, C_BLACK);
  display.setCursor(18, 240);
  display.print("BUTTON");
  display.setTextColor(C_WHITE, C_BLACK);
  display.setCursor(18, ROW_BTN1.y); display.print("USR1");
  display.setCursor(18, ROW_BTN2.y); display.print("USR2");

  display.setTextColor(C_CYAN, C_BLACK);
  display.setCursor(8, ROW_HINT.y); display.print("Serial synced output");

  // Lower footer product line: split into two compact lines so it is readable.
  display.setTextColor(C_WHITE, C_BLACK);
  display.setCursor(8, ROW_TICK.y); display.print("Powered by XIAO");

  display.setTextColor(C_YELLOW, C_BLACK);
  display.setCursor(8, ROW_TICK.y + 12); display.print("ESP32-S3 Plus");

  endLcdOp();

  drawVuFrame();

  cache_sd = "";
  cache_touch = "";
  cache_acc = "";
  cache_gyr = "";
  cache_btn1 = "";
  cache_btn2 = "";
  cache_mic = "";
  cache_footer = "";
  cache_tick = "";
  cache_vuSegments = -1;
}

// ========================= SD =========================

static void closeSdSessionAndRestoreLcd() {
  SD.end();
  // NOTE: do NOT call SPI.end() here. Seeed_GFX drives the LCD over the same
  // default SPI host (Bus_SPI), which is created once during display.begin() and
  // is not re-initialized per transaction. Tearing it down here would leave the
  // LCD bus dead until the next reboot. SD.end() releases the card; the shared
  // SPI host stays alive for the LCD.

  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH);
  restoreLcdSwSpiPins();
}

static bool sdFullMountProbe(uint32_t &okFreq, uint32_t &cardSizeMb) {
  // Full mount probe with no file access.
  // This is reliable for insert/remove, but runs in a background task.
  okFreq = 0;
  cardSizeMb = 0;

  // Do not steal the shared bus in the middle of a LCD write.
  while (g_lcdBusy) {
    vTaskDelay(pdMS_TO_TICKS(1));
  }

  acquireForSd();
  sdSpi.begin(LCD_SCK_PIN, SD_MISO_PIN, LCD_MOSI_PIN, SD_CS_PIN);
  delay(3);

  const uint32_t freqs[] = {400000};
  for (uint8_t i = 0; i < sizeof(freqs) / sizeof(freqs[0]); i++) {
    if (SD.begin(SD_CS_PIN, sdSpi, freqs[i])) {
      okFreq = freqs[i];
      uint64_t mb = SD.cardSize() / (1024ULL * 1024ULL);
      if (mb == 0) mb = 1;
      if (mb > 0xFFFFFFFFUL) mb = 0xFFFFFFFFUL;
      cardSizeMb = (uint32_t)mb;

      closeSdSessionAndRestoreLcd();
      return true;
    }

    SD.end();
    restoreLcdSwSpiPins();
    delay(2);
  }

  closeSdSessionAndRestoreLcd();
  return false;
}

static void sdProbeTask(void *param) {
  (void)param;

  for (;;) {
    if (g_sdProbeRequest && !g_sdProbeBusy) {
      g_sdProbeRequest = false;
      g_sdProbeBusy = true;

      uint32_t freq = 0;
      uint32_t sizeMb = 0;
      bool present = sdFullMountProbe(freq, sizeMb);

      g_sdProbePresentResult = present;
      g_sdProbeFreqResult = freq;
      g_sdProbeSizeMbResult = sizeMb;
      g_sdProbeResultReady = true;

      g_sdProbeBusy = false;
    }

    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

static void startSdProbeTask() {
  static bool started = false;
  if (started) return;
  started = true;

  xTaskCreatePinnedToCore(
    sdProbeTask,
    "sd_probe",
    4096,
    nullptr,
    1,
    nullptr,
    0
  );
}

static void requestSdProbe() {
  if (g_sdProbeBusy || g_sdProbeRequest) return;
  g_sdProbeRequest = true;
}

static void consumeSdProbeResult() {
  if (!g_sdProbeResultReady) return;

  bool present = g_sdProbePresentResult;
  uint32_t freq = g_sdProbeFreqResult;
  uint32_t sizeMb = g_sdProbeSizeMbResult;

  g_sdProbeResultReady = false;

  // Debounce:
  // - insert requires two positive probes.
  // - removal requires one negative probe for quick unplug response.
  if (present) {
    if (g_sdInsertStable < 3) g_sdInsertStable++;
    g_sdRemoveStable = 0;
  } else {
    if (g_sdRemoveStable < 3) g_sdRemoveStable++;
    g_sdInsertStable = 0;
  }

  bool oldState = g_sdMounted;

  if (present && g_sdInsertStable >= 2) {
    g_sdMounted = true;
    g_sdOkFreq = freq;
    g_sdCardSizeMB = sizeMb ? sizeMb : 1;
  }

  if (!present && g_sdRemoveStable >= 1) {
    g_sdMounted = false;
    g_sdOkFreq = 0;
    g_sdCardSizeMB = 0;
  }

  if (oldState != g_sdMounted) {
    cache_sd = "";  // force LCD row update immediately
  }

  Serial.printf("[SD] async result present=%u shown=%u freq=%lu size=%lluMB busy=%u\n",
                present ? 1 : 0,
                g_sdMounted ? 1 : 0,
                (unsigned long)g_sdOkFreq,
                g_sdCardSizeMB,
                g_sdProbeBusy ? 1 : 0);
}

static void updateSdStatus() {
  consumeSdProbeResult();
  requestSdProbe();
}

static void writeSdTestFileOnce() {
  // Dashboard mode: USR2 only requests an immediate async probe.
  requestSdProbe();
  Serial.println("[SD] async status refresh requested");
}

// ========================= Touch =========================

static bool i2cDevicePresent(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

static void initTouch() {
  pinMode(TOUCH_INT_PIN, INPUT_PULLUP);

  if (!i2cDevicePresent(AXS5106L_I2C_ADDR)) {
    g_touchFound = false;
    g_touchAddr = 0;
    Serial.println("[TOUCH] AXS5106L not found at 0x63");
    return;
  }

  // display.begin() has already pulsed the shared LCD/touch RST line, so the touch
  // controller is initialized. Attach with rstPin=-1 to avoid resetting the LCD.
  if (!display.attachTouch(touch, display.panel().driver().bus())) {
    Serial.println("[TOUCH] attachTouch failed");
    Serial.println(display.lastResult().message);
    g_touchFound = false;
    return;
  }

  g_touchFound = true;
  g_touchAddr = AXS5106L_I2C_ADDR;
  g_touchValid = false;
  g_touchX = -1;
  g_touchY = -1;

  Serial.println("[TOUCH] AXS5106L OK at 0x63 (Seeed_GFX driver)");
}

static void updateTouch() {
  g_touchValid = false;
  g_touchX = -1;
  g_touchY = -1;
  g_touchRawX = -1;
  g_touchRawY = -1;
  g_touchIntRaw = digitalRead(TOUCH_INT_PIN);

  for (int i = 0; i < 8; i++) {
    g_touchRawBytes[i] = 0;
  }

  if (!g_touchFound) {
    return;
  }

  // Seeed_GFX Touch_AXS5106L reads register 0x01 and clamps coordinates to the
  // 172x320 panel range, replacing the old BSP bsp_touch_read/get_coordinates
  // path and the raw-I2C packet decoders.
  int32_t x = 0;
  int32_t y = 0;
  if (display.getTouch(&x, &y)) {
    g_touchX = (int)x;
    g_touchY = (int)y;
    g_touchRawX = (int)x;
    g_touchRawY = (int)y;

    g_touchLastX = g_touchX;
    g_touchLastY = g_touchY;
    g_touchLastSeenMs = millis();

    g_touchValid = true;
    return;
  }

  // The driver is interrupt-driven. Some touch panels only fire INT on down/move,
  // so keep the latest coordinate visible briefly instead of flickering immediately
  // back to release.
  if (g_touchLastSeenMs != 0 && (millis() - g_touchLastSeenMs) < 350) {
    g_touchX = g_touchLastX;
    g_touchY = g_touchLastY;
    g_touchRawX = g_touchLastX;
    g_touchRawY = g_touchLastY;
    g_touchValid = true;
    return;
  }
}

// ========================= IMU =========================

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

static void initImu() {
  pinMode(IMU_INT_PIN, INPUT_PULLUP);

  g_imuType = IMU_NONE;
  g_imuOk = false;

  if (initQmi(0x6B)) return;
  if (initQmi(0x6A)) return;
  if (initLsm(0x6A)) return;
  if (initLsm(0x6B)) return;

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

// ========================= Buttons =========================

static void updateButtons() {
  g_btnARaw = digitalRead(BTN_A_PIN);
  g_btnBRaw = digitalRead(BTN_B_PIN);
  g_btnA = (g_btnARaw == LOW);
  g_btnB = (g_btnBRaw == LOW);
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

static void updateMicPeak() {
  if (!g_micOk) return;

  size_t bytesRead = 0;
  if (!readMic(&bytesRead, 2)) {
    return;
  }

  int samples = bytesRead / sizeof(int16_t);
  if (samples <= 0) return;

  int64_t sum = 0;
  for (int i = 0; i < samples; ++i) {
    sum += g_pdmBuf[i];
  }
  int32_t mean = (int32_t)(sum / samples);

  uint32_t peak = 0;
  uint64_t sq = 0;
  for (int i = 0; i < samples; ++i) {
    int32_t v = (int32_t)g_pdmBuf[i] - mean;
    uint32_t a = (v < 0) ? (uint32_t)(-v) : (uint32_t)v;
    if (a > peak) peak = a;
    sq += (uint64_t)a * (uint64_t)a;
  }

  g_micPeak = (uint16_t)((peak > 65535) ? 65535 : peak);
  g_micRms = (uint32_t)sqrt((double)sq / samples);
  g_micLastUpdateMs = millis();
}

static uint16_t currentMicPeak() {
  uint32_t now = millis();
  if (now - g_micLastUpdateMs > MIC_DECAY_MS) {
    g_micPeak = (uint16_t)(g_micPeak * 0.75f);
    g_micRms = (uint32_t)(g_micRms * 0.75f);
  }
  return g_micPeak;
}

static uint16_t vuColorForIndex(int idx) {
  if (idx >= 10) return C_RED;
  if (idx >= 7) return C_ORANGE;
  return C_GREEN;
}

// ========================= UI updates =========================

static void updateVuMeter() {
  uint16_t micPeak = currentMicPeak();

  float target = (float)micPeak / MIC_DISPLAY_SCALE;
  if (target < 0.0f) target = 0.0f;
  if (target > 1.0f) target = 1.0f;

  // attack faster, release slower
  if (target > g_vuSmooth) g_vuSmooth = g_vuSmooth * 0.55f + target * 0.45f;
  else g_vuSmooth = g_vuSmooth * 0.86f + target * 0.14f;

  int activeSegs = (int)roundf(g_vuSmooth * VU_SEG_COUNT);
  if (activeSegs < 0) activeSegs = 0;
  if (activeSegs > VU_SEG_COUNT) activeSegs = VU_SEG_COUNT;

  if (activeSegs == cache_vuSegments) return;

  if (!beginLcdOp()) return;
  for (int i = 0; i < VU_SEG_COUNT; ++i) {
    int x = VU_X + 1 + i * (VU_SEG_W + VU_GAP);
    uint16_t color = (i < activeSegs) ? vuColorForIndex(i) : C_BLACK;
    display.fillRect(x, VU_Y + 1, VU_SEG_W, VU_H - 2, color);
  }
  endLcdOp();

  cache_vuSegments = activeSegs;
}

static void updateMicText() {
  uint16_t micPeak = currentMicPeak();
  String micText = String("Raw ") + String((unsigned)micPeak);
  if (micText != cache_mic) {
    if (printTextFixed(18, ROW_MIC_V.y, C_WHITE, micText, 18)) {
      cache_mic = micText;
    }
  }
}

static void updateUiFast() {
  String sdText;
  if (g_sdMounted) {
    sdText = String("OK  ") + String(g_sdOkFreq / 1000) + "k";
  } else {
    sdText = "NO CARD";
  }

  if (sdText != cache_sd) {
    if (printTextFixed(52, ROW_SD.y, g_sdMounted ? C_GREEN : C_RED, sdText, 14)) {
      cache_sd = sdText;
    }
  }

  String touchText;
  if (!g_touchFound) {
    touchText = "not found";
  } else if (g_touchValid) {
    touchText = String(g_touchX) + "," + g_touchY;
  } else {
    touchText = "release";
  }

  if (touchText != cache_touch) {
    if (printTextFixed(66, ROW_TOUCH.y, g_touchFound ? C_CYAN : C_RED, touchText, 12)) {
      cache_touch = touchText;
    }
  }

  String btn1Text = g_btnA ? "Pressed " : "Released";
  if (btn1Text != cache_btn1) {
    if (printTextFixed(62, ROW_BTN1.y, g_btnA ? C_GREEN : C_WHITE, btn1Text, 11)) {
      cache_btn1 = btn1Text;
    }
  }

  String btn2Text = g_btnB ? "Pressed " : "Released";
  if (btn2Text != cache_btn2) {
    if (printTextFixed(62, ROW_BTN2.y, g_btnB ? C_GREEN : C_WHITE, btn2Text, 11)) {
      cache_btn2 = btn2Text;
    }
  }
}

static void updateUiSlow() {
  char buf[64];

  String accText;
  if (g_imuOk) {
    snprintf(buf, sizeof(buf), "%.1f %.1f %.1f", g_ax, g_ay, g_az);
    accText = String(buf);
  } else {
    accText = "not found";
  }

  if (accText != cache_acc) {
    if (printTextFixed(52, ROW_ACC.y, g_imuOk ? C_WHITE : C_RED, accText, 14)) {
      cache_acc = accText;
    }
  }

  String gyrText;
  if (g_imuOk) {
    snprintf(buf, sizeof(buf), "%.1f %.1f %.1f", g_gx, g_gy, g_gz);
    gyrText = String(buf);
  } else {
    gyrText = "not found";
  }

  if (gyrText != cache_gyr) {
    if (printTextFixed(52, ROW_GYR.y, g_imuOk ? C_WHITE : C_RED, gyrText, 14)) {
      cache_gyr = gyrText;
    }
  }
}

static void updateFooter() {
  // Footer is static in v6.6:
  // - "Serial synced output"
  // - "Powered by XIAO"
  // - "ESP32-S3 Plus"
}

static void printSerialStatus() {
  logf("[DASH] sd=%s freq=%lu size=%lluMB touch=%s x=%d y=%d imu=%s type=%d "
       "acc=(%.2f,%.2f,%.2f) gyr=(%.2f,%.2f,%.2f) micPeak=%u micRms=%lu "
       "usr1=%s raw1=%d usr2=%s raw2=%d tick=%lu touchAddr=0x%02X int=%d rawTouch=(%d,%d)\n",
       g_sdMounted ? "Inserted" : "Unplugged",
       (unsigned long)g_sdOkFreq,
       g_sdCardSizeMB,
       g_touchValid ? "Y" : "N",
       g_touchX, g_touchY,
       g_imuOk ? "OK" : "NO",
       (int)g_imuType,
       g_ax, g_ay, g_az,
       g_gx, g_gy, g_gz,
       (unsigned)currentMicPeak(),
       (unsigned long)g_micRms,
       g_btnA ? "Pressed" : "Released", g_btnARaw,
       g_btnB ? "Pressed" : "Released", g_btnBRaw,
       (unsigned long)g_frameCounter,
       g_touchAddr,
       g_touchIntRaw,
       g_touchRawX, g_touchRawY);
}

// ========================= Setup / loop =========================

static void printHeader() {
  Serial.println();
  Serial.println("=== XIAO ESP32-S3 Plus Factory Dynamic Dashboard v6.6 (Seeed_GFX) ===");
  Serial.println("dynamic raw-SD detect + AXS5106L touch coords (Seeed_GFX Touch_AXS5106L)");
  Serial.printf("LCD: Seeed_GFX Bus_SPI, CS=D2(%d), DC=D3(%d), SCK=D8(%d), MOSI=D10(%d), RST=%d, BL=%d\n",
                LCD_CS_PIN, LCD_DC_PIN, LCD_SCK_PIN, LCD_MOSI_PIN, LCD_RST_PIN, LCD_BL_PIN);
  Serial.printf("MIC: CLK=D0(%d), DATA=D1(%d), sample=%d\n", MIC_CLK_PIN, MIC_DATA_PIN, MIC_SAMPLE_RATE_HZ);
  Serial.printf("SD : CS=D6(%d), SCK=D8(%d), MISO=D9(%d), MOSI=D10(%d)\n", SD_CS_PIN, LCD_SCK_PIN, SD_MISO_PIN, LCD_MOSI_PIN);
  Serial.printf("I2C: SDA=D4(%d), SCL=D5(%d), TOUCH_INT=D7(%d), IMU_INT=D14(%d)\n", I2C_SDA_PIN, I2C_SCL_PIN, TOUCH_INT_PIN, IMU_INT_PIN);
  Serial.printf("BTN: USR1=%d, USR2=%d\n", BTN_A_PIN, BTN_B_PIN);
  Serial.println("TOUCH: Seeed_GFX Touch_AXS5106L (I2C 0x63) via display.attachTouch()");
  Serial.println("SD: async full-mount probe task with LCD bus guard");
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Avoid flooding serial with ESP-IDF I2C NACK diagnostics while probing touch.
  // We still report touch status in our own [DASH] line.
  esp_log_level_set("i2c.master", ESP_LOG_NONE);

  WiFi.mode(WIFI_OFF);
  WiFi.disconnect(true);

  printHeader();

  pinMode(BTN_A_PIN, INPUT_PULLUP);
  pinMode(BTN_B_PIN, INPUT_PULLUP);
  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH);

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(400000);

  scanI2c();

  initImu();

  if (!initLcd()) {
    Serial.println("[FAIL] LCD init failed");
    return;
  }

  // attachTouch must run after display.begin() has pulsed the shared LCD/touch RST.
  initTouch();

  drawStaticLayout();

  if (!initMic()) {
    Serial.println("[WARN] MIC init failed, VU may stay zero");
  }

  startSdProbeTask();
  Serial.println("[SD] async probe task started");
  requestSdProbe();

  updateTouch();
  updateImu();
  updateButtons();
  updateMicPeak();

  // Keep the base UI already drawn above; do not redraw it while the first SD
  // background probe might be using the bus.
  updateUiFast();
  updateUiSlow();
  updateVuMeter();
  updateMicText();
  updateFooter();
  printSerialStatus();

  Serial.println("[BOOT] dynamic dashboard v6.6 ready");
}

void loop() {
  uint32_t now = millis();

  // Update raw states as often as possible.
  updateMicPeak();

  // Full SD.begin() probe runs in a background task.
  // Main loop only consumes results and requests the next probe, so the UI stays smooth.
  consumeSdProbeResult();
  if (now - g_lastSdMs >= SD_REFRESH_MS) {
    g_lastSdMs = now;
    requestSdProbe();
  }

  if (now - g_lastSlowUiMs >= UI_TEXT_SLOW_MS) {
    g_lastSlowUiMs = now;
    updateImu();
    updateTouch();
    updateButtons();
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

  if (now - g_lastFooterMs >= FOOTER_MS) {
    g_lastFooterMs = now;
    updateFooter();
  }

  if (now - g_lastSerialMs >= SERIAL_MS) {
    g_lastSerialMs = now;
    printSerialStatus();
  }

  delay(1);
}
