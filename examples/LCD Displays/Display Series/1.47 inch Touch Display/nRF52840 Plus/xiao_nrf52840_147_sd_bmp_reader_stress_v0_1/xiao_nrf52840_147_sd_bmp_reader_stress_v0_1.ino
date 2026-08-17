/**
 * Product: XIAO 1.47 inch Touch Display (JD9853A 172x320 + AXS5106L touch)
 * Display: JD9853A 172x320, BGR, capacitive touch
 * Target:  XIAO nRF52840 Plus (RST=38, BL=37). For ESP32-S3 Plus use the sibling
 *          ESP32-S3 Plus folder (RST=13, BL=12).
 * Wiring:  CS=D2, DC=D3, MOSI=D10, SCLK=D8; RST=38, BL=37. SD CS=D6, MISO=D9 (shared SPI bus).
 * Demo:    SD BMP viewer + SD/LCD coexistence stress test. Scans root for *.bmp,
 *          center-crops to 172x320, draws row-by-row; stress mode re-reads every frame.
 *          Custom BMP decoder (16/24/32-bit, BI_RGB and BI_BITFIELDS).
 *
 * Ported from XIAO-Display-Board-main (xiao_nrf52840_147_sd_bmp_reader_stress_v0_1,
 * TFT_eSPI). The Seeed_GFX Board/Config templates replace the original bus+panel
 * setup, manual MADCTL/invert/swapbytes, and (for TFT_eSPI) the driver.h User_Setup.
 * Keeps SdFat, the custom BMP row decoder, and acquireForLcd/acquireForSd shared-SPI
 * arbitration (LCD CS=D2 / SD CS=D6 — Seeed_GFX does not manage SD CS).
 * tft.pushImage(x,y,w,h,lineBuf) -> display.pushImage(x,y,w,h,(const uint16_t*)lineBuf).
 */

#include <Seeed_GFX.h>
#include "board/boards/XIAO_LCD_Board.h"
#include "driver/tft/Driver_JD9853A.h"
#include "panel/Panel_TFT.h"
#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <SPI.h>
#include <SdFat.h>

Seeed_GFX display;

// ========================= Pin map =========================

// Shared-SPI arbitration pins (Board template owns RST/BL + the LCD bus).
static constexpr uint8_t LCD_CS_PIN    = D2;
static constexpr uint8_t SD_CS_PIN     = D6;

static constexpr int8_t LCD_RST_PIN = 38;  // XIAO nRF52840 Plus
static constexpr int8_t LCD_BL_PIN  = 37;

// ========================= Display =========================

static constexpr int LCD_W = 172;
static constexpr int LCD_H = 320;

// ========================= SD =========================

SdFat sdCard;

static constexpr uint32_t SD_FREQ_INIT  = 400000;
static constexpr uint32_t SD_FREQ_RUN_1 = 4000000;
static constexpr uint32_t SD_FREQ_RUN_2 = 8000000;

static uint32_t g_sdFreq = 0;

// ========================= BMP config =========================

static constexpr int MAX_IMAGES = 16;
static constexpr int MAX_PATH_LEN = 64;

// Only the visible crop is read, so source images may be wider than the LCD.
static uint8_t rowBuf[LCD_W * 4];
static uint16_t lineBuf[LCD_W];

char g_imagePaths[MAX_IMAGES][MAX_PATH_LEN];
int g_imageCount = 0;
int g_imageIndex = 0;

uint32_t g_frame = 0;
uint32_t g_lastFrameMs = 0;
uint32_t g_lastRenderMs = 0;
uint32_t g_failCount = 0;

// Continuous full image read mode.
// true  = stress test: keep re-opening + reading BMP + drawing LCD.
// false = slideshow: delay between frames.
static constexpr bool STRESS_READ_EVERY_FRAME = true;
static constexpr uint32_t SLIDESHOW_DELAY_MS = 1200;

// ========================= Colors =========================

