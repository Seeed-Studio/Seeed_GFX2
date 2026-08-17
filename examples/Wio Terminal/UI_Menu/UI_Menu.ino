/**
 * Product: Wio Terminal
 * Demo: multi-page retained-mode UI with a splash screen and RGB565 artwork
 *
 * Controls:
 *   UP / DOWN   Select an item
 *   RIGHT       Open / confirm
 *   CENTER      Open / confirm
 *   LEFT        Back
 *
 * The three top keys are intentionally not used by this example.
 */

#include <Seeed_GFX.h>
#include <Seeed_UI.h>
#include "board/boards/Wio_Terminal.h"
#include "driver/tft/Driver_ILI9341.h"
#include "panel/Panel_TFT.h"
#include "gallery_images.h"

#ifndef LED_BUILTIN
#define LED_BUILTIN 13
#endif

namespace {

constexpr uint16_t kGalleryWidth = kGalleryPhotoWidth;
constexpr uint16_t kGalleryHeight = kGalleryPhotoHeight;
constexpr uint16_t kFaceWidth = kGalleryWidth;
constexpr uint16_t kFaceHeight = kGalleryHeight;
constexpr uint8_t kGallerySceneCount = kGalleryPhotoCount;
constexpr uint16_t kTitleColor = 0x3D7A; // subdued blue-green
constexpr uint16_t kHintColor = 0x2C92;

// One shared SRAM framebuffer serves both the animated splash face and the
// gallery. The three photos remain in Flash as RGB565 and only the selected
// 224x132 image is copied into SRAM, preserving the example's RAM footprint.
uint16_t galleryFrame[kGalleryWidth * kGalleryHeight];

int16_t distanceFrom(int16_t value, int16_t center) {
    const int16_t distance = value - center;
    return distance < 0 ? -distance : distance;
}

// Inspired by Seeed's Wio Terminal Interactive Emoji Faces demo: large eyes
// dominate the face and are updated through one framebuffer so gaze/blink
// animation stays smooth. No SD card or BMP assets are required here.
void buildInteractiveFaceFrame(uint32_t elapsedMs) {
    int16_t eyeRadiusY = static_cast<int16_t>(3 + elapsedMs / 24U);
    if (eyeRadiusY > 29) eyeRadiusY = 29;
    const bool blink = (elapsedMs >= 1350U && elapsedMs < 1580U) ||
                       (elapsedMs >= 2300U && elapsedMs < 2470U);
    if (blink) eyeRadiusY = 3;

    int16_t gazeX = -10;
    int16_t gazeY = 0;
    if (elapsedMs >= 600U && elapsedMs < 1200U)
        gazeX = static_cast<int16_t>(-10 + (elapsedMs - 600U) / 30U);
    else if (elapsedMs >= 1200U && elapsedMs < 1850U) {
        gazeX = 10;
        gazeY = static_cast<int16_t>((elapsedMs - 1200U) / 80U - 4);
    } else if (elapsedMs >= 1850U && elapsedMs < 2550U) {
        gazeX = static_cast<int16_t>(10 - (elapsedMs - 1850U) / 35U);
        gazeY = 4;
    } else if (elapsedMs >= 2550U) {
        gazeX = 0;
        gazeY = 0;
    }

    for (int16_t y = 0; y < kFaceHeight; ++y) {
        for (int16_t x = 0; x < kFaceWidth; ++x) {
            uint16_t color = 0x0003;

            for (uint8_t eye = 0; eye < 2; ++eye) {
                const int16_t centerX = eye == 0 ? 63 : 161;
                const int16_t centerY = 47;
                const int16_t dx = x - centerX;
                const int16_t dy = y - centerY;
                const int16_t outerY = eyeRadiusY + 4;
                const int32_t outer = static_cast<int32_t>(dx) * dx * outerY * outerY +
                                      static_cast<int32_t>(dy) * dy * 43 * 43;
                const int32_t outerLimit = static_cast<int32_t>(43) * 43 *
                                           outerY * outerY;
                if (outer <= outerLimit) color = 0x294B;

                const int32_t inner = static_cast<int32_t>(dx) * dx * eyeRadiusY * eyeRadiusY +
                                      static_cast<int32_t>(dy) * dy * 39 * 39;
                const int32_t innerLimit = static_cast<int32_t>(39) * 39 *
                                           eyeRadiusY * eyeRadiusY;
                if (inner <= innerLimit) color = 0xCE79;

                if (!blink && eyeRadiusY > 15) {
                    const int16_t pupilX = x - (centerX + gazeX);
                    const int16_t pupilY = y - (centerY + gazeY);
                    const int32_t pupilDistance = static_cast<int32_t>(pupilX) * pupilX +
                                                  static_cast<int32_t>(pupilY) * pupilY;
                    if (pupilDistance <= 15 * 15) color = 0x2C94;
                    if (pupilDistance <= 9 * 9) color = 0x0842;
                    const int16_t highlightX = pupilX + 4;
                    const int16_t highlightY = pupilY + 5;
                    if (static_cast<int32_t>(highlightX) * highlightX +
                        static_cast<int32_t>(highlightY) * highlightY <= 3 * 3)
                        color = 0xFFFF;
                }
            }

            if (elapsedMs >= 700U) {
                const int16_t cheekY = y - 91;
                const int16_t leftCheek = x - 27;
                const int16_t rightCheek = x - 197;
                if ((static_cast<int32_t>(leftCheek) * leftCheek +
                     static_cast<int32_t>(cheekY) * cheekY <= 9 * 9) ||
                    (static_cast<int32_t>(rightCheek) * rightCheek +
                     static_cast<int32_t>(cheekY) * cheekY <= 9 * 9))
                    color = 0xA28D;
            }

            if (elapsedMs >= 850U && distanceFrom(x, 112) <= 43) {
                const int16_t mouthX = x - 112;
                const int16_t mouthY = 116 - (mouthX * mouthX) / 150;
                if (distanceFrom(y, mouthY) <= 2) color = 0x8A28;
            }

            galleryFrame[y * kFaceWidth + x] = color;
        }
    }
}

void loadGalleryPhoto(uint8_t scene) {
    if (scene >= kGallerySceneCount) scene = 0;
    const uint16_t* source = kGalleryPhotos[scene];
    const size_t pixelCount =
        static_cast<size_t>(kGalleryWidth) * kGalleryHeight;
    for (size_t i = 0; i < pixelCount; ++i) {
        galleryFrame[i] = pgm_read_word(source + i);
    }
}

// Unlike a normal UiContainer, this root reads the current palette on every
// render. That makes the Dark style switch affect every screen immediately.
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
        // Low-luminance palette for the Wio Terminal's bright IPS panel.
        theme.colors.background = 0x0003;
        theme.colors.surface = 0x1082;
        theme.colors.textPrimary = 0xBDF7;
        theme.colors.textSecondary = 0x7BEF;
        theme.colors.accent = 0x038F;
        theme.colors.focus = 0xA500;
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
    CMD_DASHBOARD = 1,
    CMD_GALLERY,
    CMD_SETTINGS,
    CMD_ABOUT,
    CMD_DISPLAY_SETTINGS = 20,
    CMD_CONTROLS,
    CMD_SYSTEM_INFO
};

