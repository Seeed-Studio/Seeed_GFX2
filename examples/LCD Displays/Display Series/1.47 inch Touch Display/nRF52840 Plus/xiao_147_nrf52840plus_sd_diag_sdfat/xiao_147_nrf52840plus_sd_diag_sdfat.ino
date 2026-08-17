/**
 * Product: XIAO 1.47 inch Touch Display (JD9853A 172x320 + AXS5106L touch)
 * Display: JD9853A 172x320, BGR, capacitive touch
 * Target:  XIAO nRF52840 Plus (RST=38, BL=37). For ESP32-S3 Plus use the sibling
 *          ESP32-S3 Plus folder (RST=13, BL=12).
 * Wiring:  CS=D2, DC=D3, MOSI=D10, SCLK=D8; RST=38, BL=37. SD CS=D6 (shared SPI
 *          with LCD). PDM mic CLK=D0, DATA=D1. No touch.
 * Demo:    LCD + SD + PDM mic WAV recorder: mounts SD via SdFat SHARED_SPI,
 *          streams the PDM mic through a 4096-sample ring buffer to SD for 5s,
 *          patches the WAV header. No live LCD meter during recording.
 *
 * Ported from XIAO-Display-Board-main (xiao_147_nrf52840plus_sd_diag_sdfat,
 * Arduino_GFX). The Seeed_GFX Board/Config templates replace the original SW-SPI
 * bus+panel construction, manual hard-reset, backlight-on, and the MADCTL 0x48
 * fix (Config_XIAO_1inch47_Touch_JD9853A bakes orientation/color). The filename is
 * kept verbatim per instruction (the content is a recorder, not an SD diagnostic).
 * PDM (setPins D1/D0 + onReceive ring), SdFat SHARED_SPI SD, WAV header
 * write/patch, and the acquireForLcd/acquireForSd shared-SPI bus-idle guards are
 * preserved. MCU guard: #if defined(ARDUINO_ARCH_NRF52). No touch.
 */

#include <Seeed_GFX.h>
#include "board/boards/XIAO_LCD_Board.h"
#include "driver/tft/Driver_JD9853A.h"
#include "panel/Panel_TFT.h"

#include <SPI.h>
#include <SdFat.h>
#if defined(ARDUINO_ARCH_NRF52)
#include <PDM.h>
#endif
#include <stdarg.h>
#include <math.h>

Seeed_GFX display;

static constexpr int8_t LCD_RST_PIN = 38;  // XIAO nRF52840 Plus
static constexpr int8_t LCD_BL_PIN  = 37;

// ---------- pin map ----------
static constexpr uint8_t PDM_CLK_PIN   = D0;
static constexpr uint8_t PDM_DATA_PIN  = D1;
static constexpr uint8_t LCD_CS_PIN    = D2;
static constexpr uint8_t LCD_DC_PIN    = D3;
static constexpr uint8_t SD_CS_PIN     = D6;
static constexpr uint8_t LCD_SCK_PIN   = D8;
static constexpr uint8_t SD_MISO_PIN   = D9;   // informational
static constexpr uint8_t LCD_MOSI_PIN  = D10;

// ---------- audio config ----------
static constexpr int SAMPLE_RATE_HZ = 16000;
static constexpr int CHANNELS = 1;
static constexpr int RECORD_SECONDS = 5;
static constexpr int PDM_GAIN = 30;
static constexpr size_t PDM_CHUNK_SAMPLES = 256;
static constexpr size_t RING_SAMPLES = 4096;

// ---------- SD ----------
SdFat SD;
File recFile;

// ---------- audio buffer ----------
volatile int16_t g_ring[RING_SAMPLES];
volatile uint32_t g_ringWrite = 0;
volatile uint32_t g_ringRead = 0;
volatile uint32_t g_droppedSamples = 0;
volatile bool g_overflow = false;

int16_t g_pdmChunk[PDM_CHUNK_SAMPLES];
int16_t g_writeChunk[PDM_CHUNK_SAMPLES];

// ---------- colors ----------
static constexpr uint16_t C_BLACK = TFT_BLACK;
static constexpr uint16_t C_WHITE = TFT_WHITE;
static constexpr uint16_t C_GREEN = TFT_GREEN;   // source used RGB565_LIGHTGREEN
static constexpr uint16_t C_RED   = TFT_RED;
static constexpr uint16_t C_CYAN  = TFT_CYAN;
static constexpr uint16_t C_BLUE  = TFT_BLUE;
static constexpr uint16_t C_YELL  = TFT_YELLOW;

// ---------- WAV ----------
struct WavHeader {
  char riff[4];
  uint32_t chunkSize;
  char wave[4];
  char fmt[4];
  uint32_t subchunk1Size;
  uint16_t audioFormat;
  uint16_t numChannels;
  uint32_t sampleRate;
  uint32_t byteRate;
  uint16_t blockAlign;
  uint16_t bitsPerSample;
  char data[4];
  uint32_t subchunk2Size;
};

