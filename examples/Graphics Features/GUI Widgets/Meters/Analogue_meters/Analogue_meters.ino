#include <Seeed_GFX.h>
#include <math.h>

// Optional emergency override, normally leave undefined:
// #define SEEED_GUI_WIDGET_PRODUCT Seeed_Product::SenseCAP_Watcher
Seeed_GFX display;
Seeed_MeterWidget amps(&display);
Seeed_MeterWidget volts(&display);
Seeed_MeterWidget ohms(&display);
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

  const uint16_t meterWidth = display.width() > 300 ? 280 : display.width() - 12;
  const uint16_t meterHeight = (display.height() - 16) / 3;
  const int16_t x = (display.width() - meterWidth) / 2;
  amps.setZones(75, 100, 50, 75, 25, 50, 0, 25);
  volts.setZones(0, 100, 25, 75, 0, 0, 40, 60);
  ohms.setZones(0, 0, 0, 0, 0, 0, 0, 0);
  amps.setLabels("0", "0.5", "1.0", "1.5", "2.0");
  volts.setLabels("0", "2.5", "5", "7.5", "10");
  ohms.setLabels("0", "25", "50", "75", "100");

  displayReady = amps.draw(x, 4, meterWidth, meterHeight, 2.0f, "mA") &&
                 volts.draw(x, 6 + meterHeight, meterWidth, meterHeight, 10.0f, "V") &&
                 ohms.draw(x, 8 + meterHeight * 2, meterWidth, meterHeight, 100.0f, "R");
  if (!displayReady) Serial.println("Display is too small for the three-meter layout");
}

void loop() {
  if (!displayReady) { delay(1000); return; }
  static uint32_t updateTime = 0;
  static int16_t degrees = 0;
  if (millis() - updateTime < 180) return;
  updateTime = millis();
  degrees = (degrees + 4) % 360;
  const float percent = 50.0f + 50.0f * sinf(degrees * 0.01745329252f);
  amps.update(percent * 0.02f);
  volts.update(percent * 0.10f);
  ohms.update(percent);
}
