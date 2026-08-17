#include <Seeed_GFX.h>

// Optional emergency override, normally leave undefined:
// #define SEEED_GUI_WIDGET_PRODUCT Seeed_Product::SENSECAP_WATCHER
Seeed_GFX display;
Seeed_GraphWidget graph(&display);
Seeed_TraceWidget firstTrace(&graph);
Seeed_TraceWidget secondTrace(&graph);
float graphX = 0.0f;
float graphY = 0.0f;
float delta = 7.0f;
bool displayReady = false;

Seeed_Product::Product selectedProduct() {
#ifdef SEEED_GUI_WIDGET_PRODUCT
  return SEEED_GUI_WIDGET_PRODUCT;
#else
  return Seeed_Product::detectIntegratedDisplayProduct();
#endif
}

void drawGraph() {
  const uint16_t width = display.width() > 100 ? display.width() - 80 : display.width();
  const uint16_t height = display.height() > 110 ? display.height() - 80 : display.height();
  graph.create(width, height, display.color565(5, 5, 5));
  graph.setScale(0.0f, 100.0f, -50.0f, 50.0f);
  graph.setGrid(0.0f, 10.0f, -50.0f, 25.0f, TFT_BLUE);
  graph.draw((display.width() - width) / 2, 20);
  display.drawCircle(graph.pointX(50.0f), graph.pointY(0.0f), 5, TFT_MAGENTA);

  // Draw X axis scale labels
  display.setTextDatum(TC_DATUM);
  display.setTextSize(1);
  display.drawNumber(0, graph.pointX(0.0f), graph.pointY(-50.0f) + 3);
  display.drawNumber(50, graph.pointX(50.0f), graph.pointY(-50.0f) + 3);
  display.drawNumber(100, graph.pointX(100.0f), graph.pointY(-50.0f) + 3);

  // Draw Y axis scale labels
  display.setTextDatum(MR_DATUM);
  display.drawNumber(-50, graph.pointX(0.0f), graph.pointY(-50.0f));
  display.drawNumber(0, graph.pointX(0.0f), graph.pointY(0.0f));
  display.drawNumber(50, graph.pointX(0.0f), graph.pointY(50.0f));

  firstTrace.start(TFT_WHITE);
  secondTrace.start(TFT_YELLOW);
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
  // Demonstrate both an in-range trace and boundary clipping.
  firstTrace.start(TFT_RED);
  firstTrace.addPoint(0.0f, 0.0f);
  firstTrace.addPoint(100.0f, 0.0f);
  secondTrace.start(TFT_GREEN);
  secondTrace.addPoint(0.0f, -100.0f);
  secondTrace.addPoint(100.0f, 100.0f);
  firstTrace.start(TFT_WHITE);
  secondTrace.start(TFT_YELLOW);
  displayReady = true;
}

void loop() {
  if (!displayReady) { delay(1000); return; }
  static uint32_t plotTime = 0;
  if (millis() - plotTime < 100) return;
  plotTime = millis();
  firstTrace.addPoint(graphX, graphY);
  secondTrace.addPoint(graphX, graphY / 2.0f);
  graphX += 1.0f;
  graphY += delta;
  if (graphY > 70.0f) { graphY = 70.0f; delta = -7.0f; }
  if (graphY < -70.0f) { graphY = -70.0f; delta = 7.0f; }
  if (graphX > 100.0f) {
    graphX = graphY = 0.0f;
    drawGraph();
    firstTrace.start(TFT_GREEN);
    secondTrace.start(TFT_YELLOW);
  }
}
