#include <Seeed_GFX.h>

// Optional emergency override, normally leave undefined:
// #define SEEED_GUI_WIDGET_PRODUCT Seeed_Product::SenseCAP_Watcher
Seeed_GFX display;
Seeed_GraphWidget graph(&display);
Seeed_TraceWidget trace(&graph);
float graphX = 0.0f;
float graphY = 0.0f;
float delta = 10.0f;
bool displayReady = false;

Seeed_Product::Product selectedProduct() {
#ifdef SEEED_GUI_WIDGET_PRODUCT
  return SEEED_GUI_WIDGET_PRODUCT;
#else
  return Seeed_Product::detectIntegratedDisplayProduct();
#endif
}

void drawGraph() {
  const uint16_t width = display.width() > 80 ? display.width() - 60 : display.width();
  const uint16_t height = display.height() > 100 ? display.height() - 70 : display.height();
  graph.create(width, height, display.color565(5, 5, 5));
  graph.setScale(0.0f, 100.0f, -512.0f, 512.0f);
  graph.setGrid(0.0f, 20.0f, -512.0f, 64.0f, TFT_BLUE);
  graph.draw((display.width() - width) / 2, 20);
  trace.start(TFT_WHITE);
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
  drawGraph();
  // Demonstrate joining two points before the animated trace starts.
  trace.start(TFT_RED);
  trace.addPoint(0.0f, 0.0f);
  trace.addPoint(100.0f, 0.0f);
  trace.start(TFT_WHITE);
  displayReady = true;
}

void loop() {
  if (!displayReady) { delay(1000); return; }
  static uint32_t plotTime = 0;
  if (millis() - plotTime < 100) return;
  plotTime = millis();
  trace.addPoint(graphX, graphY);
  graphX += 1.0f;
  if (graphY > 500.0f) delta = -10.0f;
  if (graphY < -500.0f) delta = 10.0f;
  graphY += delta;
  if (graphX > 100.0f) {
    graphX = graphY = 0.0f;
    drawGraph();
    trace.start(TFT_GREEN);
  }
}
