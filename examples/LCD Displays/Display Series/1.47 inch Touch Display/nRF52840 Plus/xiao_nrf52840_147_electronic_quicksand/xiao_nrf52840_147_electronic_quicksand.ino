/**
 * Product: XIAO 1.47 inch Touch Display (JD9853A 172x320 + AXS5106L touch)
 * Display: JD9853A 172x320, BGR, capacitive touch
 * Target:  XIAO nRF52840 Plus (RST=38, BL=37). For ESP32-S3 Plus use the sibling
 *          ESP32-S3 Plus folder (RST=13, BL=12).
 * Wiring:  CS=D2, DC=D3, MOSI=D10, SCLK=D8; RST=38, BL=37.
 * Demo:    nRF52840 version of the quicksand sim; uses Seeed LSM6DS3.h library for the IMU.
 *
 * Ported from XIAO-Display-Board-main (xiao_nrf52840_147_electronic_quicksand, TFT_eSPI). The Seeed_GFX
 * Board/Config templates replace the original bus+panel setup, manual MADCTL/invert/
 * swapbytes, and (for TFT_eSPI) the driver.h User_Setup. DROP driver.h + manual init +
 * applyXIAO147PanelFix + invertDisplay. Keep Adafruit_TinyUSB.h, Wire.h, LSM6DS3.h +
 * imu.begin()/readFloatAccelX/Y. drawString+setTextDatum -> display equivalents 1:1.
 */

#include <Seeed_GFX.h>
#include "board/boards/XIAO_LCD_Board.h"
#include "driver/tft/Driver_JD9853A.h"
#include "panel/Panel_TFT.h"
#include <Adafruit_TinyUSB.h>
#include <Wire.h>
#include "SparkFunLSM6DS3.h"   // was "LSM6DS3.h" (Seeed's lib, not installed) — SparkFun's is API-identical (same class+begin/readFloatAccelX/Y), already installed for the 0.96/1.14/1.47 dashboards
#include <math.h>

Seeed_GFX display;

static constexpr int8_t LCD_RST_PIN = 38;  // XIAO nRF52840 Plus
static constexpr int8_t LCD_BL_PIN  = 37;

static constexpr uint8_t GRID_W = 24;
static constexpr uint8_t GRID_H = 45;
static constexpr uint8_t CELL_SIZE = 7;
static constexpr uint16_t FLUID_PARTICLES = 180;
static constexpr float GRAVITY = 0.34f;
static constexpr float DAMPING = 0.88f;
static constexpr float MAX_VELOCITY = 1.25f;
static constexpr float TOP_MOBILITY = 1.35f;
static constexpr float BOTTOM_MOBILITY = 0.52f;
static constexpr uint8_t FRAME_INTERVAL_MS = 8;

LSM6DS3 imu(I2C_MODE, 0x6A);

struct Particle {
  float x;
  float y;
  float vx;
  float vy;
  int8_t cellX;
  int8_t cellY;
  int8_t oldCellX;
  int8_t oldCellY;
  uint16_t color;
};

Particle particles[FLUID_PARTICLES];
bool occupied[GRID_W][GRID_H];
uint16_t particleOrder[FLUID_PARTICLES];

float accelX = 0.0f;
float accelY = 0.0f;
float gravityX = 0.0f;
float gravityY = 1.0f;
float depthMinScore = 0.0f;
float depthSpan = 1.0f;
int16_t gridOriginX = 0;
int16_t gridOriginY = 0;
uint32_t lastFrameMs = 0;

static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

static uint16_t blendColor(uint16_t a, uint16_t b, uint8_t amount) {
  uint8_t ar = (a >> 11) & 0x1F;
  uint8_t ag = (a >> 5) & 0x3F;
  uint8_t ab = a & 0x1F;
  uint8_t br = (b >> 11) & 0x1F;
  uint8_t bg = (b >> 5) & 0x3F;
  uint8_t bb = b & 0x1F;

  uint8_t rr = ar + ((int16_t)(br - ar) * amount) / 255;
  uint8_t rg = ag + ((int16_t)(bg - ag) * amount) / 255;
  uint8_t rb = ab + ((int16_t)(bb - ab) * amount) / 255;
  return (rr << 11) | (rg << 5) | rb;
}

static bool initDisplay() {
  if (!display.begin<Board_XIAO_1inch47_Touch_Display<LCD_RST_PIN, LCD_BL_PIN>, Config_XIAO_1inch47_Touch_JD9853A>()) {
    Serial.println(display.lastResult().message);
    return false;
  }
  display.fillScreen(TFT_BLACK);

  gridOriginX = (display.width() - GRID_W * CELL_SIZE) / 2;
  gridOriginY = (display.height() - GRID_H * CELL_SIZE) / 2;
  return true;
}