static constexpr uint16_t C_BLACK  = TFT_BLACK;
static constexpr uint16_t C_WHITE  = TFT_WHITE;
static constexpr uint16_t C_GREEN  = TFT_GREEN;
static constexpr uint16_t C_CYAN   = TFT_CYAN;
static constexpr uint16_t C_YELLOW = TFT_YELLOW;
static constexpr uint16_t C_RED    = TFT_RED;
static constexpr uint16_t C_GRAY   = 0x8410;
static constexpr uint16_t C_BLUE   = TFT_BLUE;

// ========================= Bus helpers =========================
// SD and LCD share the SPI bus. Seeed_GFX Bus_SPI owns LCD_CS (D2) and parks it
// HIGH between transactions; only SD_CS (D6) is idled here to keep the SD card off
// the shared bus before the other device uses it.

static void acquireForLcd() {
  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH);
  delayMicroseconds(2);
}

static void acquireForSd() {
  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH);
  delayMicroseconds(2);
}

// ========================= Basic UI =========================

static void printFixed(int x, int y, uint16_t color, const String &s, int chars) {
  String out = s;
  while ((int)out.length() < chars) out += ' ';
  if ((int)out.length() > chars) out = out.substring(0, chars);

  acquireForLcd();
  display.setTextSize(1);
  display.setTextColor(color, C_BLACK);
  display.setCursor(x, y);
  display.print(out);
}

static void drawNoImageScreen(const char *msg) {
  acquireForLcd();
  display.fillScreen(C_BLACK);

  display.setTextSize(2);
  display.setTextColor(C_GREEN, C_BLACK);
  display.setCursor(8, 16);
  display.print("Hello,XIAO!");

  display.setTextSize(1);
  display.setTextColor(C_CYAN, C_BLACK);
  display.setCursor(10, 46);
  display.print("1.47 SD BMP Reader");

  display.drawFastHLine(8, 64, 156, C_GRAY);

  display.setTextColor(C_YELLOW, C_BLACK);
  display.setCursor(10, 88);
  display.print("Put BMP files in SD root");

  display.setTextColor(C_WHITE, C_BLACK);
  display.setCursor(10, 112);
  display.print("/test.bmp");
  display.setCursor(10, 128);
  display.print("/image001.bmp");
  display.setCursor(10, 144);
  display.print("/image002.bmp");

  display.setTextColor(C_CYAN, C_BLACK);
  display.setCursor(10, 176);
  display.print("Format:");
  display.setCursor(10, 192);
  display.print("24-bit uncompressed BMP");
  display.setCursor(10, 208);
  display.print("172x320 recommended");

  display.setTextColor(C_RED, C_BLACK);
  display.setCursor(10, 250);
  display.print(msg);
}

static void drawStatusBar(const char *path, bool ok, uint32_t renderMs) {
  acquireForLcd();

  display.fillRect(0, 0, LCD_W, 18, C_BLACK);
  display.drawFastHLine(0, 18, LCD_W, ok ? C_GREEN : C_RED);

  display.setTextSize(1);
  display.setTextColor(ok ? C_GREEN : C_RED, C_BLACK);
  display.setCursor(2, 3);
  display.print(ok ? "SD IMG" : "IMG ERR");

  display.setTextColor(C_CYAN, C_BLACK);
  display.setCursor(50, 3);
  display.print("#");
  display.print((unsigned long)g_frame);

  display.setTextColor(C_YELLOW, C_BLACK);
  display.setCursor(100, 3);
  display.print(renderMs);
  display.print("ms");

  display.fillRect(0, LCD_H - 14, LCD_W, 14, C_BLACK);
  display.setTextColor(C_WHITE, C_BLACK);
  display.setCursor(2, LCD_H - 11);

  String name = String(path);
  int slash = name.lastIndexOf('/');
  if (slash >= 0) name = name.substring(slash + 1);
  if (name.length() > 20) name = name.substring(0, 20);
  display.print(name);
}

// ========================= SD init/list =========================

