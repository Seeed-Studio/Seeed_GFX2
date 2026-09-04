#include <Seeed_GFX.h>
#include <stdio.h>

// Optional emergency override, normally leave undefined:
// #define SEEED_GUI_WIDGET_PRODUCT Seeed_Product::SenseCAP_Watcher
Seeed_GFX display;
Seeed_SliderWidget horizontalSlider(&display);
Seeed_SliderWidget verticalSlider(&display);
bool touchAvailable = false;
bool displayReady = false;

Seeed_Product::Product selectedProduct() {
#ifdef SEEED_GUI_WIDGET_PRODUCT
  return SEEED_GUI_WIDGET_PRODUCT;
#else
  return Seeed_Product::detectIntegratedDisplayProduct();
#endif
}

void setup() {
  Serial.begin(115200);
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
  touchAvailable = display.touchPointCapacity() > 0;

  Seeed_SliderConfig horizontal;
  horizontal.slotLength = display.width() > 140 ? display.width() - 100 : 100;
  horizontal.slotWidth = 9;
  horizontal.knobWidth = 18;
  horizontal.knobHeight = 30;
  horizontal.slotColor = TFT_BLUE;
  horizontal.markerColor = TFT_RED;
  horizontal.startValue = 50;
  if (!horizontalSlider.draw(30, display.height() / 3, horizontal)) {
    Serial.println("Could not create horizontal slider");
    return;
  }
  int16_t bx, by;
  uint16_t bw, bh;
  horizontalSlider.getBounds(&bx, &by, &bw, &bh);
  display.drawRect(bx, by, bw, bh, TFT_DARKGREY);

  Seeed_SliderConfig vertical = horizontal;
  vertical.orientation = Seeed_SliderOrientation::Vertical;
  vertical.slotLength = display.height() > 150 ? display.height() - 100 : 100;
  vertical.knobWidth = 30;
  vertical.knobHeight = 18;
  vertical.markerColor = TFT_GREEN;
  // Demonstrate the original example's reversed vertical scale (100 -> 0).
  vertical.minimum = 100;
  vertical.maximum = 0;
  if (!verticalSlider.draw(display.width() - 60, 30, vertical)) {
    Serial.println("Could not create vertical slider");
    return;
  }
  verticalSlider.getBounds(&bx, &by, &bw, &bh);
  display.drawRect(bx, by, bw, bh, TFT_DARKGREY);

  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.drawString("Touch and drag", 18, 12, 2);
  Serial.println(touchAvailable ? "Touch sliders" :
                                   "No touch panel: automatic slider demo");
  displayReady = true;
}

void loop() {
  if (!displayReady) { delay(1000); return; }
  static uint32_t lastScan = 0;
  static int32_t automaticValue = 0;
  static int8_t direction = 1;
  if (millis() - lastScan < 25) return;
  lastScan = millis();

  int32_t x = 0, y = 0;
  if (touchAvailable && display.getTouch(&x, &y)) {
    char message[32];
    if (horizontalSlider.checkTouch(x, y)) {
      snprintf(message, sizeof(message), "Horizontal = %ld",
               static_cast<long>(horizontalSlider.value()));
      Serial.println(message);
    }
    if (verticalSlider.checkTouch(x, y)) {
      snprintf(message, sizeof(message), "Vertical = %ld",
               static_cast<long>(verticalSlider.value()));
      Serial.println(message);
    }
    return;
  }

  if (!touchAvailable && millis() % 75U < 25U) {
    automaticValue += direction;
    if (automaticValue >= 100) { automaticValue = 100; direction = -1; }
    if (automaticValue <= 0) { automaticValue = 0; direction = 1; }
    horizontalSlider.setValue(automaticValue);
    verticalSlider.setValue(100 - automaticValue);
  }
}