class DashboardScreen : public UiScreen {
public:
    DashboardScreen()
        : _title("Dashboard", 101), _cpuLabel("CPU activity", 102),
          _cpu(35, 0, 100, 103), _storageLabel("Storage", 104),
          _storage(68, 0, 100, 105), _runtime("Running: 0 s", 106),
          _hint("LEFT: Back", 107) {}

    UiStatus onCreate() override {
        _root.clearBackground();
        _root.addChild(_title);
        _root.addChild(_cpuLabel);
        _root.addChild(_cpu);
        _root.addChild(_storageLabel);
        _root.addChild(_storage);
        _root.addChild(_runtime);
        _root.addChild(_hint);

        _title.setTextSize(2);
        _title.setColor(kTitleColor);
        _title.setBounds({12, 8, 296, 24});
        _cpuLabel.setBounds({16, 48, 288, 18});
        _cpu.setBounds({16, 72, 288, 20});
        _storageLabel.setBounds({16, 110, 288, 18});
        _storage.setBounds({16, 134, 288, 20});
        _runtime.setTextSize(1);
        _runtime.setBounds({16, 176, 288, 16});
        _hint.setTextSize(1);
        _hint.setColor(kHintColor);
        _hint.setBounds({16, 216, 288, 16});
        return UiStatus::Ok;
    }

