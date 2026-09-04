/**
 * Product: XIAO 1.47 inch Touch Display (JD9853A 172x320 + AXS5106L touch)
 * Display: JD9853A 172x320, BGR, capacitive touch
 * Target:  XIAO nRF52840 Plus (RST=38, BL=37). For ESP32-S3 Plus use the sibling
 *          ESP32-S3 Plus folder (RST=13, BL=12).
 * Wiring:  CS=D2, DC=D3, MOSI=D10, SCLK=D8; RST=38, BL=37.
 * Demo:    Simpler SD BMP slideshow -- same decoder as the stress reader but a 2s delay
 *          between images, no stress mode. Center-crops 16/24/32-bit BMPs and supports
 *          BI_RGB plus RGB555/RGB565 BI_BITFIELDS input.
 *
 * Ported from XIAO-Display-Board-main (xiao_nrf52840_147_sd_image_reader, TFT_eSPI). The
 * Seeed_GFX Board/Config templates replace the original bus+panel setup, manual MADCTL/
 * invert/swapbytes, and (for TFT_eSPI) the driver.h User_Setup. SdFat, the custom BMP row
 * decoder, and the acquireForLcd/acquireForSd shared-SPI arbitration are kept;
 * tft.pushImage maps 1:1 to display.pushImage. Touch is unused on this build (LCD-only).
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

static constexpr int8_t  LCD_RST_PIN = 38;  // XIAO nRF52840 Plus
static constexpr int8_t  LCD_BL_PIN  = 37;

// CS lines are toggled directly by the shared-SPI arbitration below; Seeed_GFX owns the
// rest of the LCD bus (DC/MOSI/SCLK/RST/BL) via the Board/Config templates.
static constexpr uint8_t LCD_CS_PIN = D2;
static constexpr uint8_t SD_CS_PIN  = D6;

static constexpr int LCD_W = 172;
static constexpr int LCD_H = 320;
static constexpr int MAX_FILES = 24;
static constexpr int MAX_PATH_LEN = 64;

SdFat sdCard;

char imagePaths[MAX_FILES][MAX_PATH_LEN];
int imageCount = 0;
int imageIndex = 0;
uint32_t mountedFreq = 0;

// Only the visible crop is read, so source images may be wider than the LCD.
static uint8_t  rowBuf[LCD_W * 4];
static uint16_t lineBuf[LCD_W];

// --- Shared-SPI bus arbitration (LCD + SD share one SPI bus) -------------------
// Seeed_GFX Bus_SPI owns LCD_CS (D2) and parks it HIGH between transactions; only
// SD_CS (D6) is idled here to keep the SD card off the shared bus. The original
// TFT_eSPI build also re-pinned LCD_CS; those lines were removed because Bus_SPI
// already manages D2 (rectify #74).
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

static void showMessage(const char *title, const String &line1, const String &line2 = "") {
  acquireForLcd();
  display.fillScreen(TFT_BLACK);
  display.setTextSize(1);
  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.setCursor(4, 4);
  display.print(title);
  display.drawFastHLine(0, 18, LCD_W, TFT_CYAN);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setCursor(4, 34);
  display.print(line1);
  if (line2.length()) {
    display.setCursor(4, 50);
    display.print(line2);
  }
}

static bool beginSd() {
  acquireForSd();
  SPI.begin();
  const uint32_t freqs[] = {8000000, 4000000, 1000000, 400000};
  for (size_t i = 0; i < sizeof(freqs) / sizeof(freqs[0]); ++i) {
    SdSpiConfig cfg(SD_CS_PIN, SHARED_SPI, freqs[i], &SPI);
    if (sdCard.begin(cfg)) {
      mountedFreq = freqs[i];
      return true;
    }
    delay(100);
  }
  return false;
}

static bool endsWithNoCase(const char *name, const char *ext) {
  size_t n = strlen(name);
  size_t e = strlen(ext);
  if (n < e) return false;
  for (size_t i = 0; i < e; ++i) {
    if (tolower(name[n - e + i]) != tolower(ext[i])) return false;
  }
  return true;
}

static bool isBmpFile(const char *name) {
  return endsWithNoCase(name, ".bmp");
}

static void scanImageDirectory(const char* directory, const char* prefix) {
  File32 root;
  if (!root.open(directory)) return;
  File32 entry;
  while (imageCount < MAX_FILES && entry.openNext(&root, O_RDONLY)) {
    if (!entry.isDir()) {
      char name[MAX_PATH_LEN];
      entry.getName(name, sizeof(name));
      if (isBmpFile(name)) {
        snprintf(imagePaths[imageCount], MAX_PATH_LEN, "%s/%s", prefix, name);
        Serial.print("[IMAGE] ");
        Serial.println(imagePaths[imageCount]);
        imageCount++;
      }
    }
    entry.close();
  }
  root.close();
}

static void scanImages() {
  imageCount = 0;
  scanImageDirectory("/", "");
  scanImageDirectory("/img", "/img");
}

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
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

static uint8_t expandMaskedChannel(uint32_t pixel, uint32_t mask) {
  if (!mask) return 0;
  uint8_t shift = 0;
  while ((mask & 1U) == 0U) { mask >>= 1; ++shift; }
  const uint32_t value = (pixel >> shift) & mask;
  return (uint8_t)((value * 255U + mask / 2U) / mask);
}

static bool drawBmp(const char *path) {
  File32 f;
  if (!f.open(path, O_RDONLY)) return false;
  if (readLE16(f) != 0x4D42) {
    f.close();
    return false;
  }

  (void)readLE32(f);
  (void)readLE16(f);
  (void)readLE16(f);
  uint32_t dataOffset = readLE32(f);
  uint32_t headerSize = readLE32(f);
  int32_t srcW = (int32_t)readLE32(f);
  int32_t srcHRaw = (int32_t)readLE32(f);
  uint16_t planes = readLE16(f);
  uint16_t bpp = readLE16(f);
  uint32_t compression = readLE32(f);

  if (headerSize < 40 || planes != 1 ||
      !((compression == 0) ||
        (compression == 3 && (bpp == 16 || bpp == 32))) ||
      srcW <= 0 || srcHRaw == INT32_MIN ||
      !(bpp == 16 || bpp == 24 || bpp == 32)) {
    f.close();
    return false;
  }

  uint32_t redMask = 0x00FF0000U;
  uint32_t greenMask = 0x0000FF00U;
  uint32_t blueMask = 0x000000FFU;
  if (bpp == 16 && compression == 0) {
    // Windows BI_RGB 16-bit pixels are RGB555 by definition.
    redMask = 0x7C00U; greenMask = 0x03E0U; blueMask = 0x001FU;
  } else if (compression == 3) {
    if (!f.seekSet(14U + 40U)) { f.close(); return false; }
    redMask = readLE32(f);
    greenMask = readLE32(f);
    blueMask = readLE32(f);
    if (!redMask || !greenMask || !blueMask) { f.close(); return false; }
  }

  bool topDown = srcHRaw < 0;
  int32_t srcH = topDown ? -srcHRaw : srcHRaw;
  if (srcH <= 0) {
    f.close();
    return false;
  }

  const uint64_t rowSize64 = (((uint64_t)srcW * bpp + 31ULL) / 32ULL) * 4ULL;
  if (rowSize64 > UINT32_MAX) {
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
  display.fillScreen(TFT_BLACK);

  for (int y = 0; y < drawH; ++y) {
    int srcY = cropY + y;
    int fileY = topDown ? srcY : srcH - 1 - srcY;
    const uint32_t bytesPerPixel = bpp / 8U;
    const uint32_t visibleBytes = (uint32_t)drawW * bytesPerPixel;
    const uint64_t rowOffset64 = (uint64_t)dataOffset +
        (uint64_t)fileY * rowSize + (uint64_t)cropX * bytesPerPixel;
    if (rowOffset64 > UINT32_MAX || visibleBytes > sizeof(rowBuf) ||
        !f.seekSet((uint32_t)rowOffset64) ||
        f.read(rowBuf, visibleBytes) != (int)visibleBytes) {
      f.close();
      return false;
    }

    for (int x = 0; x < drawW; ++x) {
      if (bpp == 24) {
        int p = x * 3;
        lineBuf[x] = rgb888To565(rowBuf[p + 2], rowBuf[p + 1], rowBuf[p]);
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
    display.pushImage(dstX, dstY + y, drawW, 1, (const uint16_t *)lineBuf);
  }

  f.close();
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(800);

  if (!display.begin<Board_XIAO_1inch47_Touch_Display<LCD_RST_PIN, LCD_BL_PIN>, Config_Seeed_1inch47_Touch_JD9853A>()) {
    Serial.println(display.lastResult().message);
    return;
  }

  showMessage("SD image reader", "Mounting SD...");

  if (!beginSd()) {
    showMessage("SD image reader", "SD mount failed");
    while (1) delay(1000);
  }

  scanImages();
  Serial.print("[SD] mounted @ ");
  Serial.println(mountedFreq);
  if (imageCount == 0) showMessage("SD image reader", "No BMP found");
}

void loop() {
  if (imageCount == 0) {
    delay(1000);
    return;
  }

  const char *path = imagePaths[imageIndex];
  if (!drawBmp(path)) showMessage(path, "BMP decode failed");

  delay(2000);
  imageIndex = (imageIndex + 1) % imageCount;
}