// ---------- utils ----------
static void logf(const char *fmt, ...) {
  char buf[192];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  Serial.print(buf);
}

static void fillWavHeader(WavHeader &h, uint32_t pcmBytes) {
  memcpy(h.riff, "RIFF", 4);
  memcpy(h.wave, "WAVE", 4);
  memcpy(h.fmt,  "fmt ", 4);
  memcpy(h.data, "data", 4);
  h.subchunk1Size = 16;
  h.audioFormat = 1;
  h.numChannels = CHANNELS;
  h.sampleRate = SAMPLE_RATE_HZ;
  h.bitsPerSample = 16;
  h.byteRate = SAMPLE_RATE_HZ * CHANNELS * (h.bitsPerSample / 8);
  h.blockAlign = CHANNELS * (h.bitsPerSample / 8);
  h.subchunk2Size = pcmBytes;
  h.chunkSize = 36 + pcmBytes;
}

static bool writeEmptyWavHeader(File &file) {
  WavHeader h;
  fillWavHeader(h, 0);
  return file.write(reinterpret_cast<const uint8_t *>(&h), sizeof(h)) == sizeof(h);
}

static bool patchWavHeader(File &file, uint32_t pcmBytes) {
  WavHeader h;
  fillWavHeader(h, pcmBytes);
  if (!file.seek(0)) return false;
  return file.write(reinterpret_cast<const uint8_t *>(&h), sizeof(h)) == sizeof(h);
}

// ---------- shared bus helpers ----------
// The LCD and SD share the same hardware SPI bus (SCK=D8, MOSI=D10). The Board
// template owns the LCD CS (D2) during display calls and SdFat SHARED_SPI owns
// the SD CS (D6) during SD calls; these helpers idle the OTHER device's CS high
// before each transaction so a stale low CS never lets the wrong device clock in
// garbage. Seeed_GFX does not manage SD CS, so the arbitration stays here.
static void gpioIdleState() {
  // LCD CS is (re)configured by display.begin<>() below; idle it high now so the
  // shared-SPI arbitration helpers start from a clean state.
  pinMode(LCD_CS_PIN, OUTPUT);
  digitalWrite(LCD_CS_PIN, HIGH);

  // SD CS is NOT managed by Seeed_GFX — idle it high so the SD card ignores LCD
  // traffic on the shared SCK/MOSI lines.
  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH);
}

static void acquireForLcd() {
  digitalWrite(SD_CS_PIN, HIGH);
  digitalWrite(LCD_CS_PIN, HIGH);
  delayMicroseconds(2);
}

static void acquireForSd() {
  digitalWrite(LCD_CS_PIN, HIGH);
  digitalWrite(SD_CS_PIN, HIGH);
  delayMicroseconds(2);
}

// ---------- LCD ----------
static bool initLcd() {
  // Idle the shared-SPI chip selects first (Board template drives RST+BL and the
  // LCD CS/DC, so the manual hard-reset / backlight-on / MADCTL fix from the
  // Arduino_GFX source are all dropped).
  gpioIdleState();

  if (!display.begin<Board_XIAO_1inch47_Touch_Display<LCD_RST_PIN, LCD_BL_PIN>,
                    Config_XIAO_1inch47_Touch_JD9853A>()) {
    Serial.println("[LCD] display.begin() failed");
    Serial.println(display.lastResult().message);
    return false;
  }

  display.fillScreen(C_BLACK);
  return true;
}

static void lcdLine(int y, uint16_t color, const String &text, int size = 1) {
  acquireForLcd();
  int h = (size == 1) ? 16 : 24;
  display.fillRect(0, y, 172, h, C_BLACK);
  display.setCursor(0, y);
  display.setTextSize(size);
  display.setTextColor(color, C_BLACK);
  display.print(text);
}

static void lcdTitle(const String &title, const String &sub) {
  acquireForLcd();
  display.fillScreen(C_BLACK);
  display.drawRect(0, 0, 172, 320, C_YELL);

  display.setCursor(0, 22);
  display.setTextSize(2);
  display.setTextColor(C_GREEN, C_BLACK);
  display.println(title);

  display.setCursor(0, 50);
  display.setTextSize(1);
  display.setTextColor(C_WHITE, C_BLACK);
  display.println(sub);
}

static void lcdColorBars() {
  acquireForLcd();
  display.fillRect(0,   90, 43, 18, C_RED);
  display.fillRect(43,  90, 43, 18, C_GREEN);
  display.fillRect(86,  90, 43, 18, C_BLUE);
  display.fillRect(129, 90, 43, 18, C_WHITE);
}