static bool beginSd() {
  acquireForSd();
  SPI.begin();

  uint32_t freqs[] = {SD_FREQ_RUN_2, SD_FREQ_RUN_1, SD_FREQ_INIT};

  for (size_t i = 0; i < sizeof(freqs) / sizeof(freqs[0]); i++) {
    SdSpiConfig cfg(SD_CS_PIN, SHARED_SPI, freqs[i], &SPI);
    if (sdCard.begin(cfg)) {
      g_sdFreq = freqs[i];
      Serial.print("[SD] begin OK freq=");
      Serial.println(g_sdFreq);
      return true;
    }

    Serial.print("[SD] begin failed freq=");
    Serial.println(freqs[i]);
    delay(50);
  }

  return false;
}

static bool endsWithBmp(const char *name) {
  size_t n = strlen(name);
  if (n < 4) return false;

  char a = tolower(name[n - 4]);
  char b = tolower(name[n - 3]);
  char c = tolower(name[n - 2]);
  char d = tolower(name[n - 1]);

  return a == '.' && b == 'b' && c == 'm' && d == 'p';
}

static void addImagePath(const char *name) {
  if (g_imageCount >= MAX_IMAGES) return;
  if (!endsWithBmp(name)) return;

  char path[MAX_PATH_LEN];
  if (name[0] == '/') {
    snprintf(path, sizeof(path), "%s", name);
  } else {
    snprintf(path, sizeof(path), "/%s", name);
  }

  strncpy(g_imagePaths[g_imageCount], path, MAX_PATH_LEN - 1);
  g_imagePaths[g_imageCount][MAX_PATH_LEN - 1] = 0;

  Serial.print("[IMG] found ");
  Serial.println(g_imagePaths[g_imageCount]);

  g_imageCount++;
}

static void scanBmpDirectory(const char* directory, const char* prefix) {
  File32 root;
  if (!root.open(directory)) return;

  File32 entry;
  while (g_imageCount < MAX_IMAGES && entry.openNext(&root, O_RDONLY)) {
    if (!entry.isDir()) {
      char name[MAX_PATH_LEN];
      entry.getName(name, sizeof(name));
      char path[MAX_PATH_LEN];
      snprintf(path, sizeof(path), "%s/%s", prefix, name);
      addImagePath(path);
    }
    entry.close();
  }

  root.close();
}

static void scanBmpFiles() {
  g_imageCount = 0;
  scanBmpDirectory("/", "");
  scanBmpDirectory("/img", "/img");

  // Fallback candidates, useful if LFN listing behaves differently.
  if (g_imageCount == 0) {
    const char *candidates[] = {
      "/test.bmp",
      "/image.bmp",
      "/image001.bmp",
      "/image002.bmp",
      "/1.bmp",
      "/2.bmp"
    };

    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
      File32 f;
      if (f.open(candidates[i], O_RDONLY)) {
        f.close();
        addImagePath(candidates[i]);
      }
    }
  }

  Serial.print("[IMG] count=");
  Serial.println(g_imageCount);
}

// ========================= BMP reader =========================

static uint16_t readLE16(File32 &f) {
  uint8_t b[2];
  if (f.read(b, 2) != 2) return 0;
  return (uint16_t)b[0] | ((uint16_t)b[1] << 8);
}

static uint32_t readLE32(File32 &f) {
  uint8_t b[4];
  if (f.read(b, 4) != 4) return 0;
  return (uint32_t)b[0] |
         ((uint32_t)b[1] << 8) |
         ((uint32_t)b[2] << 16) |
         ((uint32_t)b[3] << 24);
}

static uint16_t rgb888To565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) |
         ((g & 0xFC) << 3) |
         (b >> 3);
}

static uint8_t expandMaskedChannel(uint32_t pixel, uint32_t mask) {
  if (!mask) return 0;
  uint8_t shift = 0;
  while ((mask & 1U) == 0U) { mask >>= 1; ++shift; }
  const uint32_t value = (pixel >> shift) & mask;
  return (uint8_t)((value * 255U + mask / 2U) / mask);
}