    void update(uint32_t nowMs) override {
        if (static_cast<uint32_t>(nowMs - _lastUpdate) < 250U) return;
        _lastUpdate = nowMs;
        if (_animationsEnabled) {
            int32_t wave = static_cast<int32_t>((nowMs / 35U) % 200U);
            if (wave > 100) wave = 200 - wave;
            _cpu.setValue(wave);
            _storage.setValue(55 + wave / 3);
        }
        snprintf(_runtimeText, sizeof(_runtimeText), "Running: %lu s",
                 static_cast<unsigned long>(nowMs / 1000U));
        _runtime.setText(_runtimeText);
    }

    void setAnimationsEnabled(bool enabled) {
        _animationsEnabled = enabled;
        if (!enabled) {
            _cpu.setValue(42);
            _storage.setValue(68);
        }
        _root.invalidate(_root.bounds());
    }

    UiWidget& root() override { return _root; }

private:
    ThemedContainer _root;
    UiLabel _title;
    UiLabel _cpuLabel;
    UiProgressBar _cpu;
    UiLabel _storageLabel;
    UiProgressBar _storage;
    UiLabel _runtime;
    UiLabel _hint;
    uint32_t _lastUpdate = 0;
    bool _animationsEnabled = true;
    char _runtimeText[32] = "Running: 0 s";
};

class GalleryScreen : public UiScreen {
public:
    GalleryScreen()
        : _title("Photo Gallery", 201),
          _image(galleryFrame, kGalleryWidth, kGalleryHeight, 202),
          _caption("1/3 Sunset Beach", 203),
          _hint("UP/DOWN Image  CENTER Next  LEFT Back", 204) {}

    UiStatus onCreate() override {
        _root.clearBackground();
        _root.addChild(_title);
        _root.addChild(_image);
        _root.addChild(_caption);
        _root.addChild(_hint);
        _title.setTextSize(2);
        _title.setColor(kTitleColor);
        _title.setBounds({12, 8, 296, 24});
        _image.setBounds({48, 38, kGalleryWidth, kGalleryHeight});
        _caption.setTextSize(1);
        _caption.setBounds({56, 180, 240, 16});
        _hint.setTextSize(1);
        _hint.setColor(kHintColor);
        _hint.setBounds({8, 216, 304, 16});
        return UiStatus::Ok;
    }

    void onEnter(const void*) override {
        renderScene();
    }

    bool onEvent(UiEvent& event) override {
        if (event.type != UiEventType::Action) return false;
        if ((event.phase == UiActionPhase::Pressed ||
             event.phase == UiActionPhase::Repeat) &&
            (event.action == UiAction::NavigateUp ||
             event.action == UiAction::NavigateDown)) {
            if (event.action == UiAction::NavigateDown)
                _scene = static_cast<uint8_t>((_scene + 1) % kGallerySceneCount);
            else
                _scene = static_cast<uint8_t>((_scene + kGallerySceneCount - 1) %
                                              kGallerySceneCount);
            renderScene();
            return true;
        }
        if (event.action == UiAction::Activate &&
            event.phase == UiActionPhase::Released) {
            _scene = static_cast<uint8_t>((_scene + 1) % kGallerySceneCount);
            renderScene();
            return true;
        }
        return false;
    }