// ---------- SD ----------
static bool initSdSharedSpi(uint32_t &okFreq) {
  const uint32_t freqs[] = {400000, 1000000, 4000000, 8000000};

  for (size_t i = 0; i < sizeof(freqs) / sizeof(freqs[0]); ++i) {
    acquireForSd();
    SPI.begin();
    delay(5);

    logf("[SD] try init @ %lu Hz ... ", (unsigned long)freqs[i]);

    SdSpiConfig cfg(SD_CS_PIN, SHARED_SPI, freqs[i], &SPI);
    if (SD.begin(cfg)) {
      Serial.println("OK");
      okFreq = freqs[i];
      return true;
    }

    Serial.println("FAIL");
    if (SD.card()) {
      logf("  code=0x%02X data=0x%02X\n",
           SD.card()->errorCode(),
           SD.card()->errorData());
    }
    delay(120);
  }

  return false;
}

static String nextRecordFileName() {
  char name[24];
  for (int i = 1; i < 1000; ++i) {
    snprintf(name, sizeof(name), "/REC_%03d.WAV", i);
    acquireForSd();
    if (!SD.exists(name)) return String(name);
  }
  return String("/REC_999.WAV");
}

// ---------- audio ring ----------
static inline uint32_t ringCountNoLock(uint32_t w, uint32_t r) {
  return (w >= r) ? (w - r) : (RING_SAMPLES - (r - w));
}

static inline uint32_t ringFreeNoLock(uint32_t w, uint32_t r) {
  return (RING_SAMPLES - 1) - ringCountNoLock(w, r);
}

static void resetRing() {
  noInterrupts();
  g_ringWrite = 0;
  g_ringRead = 0;
  g_droppedSamples = 0;
  g_overflow = false;
  interrupts();
}

static void pushSamplesToRing(const int16_t *src, size_t count) {
  noInterrupts();
  uint32_t w = g_ringWrite;
  uint32_t r = g_ringRead;
  for (size_t i = 0; i < count; ++i) {
    if (ringFreeNoLock(w, r) == 0) {
      g_overflow = true;
      ++g_droppedSamples;
      continue;
    }
    g_ring[w] = src[i];
    w = (w + 1) % RING_SAMPLES;
  }
  g_ringWrite = w;
  interrupts();
}

static size_t popSamplesFromRing(int16_t *dst, size_t maxCount) {
  size_t n = 0;
  noInterrupts();
  while (n < maxCount && g_ringRead != g_ringWrite) {
    dst[n++] = g_ring[g_ringRead];
    g_ringRead = (g_ringRead + 1) % RING_SAMPLES;
  }
  interrupts();
  return n;
}

static void onPDMdata() {
#if defined(ARDUINO_ARCH_NRF52)
  int bytesAvailable = PDM.available();
  if (bytesAvailable <= 0) return;
  if (bytesAvailable > (int)sizeof(g_pdmChunk)) bytesAvailable = sizeof(g_pdmChunk);
  int bytesRead = PDM.read(reinterpret_cast<void *>(g_pdmChunk), bytesAvailable);
  if (bytesRead > 0) pushSamplesToRing(g_pdmChunk, static_cast<size_t>(bytesRead / 2));
#endif
}

static bool startPDM() {
#if defined(ARDUINO_ARCH_NRF52)
  PDM.setPins(D1, D0, -1);
  resetRing();
  PDM.onReceive(onPDMdata);
  PDM.setBufferSize(sizeof(g_pdmChunk));
  PDM.setGain(PDM_GAIN);
  if (!PDM.begin(CHANNELS, SAMPLE_RATE_HZ)) return false;
  delay(50);
  return true;
#else
  // PDM mic library is nRF52840-only; recording cannot run on this MCU.
  Serial.println("[MIC] PDM mic unavailable on this MCU (nRF52840-only)");
  return false;
#endif
}

static void stopPDM() {
#if defined(ARDUINO_ARCH_NRF52)
  PDM.end();
#endif
}

static int32_t calcPeak(const int16_t *samples, size_t count) {
  int32_t peak = 0;
  for (size_t i = 0; i < count; ++i) {
    int32_t a = abs((int32_t)samples[i]);
    if (a > peak) peak = a;
  }
  return peak;
}