static void drawBackground() {
  display.fillScreen(TFT_BLACK);
}

static void showMessage(const char *title, const char *line) {
  display.fillScreen(TFT_BLACK);
  display.setTextDatum(MC_DATUM);
  display.setTextColor(rgb565(255, 220, 80), TFT_BLACK);
  display.drawString(title, display.width() / 2, 118, 4);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.drawString(line, display.width() / 2, 160, 2);
}

static void drawCell(int8_t x, int8_t y, uint16_t color) {
  int16_t px = gridOriginX + x * CELL_SIZE;
  int16_t py = gridOriginY + y * CELL_SIZE;
  display.fillRect(px + 1, py + 1, CELL_SIZE - 2, CELL_SIZE - 2, color);
}

static void clearCell(int8_t x, int8_t y) {
  if (x < 0 || x >= GRID_W || y < 0 || y >= GRID_H) return;
  drawCell(x, y, TFT_BLACK);
}

static float randUnit() {
  return random(-1000, 1001) * 0.001f;
}

static void clearOccupancy() {
  memset(occupied, 0, sizeof(occupied));
}

static void resetParticles() {
  const uint16_t deep = rgb565(196, 128, 18);
  const uint16_t bright = rgb565(255, 236, 92);
  uint16_t index = 0;

  clearOccupancy();
  for (int8_t y = GRID_H - 8; y < GRID_H && index < FLUID_PARTICLES; ++y) {
    for (int8_t x = 0; x < GRID_W && index < FLUID_PARTICLES; ++x) {
      Particle &p = particles[index++];
      p.x = x;
      p.y = y;
      p.vx = randUnit() * 0.2f;
      p.vy = randUnit() * 0.2f;
      p.cellX = x;
      p.cellY = y;
      p.oldCellX = -1;
      p.oldCellY = -1;
      p.color = blendColor(deep, bright, random(50, 230));
      occupied[x][y] = true;
    }
  }

  for (uint16_t i = 0; i < FLUID_PARTICLES; ++i) {
    particleOrder[i] = i;
  }
}

static void readGravity() {
  float x = imu.readFloatAccelX();
  float y = imu.readFloatAccelY();

  accelX = accelX * 0.78f + x * 0.22f;
  accelY = accelY * 0.78f + y * 0.22f;

  float len = sqrtf(accelX * accelX + accelY * accelY);
  if (len < 0.06f) return;

  gravityX = gravityX * 0.72f + (accelX / len) * 0.28f;
  gravityY = gravityY * 0.72f + (accelY / len) * 0.28f;
}

static void updateDepthRange() {
  float minScore = 100000.0f;
  float maxScore = -100000.0f;
  const float corners[4][2] = {
    {0.0f, 0.0f},
    {(float)(GRID_W - 1), 0.0f},
    {0.0f, (float)(GRID_H - 1)},
    {(float)(GRID_W - 1), (float)(GRID_H - 1)}
  };

  for (uint8_t i = 0; i < 4; ++i) {
    float score = corners[i][0] * gravityX + corners[i][1] * gravityY;
    if (score < minScore) minScore = score;
    if (score > maxScore) maxScore = score;
  }

  depthMinScore = minScore;
  depthSpan = maxScore - minScore;
  if (depthSpan < 0.001f) depthSpan = 1.0f;
}

static float particleDepth(const Particle &p) {
  float current = p.cellX * gravityX + p.cellY * gravityY;
  return constrain((current - depthMinScore) / depthSpan, 0.0f, 1.0f);
}

static float particleMobility(const Particle &p) {
  float depth = particleDepth(p);
  return TOP_MOBILITY + (BOTTOM_MOBILITY - TOP_MOBILITY) * depth;
}

static float clampVelocity(float v, float limit) {
  if (v > limit) return limit;
  if (v < -limit) return -limit;
  return v;
}

static void constrainToScreen(Particle &p) {
  if (p.x < 0.0f) {
    p.x = 0.0f;
    if (p.vx < 0.0f) p.vx *= -0.25f;
  } else if (p.x > GRID_W - 1) {
    p.x = GRID_W - 1;
    if (p.vx > 0.0f) p.vx *= -0.25f;
  }

  if (p.y < 0.0f) {
    p.y = 0.0f;
    if (p.vy < 0.0f) p.vy *= -0.25f;
  } else if (p.y > GRID_H - 1) {
    p.y = GRID_H - 1;
    if (p.vy > 0.0f) p.vy *= -0.25f;
  }
}