    UiWidget& root() override { return _root; }

private:
    void renderScene() {
        loadGalleryPhoto(_scene);
        _image.setImage(galleryFrame, kGalleryWidth, kGalleryHeight);
        updateCaption();
    }

    void updateCaption() {
        static const char* names[kGallerySceneCount] = {
            "Sunset Beach", "Forest Sunbeams", "Garden Bird"
        };
        snprintf(_captionText, sizeof(_captionText), "%u/%u %s",
                 static_cast<unsigned>(_scene + 1),
                 static_cast<unsigned>(kGallerySceneCount), names[_scene]);
        _caption.setText(_captionText);
    }

    ThemedContainer _root;
    UiLabel _title;
    UiImage _image;
    UiLabel _caption;
    UiLabel _hint;
    uint8_t _scene = 0;
    char _captionText[48] = "1/3 Sunset Beach";
};

class DisplaySettingsScreen : public UiScreen {
public:
    DisplaySettingsScreen(UiTheme& theme, DashboardScreen& dashboard)
        : _title("Display Options", 301),
          _darkStyle("Dark style", true, {handleSetting, this, 1}, 302),
          _animation("Animations", true, {handleSetting, this, 2}, 303),
          _statusLights("Status LED", false, {handleSetting, this, 3}, 304),
          _status("Applied: Dark / Animation ON / LED OFF", 305),
          _hint("UP/DOWN Select  RIGHT/CENTER Toggle  LEFT Back", 306),
          _theme(theme), _dashboard(dashboard) {}

    UiStatus onCreate() override {
        _root.clearBackground();
        _root.addChild(_title);
        _root.addChild(_darkStyle);
        _root.addChild(_animation);
        _root.addChild(_statusLights);
        _root.addChild(_status);
        _root.addChild(_hint);
        _title.setTextSize(2);
        _title.setColor(kTitleColor);
        _title.setBounds({12, 8, 296, 24});
        _darkStyle.setBounds({12, 48, 296, 40});
        _animation.setBounds({12, 92, 296, 40});
        _statusLights.setBounds({12, 136, 296, 40});
        _status.setTextSize(1);
        _status.setBounds({18, 188, 290, 16});
        _hint.setTextSize(1);
        _hint.setColor(kHintColor);
        _hint.setBounds({8, 216, 304, 16});
        pinMode(LED_BUILTIN, OUTPUT);
        applySettings();
        return UiStatus::Ok;
    }

    UiWidget& root() override { return _root; }

private:
    static void handleSetting(void* context, uint16_t) {
        static_cast<DisplaySettingsScreen*>(context)->applySettings();
    }

    void applySettings() {
        applyColorTheme(_theme, _darkStyle.checked());
        _dashboard.setAnimationsEnabled(_animation.checked());
        digitalWrite(LED_BUILTIN, _statusLights.checked() ? HIGH : LOW);
        snprintf(_statusText, sizeof(_statusText), "Applied: %s / Animation %s / LED %s",
                 _darkStyle.checked() ? "Dark" : "Light",
                 _animation.checked() ? "ON" : "OFF",
                 _statusLights.checked() ? "ON" : "OFF");
        _status.setText(_statusText);
        _root.invalidate(_root.bounds());
    }

    ThemedContainer _root;
    UiLabel _title;
    UiToggle _darkStyle;
    UiToggle _animation;
    UiToggle _statusLights;
    UiLabel _status;
    UiLabel _hint;
    UiTheme& _theme;
    DashboardScreen& _dashboard;
    char _statusText[56] = "Applied: Dark / Animation ON / LED OFF";
};

