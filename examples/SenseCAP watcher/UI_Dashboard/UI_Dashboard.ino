/**
 * Product: SenseCAP Watcher, 412x412 SPD2010 touch display
 * Demo:    multi-page UI controlled by touch, rotary wheel and wheel button
 *
 * Pages:
 *   Welcome     product mark and Enter button
 *   Main Menu   Dashboard / Controls / Settings / Device Info
 *   Dashboard   uptime, demo activity and real internal-heap usage
 *   Controls    shared counter with -10 / +10 / Reset actions
 *   Settings    dark theme, dashboard animation and LCD brightness
 *   Device Info live ESP32-S3 flash, heap and PSRAM information
 *
 * Controls:
 *   Touch          Tap an item, button or toggle
 *   Rotate wheel   Select a menu item or move the focus frame
 *   Press wheel    Activate the focused item
 *
 * The screen is round, so interactive widgets stay inside a central
 * 300-pixel-wide safe area. The dashboard refreshes only once per second;
 * there is no continuous full-screen redraw that can cause visible flicker.
 */

#include <Seeed_UI.h>
#include <stdio.h>

Seeed_GFX display(Seeed_Product::SenseCAP_Watcher);

namespace {

constexpr int16_t kScreenSize = 412;
constexpr int16_t kSafeX = 56;
constexpr int16_t kSafeW = 300;
constexpr uint16_t kCyan = 0x07FF;
constexpr uint16_t kGreen = 0x07E0;
constexpr uint16_t kOrange = 0xFD20;
constexpr uint16_t kMutedBlue = 0x4D7F;
constexpr uint32_t kDashboardIntervalMs = 1000;

struct AppState {
    int32_t counter = 50;
    uint8_t brightness = 224;
    bool dark = true;
    bool animations = true;
};

enum : uint16_t {
    CMD_ENTER = 1,
    CMD_OPEN_DASHBOARD,
    CMD_OPEN_CONTROLS,
    CMD_OPEN_SETTINGS,
    CMD_OPEN_INFO,
    CMD_BACK,
    CMD_MINUS_TEN,
    CMD_PLUS_TEN,
    CMD_RESET,
    CMD_DARK_THEME,
    CMD_ANIMATIONS,
    CMD_BRIGHTNESS_MINUS,
    CMD_BRIGHTNESS_PLUS
};

void applyWatcherTheme(UiTheme& theme, bool dark) {
    if (dark) {
        theme.colors.background = 0x0000;
        theme.colors.surface = 0x18E3;
        theme.colors.textPrimary = 0xFFFF;
        theme.colors.textSecondary = 0x9CF3;
        theme.colors.accent = 0x0451;
        theme.colors.focus = 0xFFE0;
        theme.colors.disabled = 0x4208;
    } else {
        theme.colors.background = 0xEF7D;
        theme.colors.surface = 0xFFFF;
        theme.colors.textPrimary = 0x0000;
        theme.colors.textSecondary = 0x4208;
        theme.colors.accent = 0x04BF;
        theme.colors.focus = 0xFD20;
        theme.colors.disabled = 0xBDF7;
    }
    theme.metrics.borderWidth = 3;
    theme.metrics.touchSlop = 8;
    theme.bodyTextSize = 2;
    theme.smallTextSize = 1;
}

// Unlike UiContainer::setBackground(), this container reads the current theme
// each time it renders. The Settings page can therefore recolor every page
// without reconstructing the navigation stack.
class ThemedContainer : public UiContainer {
public:
    explicit ThemedContainer(UiId id = UI_ID_NONE) : UiContainer(id) {}

    void render(UiCanvas& canvas, const UiTheme& theme) override {
        if (!visible()) return;
        canvas.fillRect(bounds(), theme.colors.background);
        for (UiWidget* child = firstChild(); child; child = child->nextSibling()) {
            if (child->visible()) child->render(canvas, theme);
        }
    }
};

// A small code-drawn product mark avoids storing a large bitmap in flash.
class WatcherMark : public UiWidget {
public:
    explicit WatcherMark(UiId id = UI_ID_NONE) : UiWidget(id) {}