// ---------- record ----------
static bool recordWavToSd(String &savedName, uint32_t &pcmBytesOut, int32_t &peakOut) {
  savedName = nextRecordFileName();

  acquireForSd();
  if (!recFile.open(savedName.c_str(), O_RDWR | O_CREAT | O_TRUNC)) {
    logf("[SD] open failed: %s\n", savedName.c_str());
    return false;
  }

  acquireForSd();
  if (!writeEmptyWavHeader(recFile)) {
    Serial.println("[SD] write header failed");
    recFile.close();
    return false;
  }

  if (!startPDM()) {
    Serial.println("[MIC] PDM.begin failed");
    recFile.close();
    acquireForSd();
    SD.remove(savedName.c_str());
    return false;
  }

  lcdTitle("Recording...", savedName);
  lcdColorBars();
  lcdLine(120, C_WHITE, "conservative v1");
  lcdLine(138, C_CYAN, "no live meter");
  lcdLine(156, C_WHITE, "recording 5 sec");

  logf("[REC] start -> %s\n", savedName.c_str());

  const uint32_t startMs = millis();
  uint32_t lastLogMs = 0;
  uint32_t pcmBytes = 0;
  int32_t sessionPeak = 0;

  while (millis() - startMs < RECORD_SECONDS * 1000UL) {
    size_t n = popSamplesFromRing(g_writeChunk, PDM_CHUNK_SAMPLES);
    if (n == 0) {
      delay(1);
      continue;
    }

    int32_t peak = calcPeak(g_writeChunk, n);
    if (peak > sessionPeak) sessionPeak = peak;

    size_t bytes = n * sizeof(int16_t);

    acquireForSd();
    if (recFile.write(reinterpret_cast<const uint8_t *>(g_writeChunk), bytes) != bytes) {
      Serial.println("[SD] write failed during record");
      stopPDM();
      recFile.close();
      return false;
    }

    pcmBytes += bytes;

    uint32_t now = millis();
    if (now - lastLogMs > 500) {
      lastLogMs = now;
      logf("[MIC] t=%lu ms peak=%ld dropped=%lu bytes=%lu\n",
           (unsigned long)(now - startMs),
           (long)peak,
           (unsigned long)g_droppedSamples,
           (unsigned long)pcmBytes);
    }
  }

  stopPDM();

  uint32_t drainStart = millis();
  while (millis() - drainStart < 300) {
    size_t n = popSamplesFromRing(g_writeChunk, PDM_CHUNK_SAMPLES);
    if (n == 0) break;

    size_t bytes = n * sizeof(int16_t);
    acquireForSd();
    if (recFile.write(reinterpret_cast<const uint8_t *>(g_writeChunk), bytes) != bytes) {
      Serial.println("[SD] write failed while draining");
      recFile.close();
      return false;
    }

    pcmBytes += bytes;
  }

  acquireForSd();
  if (!patchWavHeader(recFile, pcmBytes)) {
    Serial.println("[SD] patch WAV header failed");
    recFile.close();
    return false;
  }

  acquireForSd();
  recFile.flush();
  recFile.close();

  pcmBytesOut = pcmBytes;
  peakOut = sessionPeak;

  logf("[REC] done. bytes=%lu peak=%ld dropped=%lu\n",
       (unsigned long)pcmBytesOut,
       (long)peakOut,
       (unsigned long)g_droppedSamples);

  return true;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("=== XIAO nRF52840 Plus 1.47 LCD + SD + audio ===");
  Serial.println("LCD baseline = Seeed_GFX JD9853A (shared HW SPI)");
  Serial.println("SD backend   = board-package SdFat, SHARED_SPI");
  Serial.println("Audio        = PDM D0/D1, conservative v1");
  Serial.println("No IMU / No Touch");

  if (!initLcd()) {
    Serial.println("[FAIL] LCD init failed");
    return;
  }

  lcdTitle("LCD OK", "starting SD...");
  lcdColorBars();
  delay(600);

  uint32_t okFreq = 0;
  if (!initSdSharedSpi(okFreq)) {
    Serial.println("[FAIL] SD mount failed");
    lcdTitle("SD FAIL", "mount failed");
    lcdLine(74, C_RED, "shared SPI mount fail");
    return;
  }

  Serial.print("[OK] SD mounted @ ");
  Serial.print(okFreq);
  Serial.println(" Hz");

  lcdTitle("SD OK", String("freq=") + okFreq);
  lcdColorBars();
  lcdLine(120, C_WHITE, "next: record WAV");
  delay(600);

  String savedName;
  uint32_t pcmBytes = 0;
  int32_t peak = 0;

  if (!recordWavToSd(savedName, pcmBytes, peak)) {
    Serial.println("[FAIL] recording failed");
    lcdTitle("REC FAIL", "recording failed");
    lcdLine(120, C_RED, "check serial log");
    return;
  }

  Serial.println("[OK] recording success");

  lcdTitle("REC PASS", savedName);
  lcdColorBars();
  lcdLine(120, C_WHITE, String("bytes=") + pcmBytes);
  lcdLine(138, C_WHITE, String("peak=") + peak);
  lcdLine(156, C_WHITE, String("drop=") + (unsigned long)g_droppedSamples);
  lcdLine(174, C_CYAN, "Reset to record again");

  Serial.println("[DONE] LCD + SD + audio finished");
}

void loop() {
  delay(1000);
}