class ControlsScreen : public UiScreen {
public:
    ControlsScreen()
        : _title("Controls", 401), _line1("UP / DOWN   Select item", 402),
          _line2("RIGHT       Open item", 403),
          _line3("CENTER      Confirm", 404),
          _line4("LEFT        Go back", 405), _hint("LEFT: Back", 406) {}

    UiStatus onCreate() override {
        _root.clearBackground();
        _root.addChild(_title);
        _root.addChild(_line1);
        _root.addChild(_line2);
        _root.addChild(_line3);
        _root.addChild(_line4);
        _root.addChild(_hint);
        _title.setTextSize(2);
        _title.setColor(kTitleColor);
        _title.setBounds({12, 8, 296, 24});
        UiLabel* lines[] = {&_line1, &_line2, &_line3, &_line4};
        for (uint8_t i = 0; i < 4; ++i) {
            lines[i]->setTextSize(1);
            lines[i]->setBounds({28, static_cast<int16_t>(58 + i * 30), 270, 16});
        }
        _hint.setTextSize(1);
        _hint.setColor(kHintColor);
        _hint.setBounds({16, 216, 288, 16});
        return UiStatus::Ok;
    }

    UiWidget& root() override { return _root; }

private:
    ThemedContainer _root;
    UiLabel _title, _line1, _line2, _line3, _line4, _hint;
};

class SystemInfoScreen : public UiScreen {
public:
    SystemInfoScreen()
        : _title("System Info", 501), _board("Board: Wio Terminal", 502),
          _display("Display: ILI9341 320x240", 503),
          _bus("Bus: SPI3 / 24 MHz", 504),
          _library("Library: Seeed_GFX2 UI demo", 505), _hint("LEFT: Back", 506) {}

    UiStatus onCreate() override {
        _root.clearBackground();
        _root.addChild(_title);
        _root.addChild(_board);
        _root.addChild(_display);
        _root.addChild(_bus);
        _root.addChild(_library);
        _root.addChild(_hint);
        _title.setTextSize(2);
        _title.setColor(kTitleColor);
        _title.setBounds({12, 8, 296, 24});
        UiLabel* lines[] = {&_board, &_display, &_bus, &_library};
        for (uint8_t i = 0; i < 4; ++i) {
            lines[i]->setTextSize(1);
            lines[i]->setBounds({22, static_cast<int16_t>(58 + i * 28), 282, 16});
        }
        _hint.setTextSize(1);
        _hint.setColor(kHintColor);
        _hint.setBounds({16, 216, 288, 16});
        return UiStatus::Ok;
    }

    UiWidget& root() override { return _root; }

private:
    ThemedContainer _root;
    UiLabel _title, _board, _display, _bus, _library, _hint;
};

class AboutScreen : public UiScreen {
public:
    AboutScreen()
        : _title("About", 601), _name("Seeed_GFX + Seeed_UI", 602),
          _description("Multi-page Wio Terminal example", 603),
          _version("Demo version 2.0", 604), _hint("LEFT: Back", 605) {}

    UiStatus onCreate() override {
        _root.clearBackground();
        _root.addChild(_title);
        _root.addChild(_name);
        _root.addChild(_description);
        _root.addChild(_version);
        _root.addChild(_hint);
        _title.setTextSize(2);
        _title.setColor(kTitleColor);
        _title.setBounds({12, 8, 296, 24});
        _name.setBounds({28, 64, 280, 20});
        _description.setTextSize(1);
        _description.setBounds({28, 104, 280, 16});
        _version.setTextSize(1);
        _version.setBounds({28, 136, 280, 16});
        _hint.setTextSize(1);
        _hint.setColor(kHintColor);
        _hint.setBounds({16, 216, 288, 16});
        return UiStatus::Ok;
    }

    UiWidget& root() override { return _root; }

private:
    ThemedContainer _root;
    UiLabel _title, _name, _description, _version, _hint;
};

