#include <Seeed_UI.h>

Seeed_GFX display(Seeed_Product::SENSECAP_WATCHER);
SenseCAPWatcherInput controls(display);
UiInputHub input;

int32_t wheelValue = 0;
bool knobPressed = false;

void drawValue() {
    display.fillRoundRect(56, 135, 300, 142, 20, TFT_NAVY);
    display.setTextColor(TFT_WHITE, TFT_NAVY);
    display.setTextDatum(MC_DATUM);
    display.drawNumber(wheelValue, 206, 206, 7);

    display.fillRoundRect(76, 306, 260, 54, 14,
                          knobPressed ? TFT_GREEN : TFT_DARKGREY);
    display.setTextColor(knobPressed ? TFT_BLACK : TFT_WHITE,
                         knobPressed ? TFT_GREEN : TFT_DARKGREY);
    display.drawString(knobPressed ? "PRESSED" : "PRESS KNOB",
                       206, 333, 4);
}

void setup() {
    Serial.begin(115200);

    if (!display.begin()) {
        Serial.print("Watcher display init failed: ");
        Serial.println(display.lastResult().message);
        return;
    }

    if (!uiOk(input.add(controls))) {
        Serial.println("Watcher control source registration failed");
        return;
    }
    installSenseCAPWatcherDefaultActionMap(input.actionMap());
    if (!uiOk(input.begin())) {
        Serial.println("Watcher knob/button initialization failed");
        return;
    }

    display.fillScreen(TFT_BLACK);
    display.setTextDatum(MC_DATUM);
    display.setTextColor(TFT_CYAN, TFT_BLACK);
    display.drawString("WATCHER CONTROLS", 206, 70, 4);
    drawValue();
    Serial.println("Rotate the wheel or press it");
}

void loop() {
    input.scan(millis());

    UiEvent event;
    bool changed = false;
    while (input.poll(event)) {
        if (event.type == UiEventType::Scroll) {
            wheelValue += event.delta;
            Serial.print("wheel delta: ");
            Serial.print(event.delta);
            Serial.print(", value: ");
            Serial.println(wheelValue);
            changed = true;
        } else if (event.type == UiEventType::Action &&
                   event.action == UiAction::Activate) {
            if (event.phase == UiActionPhase::Pressed) {
                knobPressed = true;
                Serial.println("knob pressed");
            } else if (event.phase == UiActionPhase::Released) {
                knobPressed = false;
                Serial.println("knob released");
            } else if (event.phase == UiActionPhase::LongPressed) {
                Serial.println("knob long pressed");
            }
            changed = true;
        }
    }

    if (changed) drawValue();
    delay(2);
}
