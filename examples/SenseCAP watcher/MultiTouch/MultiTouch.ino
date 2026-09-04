#include <Seeed_GFX.h>

Seeed_GFX display(Seeed_Product::SenseCAP_Watcher);

TouchPoint points[10];
TouchPoint previousVisible[10];
uint8_t previousVisibleCount = 0;
uint8_t previousTouchCount = 0;
uint32_t lastPoll = 0;

constexpr int16_t kFrameX = 35;
constexpr int16_t kFrameY = 82;
constexpr int16_t kFrameW = 342;
constexpr int16_t kFrameH = 280;
constexpr int16_t kMarkerMinX = 55;
constexpr int16_t kMarkerMaxX = 357;
constexpr int16_t kMarkerMinY = 140;
constexpr int16_t kMarkerMaxY = 342;

bool markerIsVisible(const TouchPoint& point) {
    return point.x >= kMarkerMinX && point.x <= kMarkerMaxX &&
           point.y >= kMarkerMinY && point.y <= kMarkerMaxY;
}

uint8_t collectVisiblePoints(const TouchPoint* source, uint8_t count,
                             TouchPoint* output) {
    uint8_t visibleCount = 0;
    for (uint8_t i = 0; i < count && visibleCount < 10; ++i) {
        if (markerIsVisible(source[i])) output[visibleCount++] = source[i];
    }
    return visibleCount;
}

bool visiblePointsChanged(const TouchPoint* current, uint8_t currentCount) {
    if (currentCount != previousVisibleCount) return true;
    for (uint8_t i = 0; i < currentCount; ++i) {
        const int16_t dx = static_cast<int16_t>(current[i].x) -
                           static_cast<int16_t>(previousVisible[i].x);
        const int16_t dy = static_cast<int16_t>(current[i].y) -
                           static_cast<int16_t>(previousVisible[i].y);
        if (current[i].id != previousVisible[i].id ||
            abs(dx) > 1 || abs(dy) > 1) {
            return true;
        }
    }
    return false;
}

void drawTouchCount(uint8_t count) {
    display.fillRect(146, 90, 120, 20, TFT_BLACK);
    display.setTextDatum(TC_DATUM);
    display.setTextColor(TFT_CYAN, TFT_BLACK);
    char label[24];
    snprintf(label, sizeof(label), "points: %u", count);
    display.drawString(label, display.width() / 2, 92, 2);
}

void eraseMarker(const TouchPoint& point) {
    // Covers the 10-pixel circle and the short "#id" label above it.
    display.fillRect(static_cast<int16_t>(point.x) - 18,
                     static_cast<int16_t>(point.y) - 28,
                     37, 47, TFT_BLACK);
}

void drawMarker(const TouchPoint& point, uint8_t index) {
    const uint16_t color = (index & 1U) ? TFT_YELLOW : TFT_GREEN;
    display.fillCircle(point.x, point.y, 10, color);
    display.setTextDatum(BC_DATUM);
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    char label[8];
    snprintf(label, sizeof(label), "#%u", point.id);
    display.drawString(label, point.x, point.y - 14, 1);
}

void setup() {
    Serial.begin(115200);
    if (!display.begin()) {
        Serial.print("SenseCAP Watcher display init failed: ");
        Serial.println(display.lastResult().message);
        return;
    }

    display.fillScreen(TFT_BLACK);
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.setTextDatum(TC_DATUM);
    display.drawString("SPD2010 MultiTouch", display.width() / 2, 28, 2);
    display.drawString("Touch with one or more fingers",
                       display.width() / 2, 52, 2);
    display.drawRoundRect(kFrameX, kFrameY, kFrameW, kFrameH,
                          16, TFT_DARKGREY);
    drawTouchCount(0);
    Serial.printf("Touch capacity: %u points\n",
                  display.touchPointCapacity());
}

void loop() {
    if (millis() - lastPoll < 40) return;
    lastPoll = millis();

    const uint8_t count = display.getTouchPoints(points, 10);
    TouchPoint visible[10];
    const uint8_t visibleCount =
        collectVisiblePoints(points, count, visible);
    const bool markersChanged =
        visiblePointsChanged(visible, visibleCount);
    const bool countChanged = count != previousTouchCount;

    if (!markersChanged && !countChanged) return;

    display.startWrite();
    if (markersChanged) {
        for (uint8_t i = 0; i < previousVisibleCount; ++i) {
            eraseMarker(previousVisible[i]);
        }
        for (uint8_t i = 0; i < visibleCount; ++i) {
            drawMarker(visible[i], i);
            previousVisible[i] = visible[i];
        }
        previousVisibleCount = visibleCount;
    }
    if (countChanged) drawTouchCount(count);
    display.endWrite();

    if (countChanged || markersChanged) {
        Serial.printf("touch points=%u, visible markers=%u\n",
                      count, visibleCount);
    }
    for (uint8_t i = 0; i < count; ++i) {
        Serial.printf("point[%u] id=%u x=%u y=%u strength=%u\n",
                      i, points[i].id, points[i].x, points[i].y,
                      points[i].strength);
    }
    previousTouchCount = count;
}