static bool pickCell(
  Particle &p,
  int8_t wantX,
  int8_t wantY,
  int8_t &outX,
  int8_t &outY,
  float mobility
) {
  float bestScore = 100000.0f;
  bool found = false;

  for (int8_t radius = 0; radius <= 1; ++radius) {
    for (int8_t dy = -radius; dy <= radius; ++dy) {
      for (int8_t dx = -radius; dx <= radius; ++dx) {
        if (abs(dx) + abs(dy) != radius) continue;

        int8_t cx = wantX + dx;
        int8_t cy = wantY + dy;
        if (cx < 0 || cx >= GRID_W || cy < 0 || cy >= GRID_H) continue;
        if (occupied[cx][cy]) continue;

        float px = (float)cx - p.x;
        float py = (float)cy - p.y;
        float flowBonus = px * gravityX + py * gravityY;
        float score = px * px + py * py - flowBonus * (0.40f + mobility * 0.28f);
        if (score < bestScore) {
          bestScore = score;
          outX = cx;
          outY = cy;
          found = true;
        }
      }
    }
    if (found) return true;
  }

  return false;
}

static void updateParticles() {
  clearOccupancy();

  for (uint16_t i = 1; i < FLUID_PARTICLES; ++i) {
    uint16_t key = particleOrder[i];
    float keyScore = particles[key].cellX * gravityX + particles[key].cellY * gravityY;
    int16_t j = i - 1;
    while (j >= 0) {
      Particle &p = particles[particleOrder[j]];
      float score = p.cellX * gravityX + p.cellY * gravityY;
      if (score >= keyScore) break;
      particleOrder[j + 1] = particleOrder[j];
      --j;
    }
    particleOrder[j + 1] = key;
  }

  for (uint16_t i = 0; i < FLUID_PARTICLES; ++i) {
    Particle &p = particles[particleOrder[i]];
    float mobility = particleMobility(p);
    float localDamping = 0.74f + mobility * 0.15f;
    float localGravity = GRAVITY * mobility;
    float localMaxVelocity = MAX_VELOCITY * (0.52f + mobility * 0.42f);

    p.vx = clampVelocity((p.vx + gravityX * localGravity) * localDamping,
                         localMaxVelocity);
    p.vy = clampVelocity((p.vy + gravityY * localGravity) * localDamping,
                         localMaxVelocity);
    p.x += p.vx;
    p.y += p.vy;
    constrainToScreen(p);

    int8_t wantX = constrain((int)roundf(p.x), 0, GRID_W - 1);
    int8_t wantY = constrain((int)roundf(p.y), 0, GRID_H - 1);
    int8_t cellX = p.cellX;
    int8_t cellY = p.cellY;

    if (!pickCell(p, wantX, wantY, cellX, cellY, mobility)) {
      cellX = p.cellX;
      cellY = p.cellY;
      p.vx *= -0.18f;
      p.vy *= -0.18f;
    }

    p.x = p.x * 0.35f + cellX * 0.65f;
    p.y = p.y * 0.35f + cellY * 0.65f;
    p.oldCellX = p.cellX;
    p.oldCellY = p.cellY;
    p.cellX = cellX;
    p.cellY = cellY;
    occupied[cellX][cellY] = true;
  }
}

static void drawParticles() {
  for (uint16_t i = 0; i < FLUID_PARTICLES; ++i) {
    Particle &p = particles[i];
    if (p.oldCellX != p.cellX || p.oldCellY != p.cellY) {
      clearCell(p.oldCellX, p.oldCellY);
    }
  }

  for (uint16_t i = 0; i < FLUID_PARTICLES; ++i) {
    Particle &p = particles[i];
    if (p.oldCellX != p.cellX || p.oldCellY != p.cellY) {
      drawCell(p.cellX, p.cellY, p.color);
    }
  }

}

void setup() {
  Serial.begin(115200);
  delay(500);

  if (!initDisplay()) {
    while (1) delay(1000);
  }
  showMessage("Fluid", "Starting IMU...");

  Wire.begin();
  int ret = imu.begin();
  Serial.println();
  Serial.println("=== Electronic Quicksand ===");
  Serial.print("imu.begin=");
  Serial.println(ret);

  if (ret != 0) {
    showMessage("IMU Error", "Check LSM6DS3");
    while (1) delay(1000);
  }

  randomSeed((uint32_t)micros());
  drawBackground();
  resetParticles();
  for (uint16_t i = 0; i < FLUID_PARTICLES; ++i) {
    drawCell(particles[i].cellX, particles[i].cellY, particles[i].color);
  }
  lastFrameMs = millis();
}

void loop() {
  uint32_t now = millis();
  if (now - lastFrameMs < FRAME_INTERVAL_MS) return;
  lastFrameMs = now;

  readGravity();
  updateDepthRange();
  updateParticles();
  drawParticles();
}
