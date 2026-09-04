#include <Seeed_GFX.h>
#include "Free_Fonts.h"
#include <font/GFXFF/FreeSans12pt7b.h>

// Optional emergency override, normally leave undefined:
// #define SEEED_GUI_WIDGET_PRODUCT Seeed_Product::SenseCAP_Watcher
Seeed_GFX display;
Seeed_ButtonWidget leftButton(&display);
Seeed_ButtonWidget toggleButton(&display);
bool toggleOn = false;
bool touchAvailable = false;
bool displayReady = false;

// Button positions (saved during drawButtons for focus indicator)
int16_t btnX, btnY0, btnY1, btnW, btnH;

// Wio Terminal physical button support
#ifdef ARDUINO_WIO_TERMINAL
  #define HAS_PHYSICAL_BUTTONS true
  static constexpr int8_t BTN_A = WIO_KEY_A;       // Controls Momentary button
  static constexpr int8_t BTN_B = WIO_KEY_B;       // Controls Toggle button
  static constexpr int8_t BTN_C = WIO_KEY_C;       // (unused, available for extension)
  static constexpr int8_t BTN_UP = WIO_5S_UP;      // Focus previous
  static constexpr int8_t BTN_DOWN = WIO_5S_DOWN;   // Focus next
  static constexpr int8_t BTN_PRESS = WIO_5S_PRESS; // Press focused button
  int focusedButton = 0;  // 0 = momentary, 1 = toggle
#else
  #define HAS_PHYSICAL_BUTTONS false
#endif

Seeed_Product::Product selectedProduct() {
#ifdef SEEED_GUI_WIDGET_PRODUCT
  return SEEED_GUI_WIDGET_PRODUCT;
#else
  return Seeed_Product::detectIntegratedDisplayProduct();
#endif
}

void drawFocusIndicator() {
#ifdef ARDUINO_WIO_TERMINAL
  // Draw arrow indicator next to focused button
  int16_t arrowX = btnX - 20;
  int16_t y0 = btnY0 + btnH / 2;
  int16_t y1 = btnY1 + btnH / 2;

  // Clear old arrows
  display.fillRect(arrowX - 5, y0 - 8, 15, 16, TFT_BLACK);
  display.fillRect(arrowX - 5, y1 - 8, 15, 16, TFT_BLACK);

  // Draw arrow for focused button
  int16_t fy = (focusedButton == 0) ? y0 : y1;
  display.fillTriangle(arrowX, fy, arrowX + 8, fy - 6, arrowX + 8, fy + 6, TFT_YELLOW);
#endif
}

void drawButtons() {
  btnW = display.width() > 240 ? 180 : 120;
  btnH = 54;
  btnX = (display.width() - btnW) / 2;
  const int16_t centerY = display.height() / 2;
  btnY0 = centerY - btnH - 10;
  btnY1 = centerY + 10;

  leftButton.initButtonUL(btnX, btnY0, btnW, btnH,
                          TFT_WHITE, TFT_RED, TFT_BLACK, "Button", 1);
  toggleButton.initButtonUL(btnX, btnY1, btnW, btnH,
                            TFT_WHITE, TFT_BLACK, TFT_GREEN, "OFF", 1);
  leftButton.draw(false);
  toggleButton.draw(false, 2, TFT_BLACK, "OFF");

#ifdef ARDUINO_WIO_TERMINAL
  // Draw usage hint at bottom
  display.setTextColor(TFT_DARKGREY);
  display.setTextDatum(BC_DATUM);
  display.setTextSize(1);
  display.drawString("A=Momentary  B=Toggle  UP/DN=Focus", display.width() / 2, display.height() - 5, 2);
  drawFocusIndicator();
#endif
}

void momentaryPressAction() {
  leftButton.draw(true);
  Serial.println("Momentary pressed");
}

void momentaryReleaseAction() {
  leftButton.draw(false);
  Serial.println("Momentary released");
}