    void render(UiCanvas& canvas, const UiTheme& theme) override {
        if (!visible()) return;
        const UiRect& b = bounds();
        canvas.fillRect({static_cast<int16_t>(b.x + 18),
                         static_cast<int16_t>(b.y + 18),
                         static_cast<int16_t>(b.w - 36),
                         static_cast<int16_t>(b.h - 36)}, theme.colors.surface);
        canvas.drawRect({static_cast<int16_t>(b.x + 8),
                         static_cast<int16_t>(b.y + 8),
                         static_cast<int16_t>(b.w - 16),
                         static_cast<int16_t>(b.h - 16)}, theme.colors.accent, 4);
        canvas.fillRect({static_cast<int16_t>(b.x + b.w / 2 - 8),
                         static_cast<int16_t>(b.y + b.h / 2 - 8),
                         16, 16}, theme.colors.focus);
    }
};

bool moveFocusWithWheel(UiEvent& event, UiFocusManager* focus,
                        UiWidget& root) {
    if (event.type != UiEventType::Scroll || event.delta == 0 || !focus) {
        return false;
    }
    int16_t steps = event.delta;
    while (steps > 0) {
        focus->moveNext(root, true);
        --steps;
    }
    while (steps < 0) {
        focus->movePrevious(root, true);
        ++steps;
    }
    return true;
}

class DashboardScreen : public UiScreen {
public:
    explicit DashboardScreen(AppState& state)
        : _state(state),
          _title("SYSTEM DASHBOARD", 101),
          _uptime("UPTIME  00:00:00", 102),
          _activityLabel("ACTIVITY  0%", 103),
          _activity(0, 0, 100, 104),
          _heapLabel("HEAP USED  0%", 105),
          _heap(0, 0, 100, 106),
          _memory("FREE HEAP  0 KB", 107),
          _back("< BACK", {runCommand, this, CMD_BACK}, 108),
          _hint("TOUCH / ROTATE / PRESS", 109) {}

    void attach(UiNavigator& navigator, UiFocusManager& focus) {
        _navigator = &navigator;
        _focus = &focus;
    }

    UiStatus onCreate() override {
        _root.addChild(_title);
        _root.addChild(_uptime);
        _root.addChild(_activityLabel);
        _root.addChild(_activity);
        _root.addChild(_heapLabel);
        _root.addChild(_heap);
        _root.addChild(_memory);
        _root.addChild(_back);
        _root.addChild(_hint);

        _title.setTextSize(2);
        _title.setColor(kCyan);
        // Near the top, the round LCD is much narrower than its 412-pixel
        // rectangular framebuffer. Keep long headings below that narrow cap.
        _title.setBounds({110, 40, 205, 22});
        _uptime.setTextSize(2);
        _uptime.setColor(kGreen);
        _uptime.setBounds({98, 72, 230, 22});

        _activityLabel.setTextSize(1);
        _activityLabel.setBounds({kSafeX, 108, kSafeW, 16});
        _activity.setBounds({kSafeX, 130, kSafeW, 24});
        _heapLabel.setTextSize(1);
        _heapLabel.setBounds({kSafeX, 177, kSafeW, 16});
        _heap.setBounds({kSafeX, 199, kSafeW, 24});
        _memory.setTextSize(1);
        _memory.setColor(kMutedBlue);
        _memory.setBounds({kSafeX, 244, kSafeW, 16});

        _back.setBounds({kSafeX, 286, kSafeW, 48});
        _hint.setTextSize(1);
        _hint.setBounds({115, 360, 220, 14});
        return UiStatus::Ok;
    }

    void onEnter(const void*) override {
        _lastUpdateMs = 0xFFFFFFFFU;
    }