static bool drawBmpFromSd(const char *path) {
  File32 f;
  if (!f.open(path, O_RDONLY)) {
    Serial.print("[BMP] open failed ");
    Serial.println(path);
    return false;
  }

  uint32_t t0 = millis();

  uint16_t signature = readLE16(f);
  if (signature != 0x4D42) {
    Serial.println("[BMP] not BM signature");
    f.close();
    return false;
  }

  (void)readLE32(f); // file size
  (void)readLE16(f); // reserved 1
  (void)readLE16(f); // reserved 2
  uint32_t dataOffset = readLE32(f);

  uint32_t headerSize = readLE32(f);
  if (headerSize < 40) {
    Serial.println("[BMP] unsupported header");
    f.close();
    return false;
  }

  int32_t srcW = (int32_t)readLE32(f);
  int32_t srcHRaw = (int32_t)readLE32(f);
  uint16_t planes = readLE16(f);
  uint16_t bpp = readLE16(f);
  uint32_t compression = readLE32(f);

  if (planes != 1 ||
      !((compression == 0) ||
        (compression == 3 && (bpp == 16 || bpp == 32)))) {
    Serial.println("[BMP] compression is not BI_RGB/BI_BITFIELDS");
    f.close();
    return false;
  }

  if (!(bpp == 24 || bpp == 32 || bpp == 16)) {
    Serial.print("[BMP] unsupported bpp=");
    Serial.println(bpp);
    f.close();
    return false;
  }

  if (srcHRaw == INT32_MIN) {
    Serial.println("[BMP] bad height");
    f.close();
    return false;
  }

  uint32_t redMask = 0x00FF0000U;
  uint32_t greenMask = 0x0000FF00U;
  uint32_t blueMask = 0x000000FFU;
  if (bpp == 16 && compression == 0) {
    redMask = 0x7C00U; greenMask = 0x03E0U; blueMask = 0x001FU;
  } else if (compression == 3) {
    if (!f.seekSet(14U + 40U)) { f.close(); return false; }
    redMask = readLE32(f);
    greenMask = readLE32(f);
    blueMask = readLE32(f);
    if (!redMask || !greenMask || !blueMask) {
      Serial.println("[BMP] invalid bitfield masks");
      f.close();
      return false;
    }
  }

  bool topDown = srcHRaw < 0;
  int32_t srcH = topDown ? -srcHRaw : srcHRaw;

  if (srcW <= 0 || srcH <= 0) {
    Serial.println("[BMP] bad size");
    f.close();
    return false;
  }

  const uint64_t rowSize64 = (((uint64_t)srcW * bpp + 31ULL) / 32ULL) * 4ULL;
  if (rowSize64 > UINT32_MAX) {
    Serial.println("[BMP] row is too large");
    f.close();
    return false;
  }
  const uint32_t rowSize = (uint32_t)rowSize64;

  int drawW = min((int)srcW, LCD_W);
  int drawH = min((int)srcH, LCD_H);

  int cropX = srcW > LCD_W ? (srcW - LCD_W) / 2 : 0;
  int cropY = srcH > LCD_H ? (srcH - LCD_H) / 2 : 0;

  int dstX = srcW < LCD_W ? (LCD_W - srcW) / 2 : 0;
  int dstY = srcH < LCD_H ? (LCD_H - srcH) / 2 : 0;

  acquireForLcd();
  display.fillScreen(C_BLACK);

  for (int y = 0; y < drawH; y++) {
    int srcY = cropY + y;
    int fileY = topDown ? srcY : (srcH - 1 - srcY);
    const uint32_t bytesPerPixel = bpp / 8U;
    const uint32_t visibleBytes = (uint32_t)drawW * bytesPerPixel;
    const uint64_t rowOffset64 = (uint64_t)dataOffset +
        (uint64_t)fileY * rowSize + (uint64_t)cropX * bytesPerPixel;

    if (rowOffset64 > UINT32_MAX || visibleBytes > sizeof(rowBuf) ||
        !f.seekSet((uint32_t)rowOffset64)) {
      Serial.println("[BMP] seek failed");
      f.close();
      return false;
    }

    int n = f.read(rowBuf, visibleBytes);
    if (n != (int)visibleBytes) {
      Serial.println("[BMP] row read failed");
      f.close();
      return false;
    }

    for (int x = 0; x < drawW; x++) {
      if (bpp == 24) {
        int p = x * 3;
        uint8_t b = rowBuf[p + 0];
        uint8_t g = rowBuf[p + 1];
        uint8_t r = rowBuf[p + 2];
        lineBuf[x] = rgb888To565(r, g, b);
      } else {
        const int bytes = bpp / 8;
        const int p = x * bytes;
        uint32_t pixel = (uint32_t)rowBuf[p] | ((uint32_t)rowBuf[p + 1] << 8);
        if (bytes == 4) {
          pixel |= ((uint32_t)rowBuf[p + 2] << 16) |
                   ((uint32_t)rowBuf[p + 3] << 24);
        }
        lineBuf[x] = rgb888To565(expandMaskedChannel(pixel, redMask),
                                 expandMaskedChannel(pixel, greenMask),
                                 expandMaskedChannel(pixel, blueMask));
      }
    }

    acquireForLcd();
    display.pushImage(dstX, dstY + y, drawW, 1, (const uint16_t*)lineBuf);
  }

  f.close();

  g_lastRenderMs = millis() - t0;

  Serial.print("[BMP] draw OK path=");
  Serial.print(path);
  Serial.print(" size=");
  Serial.print(srcW);
  Serial.print("x");
  Serial.print(srcH);
  Serial.print(" bpp=");
  Serial.print(bpp);
  Serial.print(" renderMs=");
  Serial.println(g_lastRenderMs);

  return true;
}

