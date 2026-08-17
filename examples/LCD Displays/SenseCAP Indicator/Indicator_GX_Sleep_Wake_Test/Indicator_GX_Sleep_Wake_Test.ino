#include <Seeed_GFX.h>

Seeed_GFX display(Seeed_Product::SENSECAP_INDICATOR_GX);

constexpr uint16_t TEST_CYCLES = 100;
constexpr uint32_t VISIBLE_MS = 800;
constexpr uint32_t SLEEP_MS = 700;
constexpr uint32_t RESTORE_CHECK_MS = 1200;

uint16_t cycleNumber = 0;
uint16_t failureCount = 0;
bool testFinished = false;

void drawReferencePattern(uint16_t cycle) {
    display.startWrite();

    display.fillRect(0,   0,   240, 240, TFT_RED);
    display.fillRect(240, 0,   240, 240, TFT_GREEN);
    display.fillRect(0,   240, 240, 240, TFT_BLUE);
    display.fillRect(240, 240, 240, 240, TFT_YELLOW);

    display.fillRoundRect(55, 155, 370, 170, 18, TFT_BLACK);
    display.drawRoundRect(55, 155, 370, 170, 18, TFT_WHITE);

    display.setTextDatum(MC_DATUM);
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.drawString("GX Sleep / Wake", 240, 195, 4);

    char text[32];
    snprintf(text, sizeof(text), "Cycle %u / %u",
             cycle, TEST_CYCLES);
    display.drawString(text, 240, 255, 4);

    display.setTextColor(TFT_CYAN, TFT_BLACK);
    display.drawString("Image must return unchanged",
                       240, 295, 2);

    display.endWrite();
}

bool checkDriver(const char* stage) {
    IDriver& driver = display.panel().driver();
    IBus& bus = driver.bus();

    const bool ok =
        driver.lastOperationError() == DriverOperationError::None &&
        bus.lastError() == 0;

    Serial.printf(
        "[%03u] %s: %s",
        cycleNumber, stage, ok ? "OK" : "FAILED");

    if (!ok && bus.lastErrorMessage()) {
        Serial.print(" - ");
        Serial.print(bus.lastErrorMessage());
    }

    Serial.println();
    return ok;
}

void drawFinishedScreen() {
    display.startWrite();
    display.fillScreen(failureCount == 0 ? TFT_DARKGREEN : TFT_RED);

    display.setTextDatum(MC_DATUM);
    display.setTextColor(TFT_WHITE);

    display.drawString("100 cycles finished", 240, 180, 4);

    char text[32];
    snprintf(text, sizeof(text), "API failures: %u",
             failureCount);
    display.drawString(text, 240, 240, 4);

    display.drawString("Touch screen to verify touch",
                       240, 300, 2);
    display.endWrite();
}

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println();
    Serial.println("Indicator GX sleep/wake test starting");

    if (!display.begin()) {
        Serial.print("Display initialization failed: ");
        Serial.println(display.lastResult().message);
        while (true) delay(1000);
    }

    // The GX product config includes its 180-degree touch mounting correction.
    // setRotation() keeps LCD and touch aligned; no setTouchRotation(2) patch
    // is required in this or any other official Indicator example.
    display.setRotation(0);
}

void loop() {
    if (testFinished) {
        // 测完100次后继续验证触摸是否正常
        int32_t x;
        int32_t y;

        if (display.getTouch(&x, &y)) {
            display.fillCircle(x, y, 8, TFT_WHITE);
            Serial.printf("Touch after test: x=%ld y=%ld\n",
                          static_cast<long>(x),
                          static_cast<long>(y));
            delay(80);
        }
        return;
    }

    ++cycleNumber;

    // 先画本轮参考画面
    drawReferencePattern(cycleNumber);
    delay(VISIBLE_MS);

    IDriver& driver = display.panel().driver();

    // 清除之前的粘滞错误
    driver.clearOperationError();

    Serial.printf("[%03u] entering sleep\n", cycleNumber);
    display.panel().sleep();

    if (!checkDriver("sleep")) {
        ++failureCount;
    }

    // 正常现象：背光熄灭
    delay(SLEEP_MS);

    driver.clearOperationError();

    Serial.printf("[%03u] waking\n", cycleNumber);
    display.panel().wake();

    if (!checkDriver("wake")) {
        ++failureCount;
    }

    /*
     * 这里故意不重画。
     * 唤醒后应自动恢复本轮睡前的四色画面和 Cycle 编号。
     */
    delay(RESTORE_CHECK_MS);

    if (cycleNumber >= TEST_CYCLES) {
        testFinished = true;
        drawFinishedScreen();

        Serial.println();
        Serial.println("Sleep/wake test finished");
        Serial.printf("API failure count: %u\n", failureCount);
        Serial.println("Now touch the screen to verify touch recovery.");
    }
}