class SettingsMenuScreen : public UiScreen {
public:
    SettingsMenuScreen(DisplaySettingsScreen& display, ControlsScreen& controls,
                       SystemInfoScreen& system)
        : _display(display), _controls(controls), _system(system),
          _title("Settings", 701), _menu(_items, 3, 702),
          _hint("UP/DOWN Select  RIGHT/CENTER Open  LEFT Back", 703) {
        _items[0] = {710, "Display options", nullptr,
                     {openItem, this, CMD_DISPLAY_SETTINGS}, true, true};
        _items[1] = {711, "Controls", nullptr,
                     {openItem, this, CMD_CONTROLS}, true, true};
        _items[2] = {712, "System info", nullptr,
                     {openItem, this, CMD_SYSTEM_INFO}, true, true};
    }

    void attachNavigator(UiNavigator& navigator) { _navigator = &navigator; }

    UiStatus onCreate() override {
        _root.clearBackground();
        _root.addChild(_title);
        _root.addChild(_menu);
        _root.addChild(_hint);
        _title.setTextSize(2);
        _title.setColor(kTitleColor);
        _title.setBounds({12, 8, 296, 24});
        _menu.setBounds({8, 42, 304, 132});
        _hint.setTextSize(1);
        _hint.setColor(kHintColor);
        _hint.setBounds({8, 216, 304, 16});
        return UiStatus::Ok;
    }

    UiWidget& root() override { return _root; }

private:
    static void openItem(void* context, uint16_t command) {
        SettingsMenuScreen* self = static_cast<SettingsMenuScreen*>(context);
        if (!self->_navigator) return;
        if (command == CMD_DISPLAY_SETTINGS) self->_navigator->push(self->_display);
        else if (command == CMD_CONTROLS) self->_navigator->push(self->_controls);
        else if (command == CMD_SYSTEM_INFO) self->_navigator->push(self->_system);
    }

    DisplaySettingsScreen& _display;
    ControlsScreen& _controls;
    SystemInfoScreen& _system;
    UiNavigator* _navigator = nullptr;
    ThemedContainer _root;
    UiLabel _title;
    UiMenuItem _items[3];
    UiMenu _menu;
    UiLabel _hint;
};

class MainMenuScreen : public UiScreen {
public:
    MainMenuScreen(DashboardScreen& dashboard, GalleryScreen& gallery,
                   SettingsMenuScreen& settings, AboutScreen& about)
        : _dashboard(dashboard), _gallery(gallery), _settings(settings),
          _about(about), _title("Main Menu", 801), _menu(_items, 4, 802),
          _hint("UP/DOWN Select  RIGHT/CENTER Open  LEFT Back", 803) {
        _items[0] = {810, "Dashboard", nullptr,
                     {openItem, this, CMD_DASHBOARD}, true, true};
        _items[1] = {811, "Image gallery", nullptr,
                     {openItem, this, CMD_GALLERY}, true, true};
        _items[2] = {812, "Settings  >", nullptr,
                     {openItem, this, CMD_SETTINGS}, true, true};
        _items[3] = {813, "About", nullptr,
                     {openItem, this, CMD_ABOUT}, true, true};
    }

    void attachNavigator(UiNavigator& navigator) { _navigator = &navigator; }

    UiStatus onCreate() override {
        _root.clearBackground();
        _root.addChild(_title);
        _root.addChild(_menu);
        _root.addChild(_hint);
        _title.setTextSize(2);
        _title.setColor(kTitleColor);
        _title.setBounds({12, 8, 296, 24});
        _menu.setBounds({8, 40, 304, 160});
        _hint.setTextSize(1);
        _hint.setColor(kHintColor);
        _hint.setBounds({8, 216, 304, 16});
        return UiStatus::Ok;
    }

