// Sketch to draw an analogue clock on the screen
// This uses anti-aliased drawing functions that are built into TFT_eSPI

// Anti-aliased lines can be drawn with sub-pixel resolution and permit lines to be
// drawn with less jaggedness.

// Based on a sketch by DavyLandman:
// https://github.com/Bodmer/TFT_eSPI/issues/905


// Set to 1 to enable the original WiFi/NTP extension. That mode additionally
// needs the Time, Timezone and platform WiFi libraries documented in NTP_Time.h.
#ifndef ENABLE_NTP_TIME
#define ENABLE_NTP_TIME 0
#endif

#if ENABLE_NTP_TIME
#define WIFI_SSID      "Your_SSID"
#define WIFI_PASSWORD  "Your_Password"
#endif

#include <Arduino.h>
#include <Seeed_GFX.h>

// -- Board & Driver: edit for your hardware --
#include "board/boards/XIAO_LCD_Board.h"
#include "driver/tft/Driver_ST7789.h"
#include "panel/Panel_TFT.h"
#include <SPI.h>

#include "NotoSansBold15.h"

Seeed_GFX display;  // Invoke library, pins defined in User_Setup.h
Seeed_Sprite face;

static void renderFace(float t);
void getCoord(int16_t x, int16_t y, float *xp, float *yp,
              int16_t r, float a);

#define CLOCK_X_POS 10
#define CLOCK_Y_POS 10

#define CLOCK_FG   TFT_SKYBLUE
#define CLOCK_BG   TFT_NAVY
#define SECCOND_FG TFT_RED
#define LABEL_FG   TFT_GOLD

#define CLOCK_R       127.0f / 2.0f // Clock face radius (float type)
#define H_HAND_LENGTH CLOCK_R/2.0f
#define M_HAND_LENGTH CLOCK_R/1.4f
#define S_HAND_LENGTH CLOCK_R/1.3f

#define FACE_W CLOCK_R * 2 + 1
#define FACE_H CLOCK_R * 2 + 1

// Calculate 1 second increment angles. Hours and minute hand angles
// change every second so we see smooth sub-pixel movement
#define SECOND_ANGLE 360.0 / 60.0
#define MINUTE_ANGLE SECOND_ANGLE / 60.0
#define HOUR_ANGLE   MINUTE_ANGLE / 12.0

// Time h:m:s
uint8_t h = 0, m = 0, s = 0;

float time_secs = h * 3600 + m * 60 + s;

// Load the optional NTP extension after time_secs so it is in scope.
#if ENABLE_NTP_TIME
#include "NTP_Time.h" // Attached to this sketch, see that tab for library needs
#endif

// Time for next tick
uint32_t targetTime = 0;

// Setup
void setup() {
  Serial.begin(115200);
  Serial.println("Booting...");

  // Initialise the screen
  // -- Edit for your board/config --
    if (!display.begin<Board_Seeed_1inch69_LCD, Config_Seeed_1inch69_LCD_ST7789>()) {
        Serial.println(display.lastResult().message);
        while (true) delay(1000);
    }

  // Ideally set orientation for good viewing angle range because
  // the anti-aliasing effectiveness varies with screen viewing angle
  // Usually this is when screen ribbon connector is at the bottom
  display.setRotation(0);
  display.fillScreen(TFT_BLACK);

  // Create the clock face sprite
  //face.setColorDepth(8); // 8-bit will work, but reduces effectiveness of anti-aliasing
  if (!face.createSprite(&display, FACE_W, FACE_H)) {
    Serial.println("Clock sprite allocation failed");
    while (true) delay(1000);
  }

  // Only 1 font used in the sprite, so can remain loaded
  face.loadFont(NotoSansBold15);

  // Draw the whole clock - NTP time not available yet
  renderFace(time_secs);

  targetTime = millis() + 100;
}

// Loop
void loop() {
  // Update time periodically
  if (static_cast<int32_t>(millis() - targetTime) >= 0) {

    // Update next tick time in 100 milliseconds for smooth movement
    targetTime = millis() + 100;

    // Increment time by 100 milliseconds
    time_secs += 0.100;

    // Midnight roll-over
    if (time_secs >= (60 * 60 * 24)) time_secs = 0;

    // All graphics are drawn in sprite to stop flicker
    renderFace(time_secs);

    // Request time from NTP server and synchronise the local clock
    // (clock may pause since this may take >100ms)
#if ENABLE_NTP_TIME
    syncTime();
#endif
  }
}

// Draw the clock face in the sprite
static void renderFace(float t) {
  float h_angle = t * HOUR_ANGLE;
  float m_angle = t * MINUTE_ANGLE;
  float s_angle = t * SECOND_ANGLE;

  // The face is completely redrawn - this can be done quickly
  face.fillSprite(TFT_BLACK);

  // Draw the face circle
  face.fillSmoothCircle( CLOCK_R, CLOCK_R, CLOCK_R, CLOCK_BG );

  // Set text datum to middle centre and the colour
  face.setTextDatum(MC_DATUM);

  // The background colour will be read during the character rendering
  face.setTextColor(CLOCK_FG, CLOCK_BG);

  // Text offset adjustment
  constexpr uint32_t dialOffset = CLOCK_R - 10;

  float xp = 0.0, yp = 0.0; // Use float pixel position for smooth AA motion

  // Draw digits around clock perimeter
  for (uint32_t h = 1; h <= 12; h++) {
    getCoord(CLOCK_R, CLOCK_R, &xp, &yp, dialOffset, h * 360.0 / 12);
    face.drawNumber(h, xp, 2 + yp);
  }

  // Add text (could be digital time...)
  face.setTextColor(LABEL_FG, CLOCK_BG);
  face.drawString("Seeed_GFX", CLOCK_R, CLOCK_R * 0.75);

  // Draw minute hand
  getCoord(CLOCK_R, CLOCK_R, &xp, &yp, M_HAND_LENGTH, m_angle);
  face.drawWideLine(CLOCK_R, CLOCK_R, xp, yp, 6.0f, CLOCK_FG);
  face.drawWideLine(CLOCK_R, CLOCK_R, xp, yp, 2.0f, CLOCK_BG);

  // Draw hour hand
  getCoord(CLOCK_R, CLOCK_R, &xp, &yp, H_HAND_LENGTH, h_angle);
  face.drawWideLine(CLOCK_R, CLOCK_R, xp, yp, 6.0f, CLOCK_FG);
  face.drawWideLine(CLOCK_R, CLOCK_R, xp, yp, 2.0f, CLOCK_BG);

  // Draw the central pivot circle
  face.fillSmoothCircle(CLOCK_R, CLOCK_R, 4, CLOCK_FG);

  // Draw cecond hand
  getCoord(CLOCK_R, CLOCK_R, &xp, &yp, S_HAND_LENGTH, s_angle);
  face.drawWedgeLine(CLOCK_R, CLOCK_R, xp, yp, 2.5, 1.0, SECCOND_FG);
  face.pushSprite(5, 5, TFT_TRANSPARENT);
}

// Get coordinates of end of a line, pivot at x,y, length r, angle a
// Coordinates are returned to caller via the xp and yp pointers
#define DEG2RAD 0.0174532925
void getCoord(int16_t x, int16_t y, float *xp, float *yp, int16_t r, float a)
{
  float sx1 = cos( (a - 90) * DEG2RAD);
  float sy1 = sin( (a - 90) * DEG2RAD);
  *xp =  sx1 * r + x;
  *yp =  sy1 * r + y;
}