    void update(uint32_t nowMs) override {
        if (_lastUpdateMs != 0xFFFFFFFFU &&
            nowMs - _lastUpdateMs < kDashboardIntervalMs) {
            return;
        }
        _lastUpdateMs = nowMs;

        const uint32_t seconds = nowMs / 1000U;
        snprintf(_uptimeText, sizeof(_uptimeText), "UPTIME  %02lu:%02lu:%02lu",
                 static_cast<unsigned long>((seconds / 3600U) % 100U),
                 static_cast<unsigned long>((seconds / 60U) % 60U),
                 static_cast<unsigned long>(seconds % 60U));
        _uptime.setText(_uptimeText);

        const uint32_t phase = (seconds * 17U) % 101U;
        const int activity = _state.animations ?
            static_cast<int>(18U + (phase * 64U) / 100U) : 42;
        _activity.setValue(activity);
        snprintf(_activityText, sizeof(_activityText), "ACTIVITY  %d%%", activity);
        _activityLabel.setText(_activityText);

        const uint32_t heapTotal = ESP.getHeapSize();
        const uint32_t heapFree = ESP.getFreeHeap();
        const uint32_t heapUsed = heapTotal > heapFree ? heapTotal - heapFree : 0;
        const int heapPercent = heapTotal ?
            static_cast<int>((heapUsed * 100ULL) / heapTotal) : 0;
        _heap.setValue(heapPercent);
        snprintf(_heapText, sizeof(_heapText), "HEAP USED  %d%%", heapPercent);
        _heapLabel.setText(_heapText);
        snprintf(_memoryText, sizeof(_memoryText), "FREE HEAP  %lu KB",
                 static_cast<unsigned long>(heapFree / 1024U));
        _memory.setText(_memoryText);
    }

    bool onEvent(UiEvent& event) override {
        return moveFocusWithWheel(event, _focus, _root);
    }

    UiWidget& root() override { return _root; }

private:
    static void runCommand(void* context, uint16_t) {
        auto* self = static_cast<DashboardScreen*>(context);
        if (self->_navigator) (void)self->_navigator->pop();
    }

    AppState& _state;
    UiNavigator* _navigator = nullptr;
    UiFocusManager* _focus = nullptr;
    ThemedContainer _root;
    UiLabel _title, _uptime, _activityLabel, _heapLabel, _memory;
    UiProgressBar _activity, _heap;
    UiButton _back;
    UiLabel _hint;
    uint32_t _lastUpdateMs = 0xFFFFFFFFU;
    char _uptimeText[28] = "UPTIME  00:00:00";
    char _activityText[24] = "ACTIVITY  0%";
    char _heapText[24] = "HEAP USED  0%";
    char _memoryText[28] = "FREE HEAP  0 KB";
};

class ControlsScreen : public UiScreen {
public:
    ControlsScreen(AppState& state, UiInputHub& input)
        : _state(state), _input(input),
          _title("INTERACTIVE CONTROL", 201),
          _value("50", 202),
          _progress(50, 0, 100, 203),
          _minus("- 10", {runCommand, this, CMD_MINUS_TEN}, 204),
          _plus("+ 10", {runCommand, this, CMD_PLUS_TEN}, 205),
          _reset("RESET VALUE", {runCommand, this, CMD_RESET}, 206),
          _back("< BACK", {runCommand, this, CMD_BACK}, 207),
          _status("READY", 208) {}

    void attach(UiNavigator& navigator, UiFocusManager& focus) {
        _navigator = &navigator;
        _focus = &focus;
    }

    UiStatus onCreate() override {
        _root.addChild(_title);
        _root.addChild(_value);
        _root.addChild(_progress);
        _root.addChild(_minus);
        _root.addChild(_plus);
        _root.addChild(_reset);
        _root.addChild(_back);
        _root.addChild(_status);

        _title.setTextSize(2);
        _title.setColor(kCyan);
        _title.setBounds({92, 40, 235, 22});
        _value.setTextSize(4);
        _value.setColor(kOrange);
        _value.setBounds({174, 75, 100, 38});
        _progress.setBounds({kSafeX, 119, kSafeW, 22});

        _minus.setBounds({kSafeX, 151, 142, 48});
        _plus.setBounds({214, 151, 142, 48});
        _reset.setBounds({kSafeX, 211, kSafeW, 46});
        _back.setBounds({kSafeX, 269, kSafeW, 46});
        _status.setTextSize(1);
        _status.setBounds({106, 344, 240, 16});
        syncValue("READY");
        return UiStatus::Ok;
    }