    UiWidget& root() override { return _root; }

private:
    static void openItem(void* context, uint16_t command) {
        MainMenuScreen* self = static_cast<MainMenuScreen*>(context);
        if (!self->_navigator) return;
        if (command == CMD_DASHBOARD) self->_navigator->push(self->_dashboard);
        else if (command == CMD_GALLERY) self->_navigator->push(self->_gallery);
        else if (command == CMD_SETTINGS) self->_navigator->push(self->_settings);
        else if (command == CMD_ABOUT) self->_navigator->push(self->_about);
    }

    DashboardScreen& _dashboard;
    GalleryScreen& _gallery;
    SettingsMenuScreen& _settings;
    AboutScreen& _about;
    UiNavigator* _navigator = nullptr;
    ThemedContainer _root;
    UiLabel _title;
    UiMenuItem _items[4];
    UiMenu _menu;
    UiLabel _hint;
};

class SplashScreen : public UiScreen {
public:
    explicit SplashScreen(MainMenuScreen& menu)
        : _menu(menu), _face(galleryFrame, kFaceWidth, kFaceHeight, 901),
          _hello("", 902), _subtitle("Seeed UI on Wio Terminal", 903),
          _hint("Press CENTER to enter", 904) {}

    void attachNavigator(UiNavigator& navigator) { _navigator = &navigator; }

    UiStatus onCreate() override {
        _root.clearBackground();
        _root.addChild(_face);
        _root.addChild(_hello);
        _root.addChild(_subtitle);
        _root.addChild(_hint);
        _face.setBounds({48, 6, kFaceWidth, kFaceHeight});
        _hello.setTextSize(2);
        _hello.setColor(kTitleColor);
        _hello.setBounds({82, 82, 190, 24});
        _subtitle.setTextSize(1);
        _subtitle.setBounds({80, 132, 200, 16});
        _hint.setTextSize(1);
        _hint.setColor(0xA500);
        _hint.setBounds({91, 188, 180, 16});
        _hello.setVisible(false);
        _subtitle.setVisible(false);
        _hint.setVisible(false);
        return UiStatus::Ok;
    }

    void onEnter(const void*) override {
        _startedAt = millis();
        _lastFrameAt = 0;
        _typedChars = 0;
        _ready = false;
        _helloText[0] = '\0';
        _hello.setText(_helloText);
        _face.setVisible(true);
        _hello.setVisible(false);
        _subtitle.setVisible(false);
        _hint.setVisible(false);
        buildInteractiveFaceFrame(0);
        _face.setImage(galleryFrame, kFaceWidth, kFaceHeight);
    }

    void update(uint32_t nowMs) override {
        const uint32_t elapsed = static_cast<uint32_t>(nowMs - _startedAt);
        if (elapsed < 3000U) {
            if (static_cast<uint32_t>(nowMs - _lastFrameAt) >= 50U) {
                _lastFrameAt = nowMs;
                buildInteractiveFaceFrame(elapsed);
                _face.setImage(galleryFrame, kFaceWidth, kFaceHeight);
            }
            return;
        }

        if (_face.visible()) {
            _face.setVisible(false);
            _hello.setVisible(true);
        }

        const char* message = "Hello World!";
        uint8_t chars = static_cast<uint8_t>((elapsed - 3000U) / 105U);
        const uint8_t length = static_cast<uint8_t>(strlen(message));
        if (chars > length) chars = length;
        if (chars != _typedChars) {
            _typedChars = chars;
            for (uint8_t i = 0; i < chars; ++i) _helloText[i] = message[i];
            _helloText[chars] = '\0';
            _hello.setText(_helloText);
        }
        if (elapsed >= 4450U) _subtitle.setVisible(true);
        if (elapsed >= 5000U) {
            _hint.setVisible(true);
            _ready = true;
        }
    }

    bool onEvent(UiEvent& event) override {
        if (event.type == UiEventType::Action &&
            event.action == UiAction::Activate &&
            event.phase == UiActionPhase::Released && _navigator && _ready) {
            _navigator->push(_menu);
            return true;
        }
        return false;
    }

