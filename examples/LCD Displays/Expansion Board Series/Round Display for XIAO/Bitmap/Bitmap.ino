/**
 * Product: Seeed Studio Round Display for XIAO
 * Display: 1.28 inch, 240x240, GC9A01, capacitive touch (circular bezel)
 * Demo:  Embedded bitmaps via pushImage() (RGB565) and drawBitmap() (1-bit).
 * Wiki:  https://wiki.seeedstudio.com/get_start_round_display/
 *
 * Unlike the 6-color Spectra 6 ePaper, this TFT renders every RGB565 pixel
 * as-is - NO palette quantization. So a smooth color wheel shows full color.
 * To use your own picture, replace the arrays in image.h (see that header for
 * the image2cpp workflow) and adjust width/height below.
 */

#include <Seeed_GFX.h>
#include "image.h"

Seeed_GFX display(Seeed_Product::XIAO_ROUND_DISPLAY);

// Bitmap dimensions (keep in sync with image.h).
static constexpr int16_t IMG_W = 96;
static constexpr int16_t IMG_H = 96;
static constexpr int16_t HEART_W = 48;
static constexpr int16_t HEART_H = 48;

void setup() {
    Serial.begin(115200);
    if (!display.begin()) {
        Serial.println(display.lastResult().message);
        return;
    }

    // Black backdrop; the color wheel's outside-disc pixels are also black,
    // so it blends into a clean color circle within the round bezel.
    display.fillScreen(TFT_BLACK);

    // ---- 1) RGB565 bitmap via pushImage() ----
    // Centered on the 240x240 panel. Every RGB565 color renders directly -
    // no nearest-color mapping (contrast with the Spectra 6 ePaper).
    int16_t imgX = (display.width()  - IMG_W) / 2;   // 72
    int16_t imgY = (display.height() - IMG_H) / 2;   // 72
    display.pushImage(imgX, imgY, IMG_W, IMG_H, (const uint16_t*)gColorWheel_96x96);

    // ---- 2) 1-bit monochrome bitmap via drawBitmap() ----
    // White heart silhouette (fg=white, bg=black blends with backdrop).
    int16_t hx = (display.width() - HEART_W) / 2;    // 96
    int16_t hy = 176;
    display.drawBitmap(hx, hy, gHeart_48x48, HEART_W, HEART_H,
                       TFT_WHITE, TFT_BLACK);

    // ---- Caption ----
    display.setTextColor(TFT_WHITE);
    display.setTextSize(1);
    display.drawCentreString("pushImage + drawBitmap", 120, 20, 1);

    Serial.println("Bitmap displayed. Replace image.h with your own asset.");
}

void loop() {}