    void onEnter(const void*) override { syncValue("CURRENT VALUE"); }

    bool onEvent(UiEvent& event) override {
        return moveFocusWithWheel(event, _focus, _root);
    }

    UiWidget& root() override { return _root; }

private:
    static void runCommand(void* context, uint16_t command) {
        static_cast<ControlsScreen*>(context)->applyCommand(command);
    }

    void applyCommand(uint16_t command) {
        if (command == CMD_BACK) {
            if (_navigator) (void)_navigator->pop();
            return;
        }

        const char* action = "RESET";
        if (command == CMD_MINUS_TEN) {
            _state.counter -= 10;
            action = "-10";
        } else if (command == CMD_PLUS_TEN) {
            _state.counter += 10;
            action = "+10";
        } else {
            _state.counter = 50;
        }
        if (_state.counter < 0) _state.counter = 0;
        if (_state.counter > 100) _state.counter = 100;
        syncValue(action);
    }

    void syncValue(const char* action) {
        snprintf(_valueText, sizeof(_valueText), "%ld",
                 static_cast<long>(_state.counter));
        _value.setText(_valueText);
        _progress.setValue(_state.counter);
        snprintf(_statusText, sizeof(_statusText), "%s: %s",
                 _input.mode() == UiInputMode::Touch ? "TOUCH" : "KNOB",
                 action);
        _status.setText(_statusText);
    }

    AppState& _state;
    UiInputHub& _input;
    UiNavigator* _navigator = nullptr;
    UiFocusManager* _focus = nullptr;
    ThemedContainer _root;
    UiLabel _title, _value;
    UiProgressBar _progress;
    UiButton _minus, _plus, _reset, _back;
    UiLabel _status;
    char _valueText[8] = "50";
    char _statusText[32] = "READY";
};

class SettingsScreen : public UiScreen {
public:
    SettingsScreen(AppState& state, UiTheme& theme)
        : _state(state), _theme(theme),
          _title("DISPLAY SETTINGS", 301),
          _dark("DARK THEME", state.dark,
                {runCommand, this, CMD_DARK_THEME}, 302),
          _animations("LIVE ACTIVITY", state.animations,
                      {runCommand, this, CMD_ANIMATIONS}, 303),
          _brightnessLabel("BACKLIGHT", 304),
          _minus("-", {runCommand, this, CMD_BRIGHTNESS_MINUS}, 305),
          _brightness(state.brightness, 0, 255, 306),
          _plus("+", {runCommand, this, CMD_BRIGHTNESS_PLUS}, 307),
          _status("BRIGHTNESS 224 / 255", 308),
          _back("< BACK", {runCommand, this, CMD_BACK}, 309) {}

    void attach(UiNavigator& navigator, UiFocusManager& focus) {
        _navigator = &navigator;
        _focus = &focus;
    }

    UiStatus onCreate() override {
        _root.addChild(_title);
        _root.addChild(_dark);
        _root.addChild(_animations);
        _root.addChild(_brightnessLabel);
        _root.addChild(_minus);
        _root.addChild(_brightness);
        _root.addChild(_plus);
        _root.addChild(_status);
        _root.addChild(_back);

        _title.setTextSize(2);
        _title.setColor(kCyan);
        _title.setBounds({110, 40, 205, 22});
        _dark.setBounds({kSafeX, 67, kSafeW, 46});
        _animations.setBounds({kSafeX, 121, kSafeW, 46});
        _brightnessLabel.setTextSize(1);
        _brightnessLabel.setBounds({kSafeX, 181, kSafeW, 16});
        _minus.setBounds({kSafeX, 207, 58, 42});
        _brightness.setBounds({122, 207, 168, 42});
        _plus.setBounds({298, 207, 58, 42});
        _status.setTextSize(1);
        _status.setBounds({118, 265, 210, 16});
        _back.setBounds({kSafeX, 298, kSafeW, 48});
        updateStatus();
        return UiStatus::Ok;
    }

    bool onEvent(UiEvent& event) override {
        return moveFocusWithWheel(event, _focus, _root);
    }