    UiWidget& root() override { return _root; }

private:
    MainMenuScreen& _menu;
    UiNavigator* _navigator = nullptr;
    ThemedContainer _root;
    UiImage _face;
    UiLabel _hello, _subtitle, _hint;
    uint32_t _startedAt = 0;
    uint32_t _lastFrameAt = 0;
    uint8_t _typedChars = 0;
    bool _ready = false;
    char _helloText[16] = "";
};

WioTerminalInputConfig navigationInputConfig() {
    WioTerminalInputConfig config = wioDefaultInputConfig();
    // Keep directions + center (indices 0..4), disable the three top keys.
    for (uint8_t i = 5; i < 8; ++i) config.buttons[i].pin = -1;
    return config;
}

UiStatus installNavigationActionMap(UiActionMap& map) {
    UiStatus status = map.add({static_cast<uint16_t>(WioTerminalKey::Up),
                               UiAction::NavigateUp, true, false});
    if (!uiOk(status)) return status;
    status = map.add({static_cast<uint16_t>(WioTerminalKey::Down),
                      UiAction::NavigateDown, true, false});
    if (!uiOk(status)) return status;
    status = map.add({static_cast<uint16_t>(WioTerminalKey::Right),
                      UiAction::Activate, false, false});
    if (!uiOk(status)) return status;
    status = map.add({static_cast<uint16_t>(WioTerminalKey::Press),
                      UiAction::Activate, false, false});
    if (!uiOk(status)) return status;
    return map.add({static_cast<uint16_t>(WioTerminalKey::Left),
                    UiAction::Back, false, false});
}

} // namespace

Seeed_GFX display;
WioTerminalInput buttons(navigationInputConfig());
UiInputHub inputHub;
UiTheme theme = uiWioTerminalTheme();

DashboardScreen dashboardScreen;
GalleryScreen galleryScreen;
DisplaySettingsScreen displaySettingsScreen(theme, dashboardScreen);
ControlsScreen controlsScreen;
SystemInfoScreen systemInfoScreen;
AboutScreen aboutScreen;
SettingsMenuScreen settingsScreen(displaySettingsScreen, controlsScreen,
                                  systemInfoScreen);
MainMenuScreen mainScreen(dashboardScreen, galleryScreen, settingsScreen,
                          aboutScreen);
SplashScreen splashScreen(mainScreen);
UiApplication ui(display, inputHub, theme);

bool uiReady = false;
bool uiRuntimeErrorReported = false;

void setup() {
    Serial.begin(115200);
    applyColorTheme(theme, true);
    buildInteractiveFaceFrame(0);

    if (!display.begin<Board_Wio_Terminal,
                       Config_Wio_Terminal_ILI9341>()) {
        Serial.println("Wio Terminal display initialization failed");
        return;
    }
    display.setRotation(3);

    if (!uiOk(inputHub.add(buttons))) {
        Serial.println("Wio Terminal input registration failed");
        return;
    }
    if (!uiOk(installNavigationActionMap(inputHub.actionMap()))) {
        Serial.println("Wio Terminal navigation action map failed");
        return;
    }

    splashScreen.attachNavigator(ui.navigator());
    mainScreen.attachNavigator(ui.navigator());
    settingsScreen.attachNavigator(ui.navigator());

    if (!uiOk(ui.begin(splashScreen))) {
        Serial.println("Wio Terminal UI initialization failed");
        return;
    }
    uiReady = true;
}

void loop() {
    if (!uiReady) {
        delay(100);
        return;
    }

    const UiStatus status = ui.tick(millis());
    if (!uiOk(status) && !uiRuntimeErrorReported) {
        Serial.println("Wio Terminal UI render/input error");
        uiRuntimeErrorReported = true;
    } else if (uiOk(status)) {
        uiRuntimeErrorReported = false;
    }
}
