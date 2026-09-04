/**
 * Product: SenseCAP Indicator (GX / DX), 480x480 touch RGB LCD
 * Demo:   multi-page touch UI built on Seeed_GFX + Seeed_UI
 *
 *   Splash    tap "Tap to Enter"             -> MainMenu
 *   MainMenu  tap an item                    -> Dashboard / Settings / Info
 *   Dashboard live uptime clock + animated CPU / Memory bars
 *   Settings  Dark style toggle (live recolor), Animations toggle,
 *             brightness - / + stepping the real backlight, Back
 *   Info      board / display / library labels, Back
 *
 * Touch only — the Indicator has no physical buttons. No ActionMap is
 * required: UiButton, UiToggle and UiMenu all handle PointerDown/PointerUp
 * directly and invoke their UiCommand on release, and UiApplication
 * dispatches touch events by hit-testing the widget under the finger.
 * On-screen Back buttons call navigator->pop().
 *
 * If your unit has the RGB-only DX panel, change the product ID below to
 * SenseCAP_Indicator_DX (touch address 0x38 instead of 0x48).
 */

#include <Seeed_GFX.h>
#include <Seeed_UI.h>
#include <stdio.h>

// Select SenseCAP_Indicator_DX for the RGB-only DX panel.
Seeed_GFX display(Seeed_Product::SenseCAP_Indicator_GX);

// Set to 1 to print raw touch coordinates to Serial whenever the panel is
// pressed. Use it to tell apart "touch coords are wrong/rotated" from "touch
// scan is starved by redraws". When enabled it adds one extra I2C read per
// loop iteration, so leave 0 for normal use.
#define DEBUG_TOUCH 0

namespace {

constexpr int16_t kScreenW = 480;
constexpr int16_t kScreenH = 480;

constexpr uint16_t kTitleColor = 0x3D7A; // subdued blue-green
constexpr uint16_t kHintColor = 0x2C92;

// Dashboard refresh is now driven only by the uptime clock, which changes once
// per second. Progress bars stay static so the single PSRAM framebuffer with
// no VSYNC does not tear during continuous animation.
constexpr uint32_t kClockIntervalMs = 1000;

// Small RGB565 logo (96x96) generated once for the splash. Demonstrates
// the Indicator's full-color RGB framebuffer via UiImage.
constexpr int16_t kLogoW = 96;
constexpr int16_t kLogoH = 96;
uint16_t logoFrame[kLogoW * kLogoH];

void buildLogo() {
    for (int16_t y = 0; y < kLogoH; ++y) {
        for (int16_t x = 0; x < kLogoW; ++x) {
            const int16_t dx = x - 48, dy = y - 48;
            const int32_t r2 = static_cast<int32_t>(dx) * dx +
                               static_cast<int32_t>(dy) * dy;
            uint16_t color = 0x0000; // transparent-ish black background
            if (r2 < 12 * 12) color = 0xFFFF;       // white core
            else if (r2 < 24 * 24) color = 0x07FF;  // cyan ring
            else if (r2 < 34 * 34) color = 0xF800;  // red ring
            else if (r2 < 44 * 44) color = 0xFFE0;  // yellow ring
            else if (r2 < 48 * 48) color = 0x07E0;  // green edge
            logoFrame[y * kLogoW + x] = color;
        }
    }
}

// Re-reads the palette on every render so the Dark style switch recolors
// every screen instantly, without rebuilding the widget tree.
class ThemedContainer : public UiContainer {
public:
    explicit ThemedContainer(UiId id = UI_ID_NONE) : UiContainer(id) {}