    UiWidget& root() override { return _root; }

private:
    static void runCommand(void* context, uint16_t command) {
        static_cast<SettingsScreen*>(context)->applyCommand(command);
    }

    void applyCommand(uint16_t command) {
        if (command == CMD_BACK) {
            if (_navigator) (void)_navigator->pop();
            return;
        }
        if (command == CMD_DARK_THEME) {
            _state.dark = _dark.checked();
            applyWatcherTheme(_theme, _state.dark);
            _root.invalidate(_root.bounds());
        } else if (command == CMD_ANIMATIONS) {
            _state.animations = _animations.checked();
        } else if (command == CMD_BRIGHTNESS_MINUS) {
            const int value = static_cast<int>(_state.brightness) - 32;
            _state.brightness = static_cast<uint8_t>(value < 32 ? 32 : value);
        } else if (command == CMD_BRIGHTNESS_PLUS) {
            const int value = static_cast<int>(_state.brightness) + 32;
            _state.brightness = static_cast<uint8_t>(value > 255 ? 255 : value);
        }

        _brightness.setValue(_state.brightness);
        if (display.hasPanel()) display.panel().setBacklight(_state.brightness);
        updateStatus();
    }

    void updateStatus() {
        snprintf(_statusText, sizeof(_statusText), "BRIGHTNESS %u / 255",
                 static_cast<unsigned>(_state.brightness));
        _status.setText(_statusText);
    }

    AppState& _state;
    UiTheme& _theme;
    UiNavigator* _navigator = nullptr;
    UiFocusManager* _focus = nullptr;
    ThemedContainer _root;
    UiLabel _title;
    UiToggle _dark, _animations;
    UiLabel _brightnessLabel;
    UiButton _minus;
    UiProgressBar _brightness;
    UiButton _plus;
    UiLabel _status;
    UiButton _back;
    char _statusText[32] = "BRIGHTNESS 224 / 255";
};

class InfoScreen : public UiScreen {
public:
    InfoScreen()
        : _title("DEVICE INFORMATION", 401),
          _product("PRODUCT  SenseCAP Watcher", 402),
          _mcu("MCU      ESP32-S3", 403),
          _display("DISPLAY  412x412 RGB565", 404),
          _flash("FLASH    -- MB", 405),
          _psram("PSRAM    -- MB", 406),
          _heap("FREE     -- KB", 407),
          _library("LIBRARY  Seeed_GFX + Seeed_UI", 408),
          _back("< BACK", {runCommand, this, CMD_BACK}, 409) {}

    void attach(UiNavigator& navigator, UiFocusManager& focus) {
        _navigator = &navigator;
        _focus = &focus;
    }

    UiStatus onCreate() override {
        _root.addChild(_title);
        UiLabel* lines[] = {
            &_product, &_mcu, &_display, &_flash, &_psram, &_heap, &_library
        };
        for (uint8_t i = 0; i < 7; ++i) _root.addChild(*lines[i]);
        _root.addChild(_back);

        _title.setTextSize(2);
        _title.setColor(kCyan);
        _title.setBounds({98, 40, 225, 22});
        for (uint8_t i = 0; i < 7; ++i) {
            lines[i]->setTextSize(1);
            lines[i]->setBounds({73, static_cast<int16_t>(70 + i * 27),
                                 285, 16});
        }
        _back.setBounds({kSafeX, 288, kSafeW, 48});
        return UiStatus::Ok;
    }

    void onEnter(const void*) override {
        snprintf(_flashText, sizeof(_flashText), "FLASH    %lu MB",
                 static_cast<unsigned long>(ESP.getFlashChipSize() /
                                            (1024U * 1024U)));
        snprintf(_psramText, sizeof(_psramText), "PSRAM    %lu / %lu MB FREE",
                 static_cast<unsigned long>(ESP.getFreePsram() /
                                            (1024U * 1024U)),
                 static_cast<unsigned long>(ESP.getPsramSize() /
                                            (1024U * 1024U)));
        snprintf(_heapText, sizeof(_heapText), "FREE     %lu KB HEAP",
                 static_cast<unsigned long>(ESP.getFreeHeap() / 1024U));
        _flash.setText(_flashText);
        _psram.setText(_psramText);
        _heap.setText(_heapText);
    }