// ========================= Init =========================

static bool initLcd() {
  // Board template drives RST+BL and the LCD bus; Config bakes rotation/MADCTL/
  // invert/byte-order for the 172x320 BGR JD9853A panel.
  if (!display.begin<Board_XIAO_1inch47_Touch_Display<LCD_RST_PIN, LCD_BL_PIN>, Config_XIAO_1inch47_Touch_JD9853A>()) {
    Serial.println(display.lastResult().message);
    return false;
  }

  acquireForLcd();
  display.fillScreen(C_BLACK);

  Serial.println("[LCD] OK");
  return true;
}

// ========================= Arduino =========================

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("=== XIAO nRF52840 Plus 1.47 SD BMP Reader v0.1 ===");
  Serial.println("Seeed_GFX + SdFat shared-SPI.");

  if (!initLcd()) {
    while (1) delay(1000);
  }

  drawNoImageScreen("Mounting SD...");

  if (!beginSd()) {
    Serial.println("[SD] mount failed");
    drawNoImageScreen("SD mount failed");
    while (1) delay(1000);
  }

  scanBmpFiles();

  if (g_imageCount <= 0) {
    drawNoImageScreen("No BMP found");
  }
}

void loop() {
  if (g_imageCount <= 0) {
    // Keep a tiny animation so LCD refresh is still visible.
    static int x = 0;
    static int dir = 1;

    acquireForLcd();
    display.fillRect(10, 286, 152, 8, C_BLACK);
    display.fillRect(10 + x, 286, 20, 8, C_CYAN);

    x += dir * 3;
    if (x <= 0 || x >= 132) dir = -dir;

    delay(40);
    return;
  }

  const char *path = g_imagePaths[g_imageIndex];

  bool ok = drawBmpFromSd(path);
  g_frame++;

  if (!ok) {
    g_failCount++;
  }

  drawStatusBar(path, ok, g_lastRenderMs);

  Serial.print("[STAT] frame=");
  Serial.print((unsigned long)g_frame);
  Serial.print(" image=");
  Serial.print(g_imageIndex + 1);
  Serial.print("/");
  Serial.print(g_imageCount);
  Serial.print(" fail=");
  Serial.print((unsigned long)g_failCount);
  Serial.print(" sdFreq=");
  Serial.println((unsigned long)g_sdFreq);

  g_imageIndex++;
  if (g_imageIndex >= g_imageCount) g_imageIndex = 0;

  if (!STRESS_READ_EVERY_FRAME) {
    delay(SLIDESHOW_DELAY_MS);
  } else {
    delay(5);
  }
}