    void render(UiCanvas& canvas, const UiTheme& theme) override {
        if (!visible()) return;
        canvas.fillRect(bounds(), theme.colors.background);
        for (UiWidget* child = firstChild(); child; child = child->nextSibling())
            if (child->visible()) child->render(canvas, theme);
    }
};

void applyColorTheme(UiTheme& theme, bool dark) {
    if (dark) {
        theme.colors.background = 0x0000;
        theme.colors.surface = 0x1082;
        theme.colors.textPrimary = 0xFFFF;
        theme.colors.textSecondary = 0xBDF7;
        theme.colors.accent = 0x07FF;
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
}

enum : uint16_t {
    CMD_ENTER_MENU = 1,
    CMD_DASHBOARD,
    CMD_SETTINGS,
    CMD_INFO,
    CMD_BACK,
    CMD_SETTING_DARK = 20,
    CMD_SETTING_ANIM,
    CMD_BRIGHT_MINUS,
    CMD_BRIGHT_PLUS
};

// ---------- Dashboard: live clock + static demo progress bars ----------

class DashboardScreen : public UiScreen {
public:
    DashboardScreen()
        : _title("Dashboard", 101), _clock("00:00:00", 102),
          _cpuLabel("CPU activity", 103), _cpu(35, 0, 100, 104),
          _memLabel("Memory", 105), _mem(50, 0, 100, 106),
          _runtime("Uptime: 0 s", 107),
          _back("Back", {onBack, this, CMD_BACK}, 108) {}

    void attachNavigator(UiNavigator& navigator) { _navigator = &navigator; }

    UiStatus onCreate() override {
        _root.clearBackground();
        _root.addChild(_title);
        _root.addChild(_clock);
        _root.addChild(_cpuLabel);
        _root.addChild(_cpu);
        _root.addChild(_memLabel);
        _root.addChild(_mem);
        _root.addChild(_runtime);
        _root.addChild(_back);

        _title.setTextSize(2);
        _title.setColor(kTitleColor);
        _title.setBounds({16, 8, kScreenW - 32, 30});
        _clock.setTextSize(3);
        _clock.setColor(kHintColor);
        _clock.setBounds({16, 50, kScreenW - 32, 44});
        _cpuLabel.setBounds({16, 104, kScreenW - 32, 22});
        _cpu.setBounds({16, 130, kScreenW - 32, 26});
        _memLabel.setBounds({16, 168, kScreenW - 32, 22});
        _mem.setBounds({16, 194, kScreenW - 32, 26});
        _runtime.setBounds({16, 232, kScreenW - 32, 22});
        _back.setBounds({16, kScreenH - 58, kScreenW - 32, 50});
        return UiStatus::Ok;
    }

    void update(uint32_t nowMs) override {
        const unsigned long s = nowMs / 1000U;
        const unsigned long m = s / 60U;
        if (m == _lastMinute) return;
        _lastMinute = m;

        snprintf(_clockText, sizeof(_clockText), "%02lu:%02lu",
                 (s / 3600U) % 24U, (s / 60U) % 60U);
        _clock.setText(_clockText);
        snprintf(_runtimeText, sizeof(_runtimeText), "Uptime: %lu s", s);
        _runtime.setText(_runtimeText);
    }

    void setAnimationsEnabled(bool enabled) {
        _animationsEnabled = enabled;
        // Progress bars are intentionally static: no continuous animation, so
        // the single PSRAM framebuffer without VSYNC never tears. The toggle
        // only switches between two demo values.
        _cpu.setValue(_animationsEnabled ? 35 : 42);
        _mem.setValue(_animationsEnabled ? 50 : 58);
    }

    UiWidget& root() override { return _root; }

private:
    static void onBack(void* context, uint16_t) {
        auto* self = static_cast<DashboardScreen*>(context);
        if (self->_navigator) (void)self->_navigator->pop();
    }

    ThemedContainer _root;
    UiLabel _title, _clock, _cpuLabel, _memLabel, _runtime;
    UiProgressBar _cpu, _mem;
    UiButton _back;
    UiNavigator* _navigator = nullptr;
    uint32_t _lastMinute = 0xFFFFFFFF;
    bool _animationsEnabled = false;
    char _clockText[12] = "00:00";
    char _runtimeText[24] = "Uptime: 0 s";
};

// ---------- Settings: theme / animation toggles + brightness stepper ----------

class SettingsScreen : public UiScreen {
public:
    SettingsScreen(UiTheme& theme, DashboardScreen& dashboard)
        : _title("Settings", 201),
          _darkStyle("Dark style", true, {onSetting, this, CMD_SETTING_DARK}, 202),
          _animation("Animations", true, {onSetting, this, CMD_SETTING_ANIM}, 203),
          _brightLabel("Backlight brightness", 204),
          _minus("-", {onBrightness, this, CMD_BRIGHT_MINUS}, 205),
          _brightBar(128, 0, 255, 206),
          _plus("+", {onBrightness, this, CMD_BRIGHT_PLUS}, 207),
          _status("Dark / Anim ON / BL 128", 208),
          _back("Back", {onBack, this, CMD_BACK}, 209),
          _theme(theme), _dashboard(dashboard) {}

    void attachNavigator(UiNavigator& navigator) { _navigator = &navigator; }

    UiStatus onCreate() override {
        _root.clearBackground();
        _root.addChild(_title);
        _root.addChild(_darkStyle);
        _root.addChild(_animation);
        _root.addChild(_brightLabel);
        _root.addChild(_minus);
        _root.addChild(_brightBar);
        _root.addChild(_plus);
        _root.addChild(_status);
        _root.addChild(_back);

        _title.setTextSize(2);
        _title.setColor(kTitleColor);
        _title.setBounds({16, 8, kScreenW - 32, 30});
        _darkStyle.setBounds({16, 50, kScreenW - 32, 56});
        _animation.setBounds({16, 114, kScreenW - 32, 56});
        _brightLabel.setBounds({16, 182, kScreenW - 32, 22});
        _minus.setBounds({16, 212, 64, 36});
        _brightBar.setBounds({88, 212, 304, 36});
        _plus.setBounds({400, 212, 64, 36});
        _status.setTextSize(1);
        _status.setBounds({16, 258, kScreenW - 32, 22});
        _back.setBounds({16, kScreenH - 58, kScreenW - 32, 50});
        applySettings();
        return UiStatus::Ok;
    }

    UiWidget& root() override { return _root; }

private:
    static void onSetting(void* context, uint16_t) {
        static_cast<SettingsScreen*>(context)->applySettings();
    }
    static void onBrightness(void* context, uint16_t cmd) {
        auto* self = static_cast<SettingsScreen*>(context);
        self->adjustBrightness(cmd == CMD_BRIGHT_PLUS ? 16 : -16);
    }
    static void onBack(void* context, uint16_t) {
        auto* self = static_cast<SettingsScreen*>(context);
        if (self->_navigator) (void)self->_navigator->pop();
    }

    void applySettings() {
        applyColorTheme(_theme, _darkStyle.checked());
        _dashboard.setAnimationsEnabled(_animation.checked());
        if (display.hasPanel()) display.panel().setBacklight(_brightness);
        updateStatus();
        _root.invalidate(_root.bounds());
    }

    void adjustBrightness(int delta) {
        int v = static_cast<int>(_brightness) + delta;
        if (v < 0) v = 0;
        if (v > 255) v = 255;
        _brightness = static_cast<uint8_t>(v);
        _brightBar.setValue(_brightness);
        if (display.hasPanel()) display.panel().setBacklight(_brightness);
        updateStatus();
    }

    void updateStatus() {
        snprintf(_statusText, sizeof(_statusText), "%s / Anim %s / BL %u",
                 _darkStyle.checked() ? "Dark" : "Light",
                 _animation.checked() ? "ON" : "OFF",
                 static_cast<unsigned>(_brightness));
        _status.setText(_statusText);
    }

    ThemedContainer _root;
    UiLabel _title;
    UiToggle _darkStyle;
    UiToggle _animation;
    UiLabel _brightLabel;
    UiButton _minus, _plus;
    UiProgressBar _brightBar;
    UiLabel _status;
    UiButton _back;
    UiTheme& _theme;
    DashboardScreen& _dashboard;
    UiNavigator* _navigator = nullptr;
    uint8_t _brightness = 128;
    char _statusText[40] = "Dark / Anim ON / BL 128";
};

// ---------- Info: static system labels ----------

class InfoScreen : public UiScreen {
public:
    InfoScreen()
        : _title("System Info", 301),
          _line1("Board: SenseCAP Indicator", 302),
          _line2("MCU: ESP32-S3 + RP2040", 303),
          _line3("Display: 480x480 RGB565", 304),
          _line4("Panel: GX ST7701S (touch 0x48)", 305),
          _line5("Library: Seeed_GFX + Seeed_UI", 306),
          _back("Back", {onBack, this, CMD_BACK}, 307) {}

    void attachNavigator(UiNavigator& navigator) { _navigator = &navigator; }

    UiStatus onCreate() override {
        _root.clearBackground();
        _root.addChild(_title);
        UiLabel* lines[] = {&_line1, &_line2, &_line3, &_line4, &_line5};
        for (uint8_t i = 0; i < 5; ++i) _root.addChild(*lines[i]);
        _root.addChild(_back);

        _title.setTextSize(2);
        _title.setColor(kTitleColor);
        _title.setBounds({16, 8, kScreenW - 32, 30});
        for (uint8_t i = 0; i < 5; ++i) {
            lines[i]->setTextSize(1);
            lines[i]->setBounds({22, static_cast<int16_t>(58 + i * 30),
                                 kScreenW - 44, 22});
        }
        _back.setBounds({16, kScreenH - 58, kScreenW - 32, 50});
        return UiStatus::Ok;
    }

    UiWidget& root() override { return _root; }

private:
    static void onBack(void* context, uint16_t) {
        auto* self = static_cast<InfoScreen*>(context);
        if (self->_navigator) (void)self->_navigator->pop();
    }

    ThemedContainer _root;
    UiLabel _title, _line1, _line2, _line3, _line4, _line5;
    UiButton _back;
    UiNavigator* _navigator = nullptr;
};

// ---------- Main menu: touch list ----------

class MainMenuScreen : public UiScreen {
public:
    MainMenuScreen(DashboardScreen& dashboard, SettingsScreen& settings,
                   InfoScreen& info)
        : _dashboard(dashboard), _settings(settings), _info(info),
          _title("Main Menu", 401), _menu(_items, 3, 402) {
        _items[0] = {410, "Dashboard",    nullptr, {openItem, this, CMD_DASHBOARD}, true, true};
        _items[1] = {411, "Settings",     nullptr, {openItem, this, CMD_SETTINGS},  true, true};
        _items[2] = {412, "System Info",  nullptr, {openItem, this, CMD_INFO},      true, true};
    }

    void attachNavigator(UiNavigator& navigator) { _navigator = &navigator; }

    UiStatus onCreate() override {
        _root.clearBackground();
        _root.addChild(_title);
        _root.addChild(_menu);
        _title.setTextSize(2);
        _title.setColor(kTitleColor);
        _title.setBounds({16, 8, kScreenW - 32, 30});
        _menu.setRowHeight(72); // large touch targets
        _menu.setBounds({16, 50, kScreenW - 32, 400});
        return UiStatus::Ok;
    }

    UiWidget& root() override { return _root; }

private:
    static void openItem(void* context, uint16_t command) {
        auto* self = static_cast<MainMenuScreen*>(context);
        if (!self->_navigator) return;
        if (command == CMD_DASHBOARD) self->_navigator->push(self->_dashboard);
        else if (command == CMD_SETTINGS) self->_navigator->push(self->_settings);
        else if (command == CMD_INFO) self->_navigator->push(self->_info);
    }

    DashboardScreen& _dashboard;
    SettingsScreen& _settings;
    InfoScreen& _info;
    UiNavigator* _navigator = nullptr;
    ThemedContainer _root;
    UiLabel _title;
    UiMenuItem _items[3];
    UiMenu _menu;
};

// ---------- Splash: logo + tap-to-enter ----------

class SplashScreen : public UiScreen {
public:
    explicit SplashScreen(MainMenuScreen& menu)
        : _menu(menu),
          _logo(logoFrame, kLogoW, kLogoH, 501),
          _title("SenseCAP Indicator", 502),
          _subtitle("Touch UI Demo", 503),
          _enter("Tap to Enter", {enterMenu, this, CMD_ENTER_MENU}, 504) {}

    void attachNavigator(UiNavigator& navigator) { _navigator = &navigator; }

    UiStatus onCreate() override {
        _root.clearBackground();
        _root.addChild(_logo);
        _root.addChild(_title);
        _root.addChild(_subtitle);
        _root.addChild(_enter);
        _logo.setBounds({(kScreenW - kLogoW) / 2, 40, kLogoW, kLogoH});
        _title.setTextSize(3);
        _title.setColor(kTitleColor);
        _title.setBounds({16, 160, kScreenW - 32, 40});
        _subtitle.setTextSize(2);
        _subtitle.setColor(kHintColor);
        _subtitle.setBounds({16, 210, kScreenW - 32, 28});
        _enter.setBounds({120, 320, kScreenW - 240, 70});
        return UiStatus::Ok;
    }

    void onEnter(const void*) override {
        _logo.setImage(logoFrame, kLogoW, kLogoH);
    }

    UiWidget& root() override { return _root; }

private:
    static void enterMenu(void* context, uint16_t) {
        auto* self = static_cast<SplashScreen*>(context);
        if (self->_navigator) (void)self->_navigator->push(self->_menu);
    }

    MainMenuScreen& _menu;
    UiNavigator* _navigator = nullptr;
    ThemedContainer _root;
    UiImage _logo;
    UiLabel _title, _subtitle;
    UiButton _enter;
};

} // namespace

// ---------- globals ----------

UiInputHub inputHub;
UiTheme theme = uiWioTerminalTheme();
TouchInput touchInput(display);

DashboardScreen dashboardScreen;
SettingsScreen settingsScreen(theme, dashboardScreen);
InfoScreen infoScreen;
MainMenuScreen mainScreen(dashboardScreen, settingsScreen, infoScreen);
SplashScreen splashScreen(mainScreen);
UiApplication ui(display, inputHub, theme);

bool uiReady = false;

void setup() {
    Serial.begin(115200);
#if defined(CORE_DEBUG_LEVEL) && CORE_DEBUG_LEVEL >= 5
    Serial.println(
        "WARNING: Set Tools > Core Debug Level to None; Verbose I2C logs "
        "make Indicator startup and touch polling much slower.");
#endif
    applyColorTheme(theme, true);
    buildLogo();

    const uint32_t initStartedMs = millis();
    if (!display.begin()) {
        Serial.print("Indicator display init failed: ");
        Serial.println(display.lastResult().message);
        return;
    }
    Serial.print("Indicator display init OK, ms: ");
    Serial.println(millis() - initStartedMs);

    // Slow the shared I2C bus so that polled touch reads are less likely to be
    // corrupted by long RGB transactions on the single-bus SenseCAP Indicator.
    // 100 kHz is plenty for a single FT6x36 touch point and noticeably more
    // reliable than the 400 kHz default on this shared bus.
    Wire.setClock(100000);

    // SenseCAP_Indicator_GX/DX applies the panel's fixed 180-degree touch
    // mounting correction automatically.  setTouchRotation() is only needed
    // as an explicit override for custom or mechanically revised hardware.

    if (!uiOk(inputHub.add(touchInput))) {
        Serial.println("Indicator touch input registration failed");
        return;
    }

    splashScreen.attachNavigator(ui.navigator());
    mainScreen.attachNavigator(ui.navigator());
    dashboardScreen.attachNavigator(ui.navigator());
    settingsScreen.attachNavigator(ui.navigator());
    infoScreen.attachNavigator(ui.navigator());

    if (!uiOk(ui.begin(splashScreen))) {
        Serial.println("Indicator UI initialization failed");
        return;
    }

    // Render and atomically publish the complete splash. Subsequent UI updates
    // use the same double-buffered path.
    if (!uiOk(ui.tick(millis()))) {
        Serial.println("Indicator first UI frame failed");
        return;
    }
    IBus& displayBus = display.panel().driver().bus();
    if (displayBus.lastError() != 0) {
        Serial.print("Indicator first frame publication failed: ");
        Serial.println(displayBus.lastErrorMessage());
        return;
    }
    uiReady = true;
    Serial.print("Indicator first visible frame ready, ms: ");
    Serial.println(millis() - initStartedMs);
}

void loop() {
    if (!uiReady) { delay(100); return; }

    // Cap input/update work to roughly the panel refresh rate. RGB frame
    // publication itself is synchronized by the double-buffered bus.
    static uint32_t lastTickMs = 0;
    const uint32_t now = millis();
    if (now - lastTickMs < 16) return;
    lastTickMs = now;

#if DEBUG_TOUCH
    int32_t tx = 0, ty = 0;
    if (display.getTouch(&tx, &ty)) {
        Serial.printf("[touch] x=%d y=%d\n", (int)tx, (int)ty);
    }
#endif
    (void)ui.tick(now);
}