    bool onEvent(UiEvent& event) override {
        return moveFocusWithWheel(event, _focus, _root);
    }

    UiWidget& root() override { return _root; }

private:
    static void runCommand(void* context, uint16_t) {
        auto* self = static_cast<InfoScreen*>(context);
        if (self->_navigator) (void)self->_navigator->pop();
    }

    UiNavigator* _navigator = nullptr;
    UiFocusManager* _focus = nullptr;
    ThemedContainer _root;
    UiLabel _title, _product, _mcu, _display, _flash, _psram, _heap, _library;
    UiButton _back;
    char _flashText[24] = "FLASH    -- MB";
    char _psramText[36] = "PSRAM    -- MB";
    char _heapText[28] = "FREE     -- KB";
};

class MainMenuScreen : public UiScreen {
public:
    MainMenuScreen(DashboardScreen& dashboard, ControlsScreen& controls,
                   SettingsScreen& settings, InfoScreen& info)
        : _dashboard(dashboard), _controls(controls),
          _settings(settings), _info(info),
          _title("WATCHER CONTROL CENTER", 501),
          _subtitle("SELECT A MODULE", 502),
          _menu(_items, 4, 503),
          _hint("ROTATE + PRESS  OR  TOUCH", 504) {
        _items[0] = {510, "SYSTEM DASHBOARD", nullptr,
                     {openPage, this, CMD_OPEN_DASHBOARD}, true, true};
        _items[1] = {511, "INTERACTIVE CONTROL", nullptr,
                     {openPage, this, CMD_OPEN_CONTROLS}, true, true};
        _items[2] = {512, "DISPLAY SETTINGS", nullptr,
                     {openPage, this, CMD_OPEN_SETTINGS}, true, true};
        _items[3] = {513, "DEVICE INFORMATION", nullptr,
                     {openPage, this, CMD_OPEN_INFO}, true, true};
    }

    void attach(UiNavigator& navigator) { _navigator = &navigator; }

    UiStatus onCreate() override {
        _root.addChild(_title);
        _root.addChild(_subtitle);
        _root.addChild(_menu);
        _root.addChild(_hint);

        _title.setTextSize(2);
        _title.setColor(kCyan);
        _title.setBounds({74, 50, 270, 22});
        _subtitle.setTextSize(1);
        _subtitle.setColor(kMutedBlue);
        _subtitle.setBounds({139, 79, 180, 16});
        _menu.setRowHeight(55);
        _menu.setWrapNavigation(true);
        _menu.setBounds({kSafeX, 105, kSafeW, 220});
        _hint.setTextSize(1);
        _hint.setBounds({105, 345, 245, 16});
        return UiStatus::Ok;
    }

    UiWidget& root() override { return _root; }

private:
    static void openPage(void* context, uint16_t command) {
        auto* self = static_cast<MainMenuScreen*>(context);
        if (!self->_navigator) return;
        if (command == CMD_OPEN_DASHBOARD)
            (void)self->_navigator->push(self->_dashboard);
        else if (command == CMD_OPEN_CONTROLS)
            (void)self->_navigator->push(self->_controls);
        else if (command == CMD_OPEN_SETTINGS)
            (void)self->_navigator->push(self->_settings);
        else if (command == CMD_OPEN_INFO)
            (void)self->_navigator->push(self->_info);
    }

    DashboardScreen& _dashboard;
    ControlsScreen& _controls;
    SettingsScreen& _settings;
    InfoScreen& _info;
    UiNavigator* _navigator = nullptr;
    ThemedContainer _root;
    UiLabel _title, _subtitle;
    UiMenuItem _items[4];
    UiMenu _menu;
    UiLabel _hint;
};

class SplashScreen : public UiScreen {
public:
    explicit SplashScreen(MainMenuScreen& menu)
        : _menu(menu), _mark(601),
          _title("SenseCAP Watcher", 602),
          _subtitle("TOUCH + WHEEL UI", 603),
          _enter("ENTER CONTROL CENTER",
                 {runCommand, this, CMD_ENTER}, 604),
          _hint("Seeed_GFX2 DISPLAY FRAMEWORK", 605) {}

