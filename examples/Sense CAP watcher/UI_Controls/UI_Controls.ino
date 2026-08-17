/**
 * Product: SenseCAP Watcher, 412x412 SPD2010 touch display
 * Demo:    one UI controlled by touch, rotary wheel and wheel push button
 *
 * Controls:
 *   Touch          Tap any action button
 *   Rotate wheel   Move the yellow focus frame
 *   Press wheel    Activate the focused button
 *
 * The three input methods share one UiInputHub and one widget tree. Touch
 * changes focus automatically through hit testing; wheel Scroll events move
 * focus; the wheel push switch is mapped to UiAction::Activate.
 */

#include <Seeed_UI.h>
#include <stdio.h>

Seeed_GFX display(Seeed_Product::SENSECAP_WATCHER);

namespace {

constexpr int16_t kScreenSize = 412;

enum : uint16_t {
    CMD_DECREASE = 1,
    CMD_INCREASE,
    CMD_RESET,
};

void applyWatcherTheme(UiTheme& theme) {
    theme.colors.background = 0x0000;
    theme.colors.surface = 0x18E3;
    theme.colors.textPrimary = 0xFFFF;
    theme.colors.textSecondary = 0x9CF3;
    theme.colors.accent = 0x0451;
    theme.colors.focus = 0xFFE0;
    theme.colors.disabled = 0x4208;
    theme.metrics.borderWidth = 3;
    theme.metrics.touchSlop = 8;
    theme.bodyTextSize = 2;
    theme.smallTextSize = 1;
}

class WatcherControlScreen : public UiScreen {
public:
    explicit WatcherControlScreen(UiInputHub& input)
        : _input(input),
          _title("WATCHER UI", 101),
          _value("50", 102),
          _progress(50, 0, 100, 103),
          _decrease("- 10", {runCommand, this, CMD_DECREASE}, 104),
          _increase("+ 10", {runCommand, this, CMD_INCREASE}, 105),
          _reset("RESET", {runCommand, this, CMD_RESET}, 106),
          _hint("TOUCH OR ROTATE + PRESS", 107),
          _status("READY", 108) {}

    void attachFocus(UiFocusManager& focus) { _focus = &focus; }

    UiStatus onCreate() override {
        _root.setBackground(0x0000);
        _root.addChild(_title);
        _root.addChild(_value);
        _root.addChild(_progress);
        _root.addChild(_decrease);
        _root.addChild(_increase);
        _root.addChild(_reset);
        _root.addChild(_hint);
        _root.addChild(_status);

        // Keep interactive content inside the useful center of the round LCD.
        _title.setTextSize(2);
        _title.setColor(0x07FF);
        _title.setBounds({134, 28, 160, 24});

        _value.setTextSize(4);
        _value.setColor(0xFFFF);
        _value.setBounds({176, 72, 100, 38});

        _progress.setBounds({56, 122, 300, 22});

        _decrease.setBounds({56, 164, 300, 48});
        _increase.setBounds({56, 222, 300, 48});
        _reset.setBounds({56, 280, 300, 48});

        _hint.setTextSize(1);
        _hint.setColor(0x9CF3);
        _hint.setBounds({128, 348, 220, 16});

        _status.setTextSize(1);
        _status.setColor(0xFFE0);
        _status.setBounds({122, 371, 220, 16});
        return UiStatus::Ok;
    }

    bool onEvent(UiEvent& event) override {
        if (event.type != UiEventType::Scroll || event.delta == 0 ||
            !_focus) {
            return false;
        }

        int16_t steps = event.delta;
        while (steps > 0) {
            _focus->moveNext(_root, true);
            --steps;
        }
        while (steps < 0) {
            _focus->movePrevious(_root, true);
            ++steps;
        }
        updateFocusStatus();
        return true;
    }

    UiWidget& root() override { return _root; }

private:
    static void runCommand(void* context, uint16_t command) {
        static_cast<WatcherControlScreen*>(context)->applyCommand(command);
    }

    const char* inputName() const {
        return _input.mode() == UiInputMode::Touch ? "TOUCH" : "KNOB";
    }

    void applyCommand(uint16_t command) {
        const char* action = "";
        if (command == CMD_DECREASE) {
            _current -= 10;
            action = "DECREASE";
        } else if (command == CMD_INCREASE) {
            _current += 10;
            action = "INCREASE";
        } else if (command == CMD_RESET) {
            _current = 50;
            action = "RESET";
        }

        if (_current < 0) _current = 0;
        if (_current > 100) _current = 100;
        snprintf(_valueText, sizeof(_valueText), "%ld",
                 static_cast<long>(_current));
        snprintf(_statusText, sizeof(_statusText), "%s: %s",
                 inputName(), action);
        _value.setText(_valueText);
        _progress.setValue(_current);
        _status.setText(_statusText);

        Serial.print(inputName());
        Serial.print(" command ");
        Serial.print(action);
        Serial.print(", value=");
        Serial.println(_current);
    }

    void updateFocusStatus() {
        const char* selected = "DECREASE";
        if (_focus->focused() == &_increase) selected = "INCREASE";
        else if (_focus->focused() == &_reset) selected = "RESET";
        snprintf(_statusText, sizeof(_statusText), "WHEEL: %s", selected);
        _status.setText(_statusText);
    }

    UiInputHub& _input;
    UiFocusManager* _focus = nullptr;
    UiContainer _root;
    UiLabel _title;
    UiLabel _value;
    UiProgressBar _progress;
    UiButton _decrease;
    UiButton _increase;
    UiButton _reset;
    UiLabel _hint;
    UiLabel _status;
    int32_t _current = 50;
    char _valueText[8] = "50";
    char _statusText[32] = "READY";
};

} // namespace

UiInputHub inputHub;
UiTheme theme = uiWioTerminalTheme();
TouchInput touchInput(display, 2, 600, 10);
SenseCAPWatcherInput watcherControls(display);
WatcherControlScreen controlScreen(inputHub);
UiApplication ui(display, inputHub, theme);

bool uiReady = false;

void setup() {
    Serial.begin(115200);
    applyWatcherTheme(theme);

    const uint32_t startedAt = millis();
    if (!display.begin()) {
        Serial.print("Watcher display init failed: ");
        Serial.println(display.lastResult().message);
        return;
    }

    if (!uiOk(inputHub.add(touchInput))) {
        Serial.println("Watcher touch registration failed");
        return;
    }
    if (!uiOk(inputHub.add(watcherControls))) {
        Serial.println("Watcher wheel registration failed");
        return;
    }
    installSenseCAPWatcherDefaultActionMap(inputHub.actionMap());
    controlScreen.attachFocus(ui.focus());

    if (!uiOk(ui.begin(controlScreen))) {
        Serial.println("Watcher UI initialization failed");
        return;
    }
    if (!uiOk(ui.tick(millis()))) {
        Serial.println("Watcher first UI frame failed");
        return;
    }

    uiReady = true;
    Serial.print("Watcher touch/wheel UI ready, ms: ");
    Serial.println(millis() - startedAt);
}

void loop() {
    if (!uiReady) {
        delay(100);
        return;
    }

    // About 100 Hz keeps touch responsive without polling either I2C device
    // excessively. Rendering only occurs after an input invalidates a widget.
    static uint32_t lastTickMs = 0;
    const uint32_t now = millis();
    if (now - lastTickMs < 10U) return;
    lastTickMs = now;
    (void)ui.tick(now);
}