void togglePressAction() {
  toggleOn = !toggleOn;
  toggleButton.draw(toggleOn, 2, TFT_BLACK, toggleOn ? "ON" : "OFF");
  Serial.println(toggleOn ? "Toggle ON" : "Toggle OFF");
}

void toggleReleaseAction() {
  Serial.println("Toggle released");
}

void setup() {
  Serial.begin(115200);

#ifdef ARDUINO_WIO_TERMINAL
  pinMode(BTN_A, INPUT_PULLUP);
  pinMode(BTN_B, INPUT_PULLUP);
  pinMode(BTN_C, INPUT_PULLUP);
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_PRESS, INPUT_PULLUP);
#endif

  const Seeed_Product::Product product = selectedProduct();
  if (product == Seeed_Product::CUSTOM) {
    Serial.println("Product detection failed; set SEEED_GUI_WIDGET_PRODUCT");
    return;
  }
  if (!display.begin(product)) {
    Serial.print("Display init failed: ");
    Serial.println(display.lastResult().message);
    return;
  }
  display.fillScreen(TFT_BLACK);
  display.setFreeFont(FF18);
  touchAvailable = display.touchPointCapacity() > 0;
  drawButtons();
  leftButton.setPressAction(momentaryPressAction);
  leftButton.setReleaseAction(momentaryReleaseAction);
  toggleButton.setPressAction(togglePressAction);
  toggleButton.setReleaseAction(toggleReleaseAction);
  displayReady = true;

  if (touchAvailable) {
    Serial.println("Touch the buttons");
  } else if (HAS_PHYSICAL_BUTTONS) {
    Serial.println("Use physical buttons: A=Momentary, B=Toggle, UP/DN=Focus");
  } else {
    Serial.println("No touch panel: automatic button demo");
  }
}

void scanButton(Seeed_ButtonWidget& button, bool down) {
  button.press(down);
  if (button.justPressed()) button.runPressAction();
  if (button.justReleased()) button.runReleaseAction();
}

void loop() {
  if (!displayReady) { delay(1000); return; }
  static uint32_t lastScan = 0;
  static bool automaticPress = false;
  if (millis() - lastScan < 30) return;
  lastScan = millis();

  int32_t x = -1, y = -1;
  bool momentaryDown = false;
  bool toggleDown = false;

  if (touchAvailable) {
    // --- Touch mode ---
    bool pressed = display.getTouch(&x, &y);
    momentaryDown = pressed && leftButton.contains(x, y);
    toggleDown = pressed && toggleButton.contains(x, y);

#ifdef ARDUINO_WIO_TERMINAL
  } else if (HAS_PHYSICAL_BUTTONS) {
    // --- Wio Terminal physical buttons ---
    static bool prevUp = true, prevDown = true;
    bool curUp = digitalRead(BTN_UP);
    bool curDown = digitalRead(BTN_DOWN);

    // UP/DOWN to change focus (active-low, pressed = LOW)
    if (prevUp && !curUp) {
      focusedButton = 0;
      drawFocusIndicator();
    }
    if (prevDown && !curDown) {
      focusedButton = 1;
      drawFocusIndicator();
    }
    prevUp = curUp;
    prevDown = curDown;

    // A button → Momentary, B button → Toggle (active-low)
    momentaryDown = !digitalRead(BTN_A);
    toggleDown = !digitalRead(BTN_B);

    // 5-way press → press the focused button
    if (!digitalRead(BTN_PRESS)) {
      if (focusedButton == 0) momentaryDown = true;
      else toggleDown = true;
    }
#endif
  } else {
    // --- Automatic demo mode ---
    const uint8_t phase = static_cast<uint8_t>((millis() / 900U) % 4U);
    automaticPress = (phase & 1U) != 0;
    momentaryDown = automaticPress && phase == 1U;
    toggleDown = automaticPress && phase == 3U;
  }

  scanButton(leftButton, momentaryDown);
  scanButton(toggleButton, toggleDown);
}