    void attach(UiNavigator& navigator) { _navigator = &navigator; }

    UiStatus onCreate() override {
        _root.addChild(_mark);
        _root.addChild(_title);
        _root.addChild(_subtitle);
        _root.addChild(_enter);
        _root.addChild(_hint);

        _mark.setBounds({156, 42, 100, 100});
        _title.setTextSize(3);
        _title.setColor(kCyan);
        _title.setBounds({72, 171, 290, 32});
        _subtitle.setTextSize(2);
        _subtitle.setColor(kGreen);
        _subtitle.setBounds({110, 218, 240, 22});
        _enter.setBounds({kSafeX, 271, kSafeW, 56});
        _hint.setTextSize(1);
        _hint.setBounds({108, 356, 230, 16});
        return UiStatus::Ok;
    }

    UiWidget& root() override { return _root; }

private:
    static void runCommand(void* context, uint16_t) {
        auto* self = static_cast<SplashScreen*>(context);
        // Replace keeps the welcome page out of the Back stack.
        if (self->_navigator) (void)self->_navigator->replace(self->_menu);
    }

    MainMenuScreen& _menu;
    UiNavigator* _navigator = nullptr;
    ThemedContainer _root;
    WatcherMark _mark;
    UiLabel _title, _subtitle;
    UiButton _enter;
    UiLabel _hint;
};

} // namespace

UiInputHub inputHub;
UiTheme theme = uiWioTerminalTheme();
TouchInput touchInput(display, 2, 600, 10);
SenseCAPWatcherInput watcherControls(display);
AppState appState;

DashboardScreen dashboardScreen(appState);
ControlsScreen controlsScreen(appState, inputHub);
SettingsScreen settingsScreen(appState, theme);
InfoScreen infoScreen;
MainMenuScreen mainScreen(
    dashboardScreen, controlsScreen, settingsScreen, infoScreen);
SplashScreen splashScreen(mainScreen);
UiApplication ui(display, inputHub, theme);

bool uiReady = false;

void setup() {
    Serial.begin(115200);
    applyWatcherTheme(theme, appState.dark);

    const uint32_t startedAt = millis();
    if (!display.begin()) {
        Serial.print("Watcher display init failed: ");
        Serial.println(display.lastResult().message);
        return;
    }
    display.panel().setBacklight(appState.brightness);

    if (!uiOk(inputHub.add(touchInput))) {
        Serial.println("Watcher touch registration failed");
        return;
    }
    if (!uiOk(inputHub.add(watcherControls))) {
        Serial.println("Watcher wheel registration failed");
        return;
    }
    installSenseCAPWatcherDefaultActionMap(inputHub.actionMap());

    splashScreen.attach(ui.navigator());
    mainScreen.attach(ui.navigator());
    dashboardScreen.attach(ui.navigator(), ui.focus());
    controlsScreen.attach(ui.navigator(), ui.focus());
    settingsScreen.attach(ui.navigator(), ui.focus());
    infoScreen.attach(ui.navigator(), ui.focus());

    if (!uiOk(ui.begin(splashScreen))) {
        Serial.println("Watcher dashboard UI initialization failed");
        return;
    }
    if (!uiOk(ui.tick(millis()))) {
        Serial.println("Watcher dashboard first frame failed");
        return;
    }

    uiReady = true;
    Serial.print("Watcher dashboard ready, ms: ");
    Serial.println(millis() - startedAt);
}

void loop() {
    if (!uiReady) {
        delay(100);
        return;
    }

    // 100 Hz input polling keeps touch and the wheel responsive. Rendering
    // still happens only for dirty widgets and the dashboard's 1 Hz update.
    static uint32_t lastTickMs = 0;
    const uint32_t now = millis();
    if (now - lastTickMs < 10U) return;
    lastTickMs = now;
    (void)ui.tick(now);
}
