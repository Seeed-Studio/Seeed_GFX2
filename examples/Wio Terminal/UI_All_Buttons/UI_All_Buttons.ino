/**
 * Product: Wio Terminal
 * Demo: UI using the five-way switch and all three top keys
 *
 * Five-way switch:
 *   UP / DOWN   Move the selection
 *   LEFT/RIGHT  Change a value or gallery scene
 *   PRESS       Open / confirm / toggle
 *
 * Top keys:
 *   Left  (C)    Return home from anywhere
 *   Middle (B)   Open or close the quick menu
 *   Right (A)    Back
 */

#include <Seeed_GFX.h>
#include <Seeed_UI.h>
#include "board/boards/Wio_Terminal.h"
#include "driver/tft/Driver_ILI9341.h"
#include "panel/Panel_TFT.h"
#include <stdio.h>

namespace {

enum class DemoPage : uint8_t { Home = 0, Monitor, Gallery, Controls, About };

constexpr uint16_t kBlue = 0x34BF;
constexpr uint16_t kCyan = 0x4E7F;
constexpr uint16_t kGreen = 0x4E69;
constexpr uint16_t kYellow = 0xFDC7;
constexpr uint16_t kRed = 0xF24A;
constexpr uint16_t kWhite = 0xFFFF;
constexpr int16_t kGalleryBufferWidth = 280;
constexpr int16_t kGalleryBufferHeight = 128;

WioTerminalInputConfig stableInputConfig() {
    WioTerminalInputConfig config = wioDefaultInputConfig();
    // This demo deliberately advances once per physical press. Disabling the
    // scanner repeat also prevents a slow display redraw from turning one
    // ordinary press into several queued navigation events.
    config.timing.debounceMs = 40;
    config.timing.longPressMs = 800;
    config.timing.repeatDelayMs = 0;
    config.timing.repeatRateMs = 0;
    return config;
}

UiStatus installStableActionMap(UiActionMap& map) {
    const UiActionBinding bindings[] = {
        {static_cast<uint16_t>(WioTerminalKey::Up), UiAction::NavigateUp, false, false},
        {static_cast<uint16_t>(WioTerminalKey::Down), UiAction::NavigateDown, false, false},
        {static_cast<uint16_t>(WioTerminalKey::Left), UiAction::NavigateLeft, false, false},
        {static_cast<uint16_t>(WioTerminalKey::Right), UiAction::NavigateRight, false, false},
        {static_cast<uint16_t>(WioTerminalKey::Press), UiAction::Activate, false, false},
        {static_cast<uint16_t>(WioTerminalKey::A), UiAction::Back, false, false},
        {static_cast<uint16_t>(WioTerminalKey::B), UiAction::Menu, false, false},
        {static_cast<uint16_t>(WioTerminalKey::C), UiAction::Home, false, false}
    };
    for (size_t i = 0; i < sizeof(bindings) / sizeof(bindings[0]); ++i) {
        const UiStatus status = map.add(bindings[i]);
        if (!uiOk(status)) return status;
    }
    return UiStatus::Ok;
}

Seeed_GFX display;
WioTerminalInput buttons(stableInputConfig());
UiInputHub inputHub;
uint16_t galleryBuffer[kGalleryBufferWidth * kGalleryBufferHeight];

DemoPage page = DemoPage::Home;
uint8_t homeSelection = 0;
uint8_t quickSelection = 0;
uint8_t galleryScene = 0;
uint8_t monitorTarget = 64;
uint32_t actionCount = 0;
uint32_t galleryFrame = 0;
uint32_t lastAnimationAt = 0;
bool quickMenuOpen = false;
bool monitorRunning = true;
bool galleryPlaying = true;
bool darkStyle = true;
bool fullRedrawRequested = true;
bool contentRedrawRequested = false;
bool headerRedrawRequested = false;
bool uiReady = false;
const char* lastKey = "READY";

uint16_t backgroundColor() { return darkStyle ? 0x0861 : 0xE73C; }
uint16_t panelColor() { return darkStyle ? 0x18E3 : 0xFFFF; }
uint16_t selectedColor() { return darkStyle ? 0x2490 : 0xC65F; }
uint16_t textColor() { return darkStyle ? 0xEF7D : 0x10A2; }
uint16_t mutedColor() { return darkStyle ? 0x8C71 : 0x630C; }
uint16_t borderColor() { return darkStyle ? 0x3A8A : 0xA534; }

void text(const char* value, int16_t x, int16_t y, uint8_t datum = TL_DATUM,
          uint16_t color = kWhite, uint8_t font = 2) {
    display.setTextDatum(datum);
    display.setTextColor(color);
    display.drawString(value, x, y, font);
    display.setTextDatum(TL_DATUM);
}

const char* pageTitle() {
    switch (page) {
        case DemoPage::Monitor: return "SYSTEM MONITOR";
        case DemoPage::Gallery: return "MOTION GALLERY";
        case DemoPage::Controls: return "BUTTON TESTER";
        case DemoPage::About: return "ABOUT THIS DEMO";
        default: return "WIO CONTROL CENTER";
    }
}

void drawHeader() {
    display.fillRect(0, 0, 320, 38, darkStyle ? 0x10C5 : 0xB5DF);
    display.fillRect(0, 36, 320, 2, kCyan);
    text(pageTitle(), 12, 10, TL_DATUM, textColor(), 2);
    display.fillRoundRect(252, 8, 58, 21, 6, darkStyle ? 0x2948 : 0xFFFF);
    text(lastKey, 281, 18, MC_DATUM, darkStyle ? kYellow : 0xA2A0, 1);
}

void drawHeaderKey() {
    display.fillRoundRect(252, 8, 58, 21, 6, darkStyle ? 0x2948 : 0xFFFF);
    text(lastKey, 281, 18, MC_DATUM, darkStyle ? kYellow : 0xA2A0, 1);
}

void drawFooter() {
    display.fillRect(0, 213, 320, 27, darkStyle ? 0x0842 : 0xCE79);
    display.drawFastHLine(0, 213, 320, borderColor());
    text("5-WAY: MOVE/PRESS   C:HOME  B:MENU  A:BACK",
         160, 226, MC_DATUM, mutedColor(), 1);
}

void drawHome() {
    static const char* const titles[4] = {
        "System monitor", "Motion gallery", "Button tester", "About"
    };
    static const char* const details[4] = {
        "Live values and controls", "Animated vector scenes",
        "Verify all eight inputs", "Key map and demo info"
    };
    static const uint16_t colors[4] = {kGreen, kCyan, kYellow, kRed};

    for (uint8_t i = 0; i < 4; ++i) {
        const int16_t y = 46 + i * 40;
        const bool selected = i == homeSelection;
        display.fillRoundRect(12, y, 296, 34, 7,
                              selected ? selectedColor() : panelColor());
        display.drawRoundRect(12, y, 296, 34, 7,
                              selected ? colors[i] : borderColor());
        display.fillCircle(29, y + 17, 7, colors[i]);
        text(titles[i], 45, y + 5, TL_DATUM, textColor(), 2);
        text(details[i], 45, y + 22, TL_DATUM, mutedColor(), 1);
        if (selected) text(">", 294, y + 17, MC_DATUM, colors[i], 2);
    }
}

void drawGauge(int16_t x, int16_t y, int16_t width, uint8_t value,
               uint16_t color) {
    display.fillRoundRect(x, y, width, 14, 7, darkStyle ? 0x0862 : 0xB5B6);
    const int16_t fill = static_cast<int16_t>((width - 4) * value / 100);
    if (fill > 0) display.fillRoundRect(x + 2, y + 2, fill, 10, 5, color);
}

void drawMonitorBody() {
    char buffer[32];
    display.fillRoundRect(14, 48, 292, 151, 9, panelColor());
    text("SIMULATED WORKLOAD", 28, 61, TL_DATUM, mutedColor(), 1);
    snprintf(buffer, sizeof(buffer), "%u%%", monitorTarget);
    text(buffer, 286, 65, MR_DATUM, monitorTarget > 80 ? kRed : kGreen, 2);
    drawGauge(28, 85, 258, monitorTarget,
              monitorTarget > 80 ? kRed : monitorTarget > 55 ? kYellow : kGreen);

    text("PROCESS", 28, 119, TL_DATUM, mutedColor(), 1);
    display.fillRoundRect(104, 111, 86, 27, 7,
                          monitorRunning ? 0x1C87 : 0x4208);
    text(monitorRunning ? "RUNNING" : "PAUSED", 147, 124, MC_DATUM,
         monitorRunning ? kGreen : kRed, 2);

    snprintf(buffer, sizeof(buffer), "Events: %lu",
             static_cast<unsigned long>(actionCount));
    text(buffer, 28, 157, TL_DATUM, textColor(), 2);
    snprintf(buffer, sizeof(buffer), "Uptime: %lus",
             static_cast<unsigned long>(millis() / 1000UL));
    text(buffer, 174, 157, TL_DATUM, textColor(), 2);
    text("LEFT/RIGHT adjust   PRESS run/pause", 160, 188, MC_DATUM,
         mutedColor(), 1);
}

void drawMonitorUptime() {
    char buffer[24];
    display.fillRect(170, 151, 121, 24, panelColor());
    snprintf(buffer, sizeof(buffer), "Uptime: %lus",
             static_cast<unsigned long>(millis() / 1000UL));
    text(buffer, 174, 157, TL_DATUM, textColor(), 2);
}

void drawGalleryArt() {
    const uint16_t sky[4] = {0x118F, 0x40F7, 0x300D, 0x0330};
    const uint16_t accent[4] = {0xFE46, 0x4E7F, 0xF24A, 0xBDF7};
    const int16_t phase = static_cast<int16_t>(galleryFrame % 220U);
    const int16_t movingX = phase < 110 ? 30 + phase * 2 :
                                           250 - (phase - 110) * 2;

    // Build the complete artwork off-screen, then send it in one pushImage().
    // The previous primitive-by-primitive redraw exposed the cleared
    // background between shapes and looked like continuous flashing.
    for (int16_t y = 0; y < kGalleryBufferHeight; ++y) {
        for (int16_t x = 0; x < kGalleryBufferWidth; ++x) {
            uint16_t color = sky[galleryScene];
            if (galleryScene == 0) {
                color = y < 78 ? 0x118F : ((x / 12 + y / 6) & 1 ? 0x0351 : 0x04B4);
                const int16_t dx = x - movingX;
                const int16_t dy = y - 38;
                if (static_cast<int32_t>(dx) * dx + static_cast<int32_t>(dy) * dy <= 20 * 20)
                    color = kYellow;
                if (y > 102 && ((x + y + galleryFrame) % 29U) < 4U)
                    color = 0x7D76;
            } else if (galleryScene == 1) {
                color = y < 92 ? 0x40F7 : 0x2108;
                const uint8_t column = static_cast<uint8_t>(x / 35);
                const int16_t height = 25 +
                    static_cast<int16_t>((column * 17U + galleryFrame / 2U) % 68U);
                if ((x % 35) >= 7 && (x % 35) < 28 && y >= 118 - height)
                    color = column & 1 ? kCyan : kGreen;
            } else if (galleryScene == 2) {
                color = ((x / 18 + y / 14) & 1) ? 0x300D : 0x4010;
                for (uint8_t dot = 0; dot < 7; ++dot) {
                    const int16_t dx = x - (20 + dot * 40);
                    const int16_t centerY =
                        64 + (((dot + galleryFrame / 6U) & 1U) ? 27 : -27);
                    const int16_t dy = y - centerY;
                    const int32_t distance = static_cast<int32_t>(dx) * dx +
                                             static_cast<int32_t>(dy) * dy;
                    if (distance <= 18 * 18) color = dot & 1 ? kRed : kYellow;
                    if (distance <= 4 * 4) color = kWhite;
                }
            } else {
                color = ((x * 37 + y * 23) % 397) < 3 ? kWhite : 0x0330;
                const int16_t planetX = x - 140;
                const int16_t planetY = y - 64;
                if (static_cast<int32_t>(planetX) * planetX +
                    static_cast<int32_t>(planetY) * planetY <= 39 * 39)
                    color = 0x4208;
                const int16_t orbitX = movingX;
                const int16_t orbitY = 64 + (((galleryFrame / 8U) & 1U) ? 31 : -31);
                const int16_t moonX = x - orbitX;
                const int16_t moonY = y - orbitY;
                if (static_cast<int32_t>(moonX) * moonX +
                    static_cast<int32_t>(moonY) * moonY <= 9 * 9)
                    color = accent[galleryScene];
                const int16_t shineX = x - 129;
                const int16_t shineY = y - 53;
                if (static_cast<int32_t>(shineX) * shineX +
                    static_cast<int32_t>(shineY) * shineY <= 5 * 5)
                    color = kWhite;
            }
            galleryBuffer[y * kGalleryBufferWidth + x] = color;
        }
    }

    display.pushImage(20, 55, kGalleryBufferWidth, kGalleryBufferHeight,
                      galleryBuffer);

    char buffer[20];
    snprintf(buffer, sizeof(buffer), "SCENE %u / 4", galleryScene + 1);
    display.fillRoundRect(112, 54, 96, 22, 6, 0x0842);
    text(buffer, 160, 65, MC_DATUM, kWhite, 1);
}

void drawGalleryPage() {
    display.fillRoundRect(14, 48, 292, 142, 10, borderColor());
    drawGalleryArt();
    display.fillRect(70, 192, 180, 18, backgroundColor());
    text(galleryPlaying ? "PRESS: PAUSE" : "PRESS: PLAY", 160, 202,
         MC_DATUM, mutedColor(), 1);
}

void drawKeyCap(int16_t x, int16_t y, int16_t w, int16_t h,
                const char* label, uint16_t color) {
    display.fillRoundRect(x, y, w, h, 7, darkStyle ? 0x2146 : 0xFFFF);
    display.drawRoundRect(x, y, w, h, 7, color);
    text(label, x + w / 2, y + h / 2, MC_DATUM, color, 2);
}

void drawControls() {
    display.fillRoundRect(12, 48, 296, 151, 9, panelColor());
    // Facing the screen, Wio Terminal's three top keys are C, B, A.
    drawKeyCap(31, 57, 78, 34, "C  HOME", kGreen);
    drawKeyCap(121, 57, 78, 34, "B  MENU", kYellow);
    drawKeyCap(211, 57, 78, 34, "A  BACK", kRed);

    drawKeyCap(126, 101, 68, 27, "UP", kCyan);
    drawKeyCap(52, 132, 68, 27, "LEFT", kCyan);
    drawKeyCap(126, 132, 68, 27, "PRESS", kWhite);
    drawKeyCap(200, 132, 68, 27, "RIGHT", kCyan);
    drawKeyCap(126, 163, 68, 27, "DOWN", kCyan);
    text("LAST INPUT", 21, 205, ML_DATUM, mutedColor(), 1);
    text(lastKey, 299, 205, MR_DATUM, kYellow, 2);
}

void drawAbout() {
    display.fillRoundRect(14, 49, 292, 149, 10, panelColor());
    display.fillCircle(54, 91, 25, kBlue);
    text("W", 54, 91, MC_DATUM, kWhite, 4);
    text("Wio Terminal full input demo", 91, 67, TL_DATUM, textColor(), 2);
    text("Five-way switch: navigation", 91, 94, TL_DATUM, mutedColor(), 1);
    text("A / B / C: global shortcuts", 91, 111, TL_DATUM, mutedColor(), 1);
    text("No external button library required", 160, 147, MC_DATUM,
         textColor(), 2);
    text("B opens Quick Menu from every page", 160, 176, MC_DATUM,
         kCyan, 1);
}

void drawQuickMenu() {
    static const char* const items[3] = {
        "Toggle light/dark style", "Reset event counter", "Close quick menu"
    };
    display.fillRoundRect(45, 46, 230, 157, 12, darkStyle ? 0x10C4 : 0xFFFF);
    display.drawRoundRect(45, 46, 230, 157, 12, kYellow);
    text("QUICK MENU", 160, 65, MC_DATUM, kYellow, 2);
    for (uint8_t i = 0; i < 3; ++i) {
        const int16_t y = 86 + i * 34;
        display.fillRoundRect(58, y, 204, 27, 6,
                              i == quickSelection ? selectedColor() : panelColor());
        text(items[i], 70, y + 7, TL_DATUM,
             i == quickSelection ? textColor() : mutedColor(), 1);
    }
    text("UP/DOWN + PRESS    B/A: CLOSE", 160, 189, MC_DATUM,
         mutedColor(), 1);
}

void drawContent() {
    switch (page) {
        case DemoPage::Monitor: drawMonitorBody(); break;
        case DemoPage::Gallery: drawGalleryPage(); break;
        case DemoPage::Controls: drawControls(); break;
        case DemoPage::About: drawAbout(); break;
        default: drawHome(); break;
    }
}

void drawPage() {
    display.fillScreen(backgroundColor());
    drawHeader();
    drawContent();
    drawFooter();
    if (quickMenuOpen) drawQuickMenu();
    fullRedrawRequested = false;
    contentRedrawRequested = false;
    headerRedrawRequested = false;
}

void setLastKey(const char* name) {
    lastKey = name;
    ++actionCount;
    headerRedrawRequested = true;
    if (page == DemoPage::Controls) contentRedrawRequested = true;
}

void goHome() {
    page = DemoPage::Home;
    quickMenuOpen = false;
    fullRedrawRequested = true;
}

void activateHomeSelection() {
    page = static_cast<DemoPage>(homeSelection + 1);
    fullRedrawRequested = true;
}

void activateQuickSelection() {
    if (quickSelection == 0) {
        darkStyle = !darkStyle;
        fullRedrawRequested = true;
    } else if (quickSelection == 1) {
        actionCount = 0;
        contentRedrawRequested = true;
    } else {
        quickMenuOpen = false;
        fullRedrawRequested = true;
    }
}

void handleAction(const UiEvent& event) {
    // Navigation reacts only to the debounced KeyDown. Even if a custom input
    // source injects Repeat events, one physical press still moves one step.
    const bool move = event.phase == UiActionPhase::Pressed;
    const bool press = event.phase == UiActionPhase::Pressed;
    const bool release = event.phase == UiActionPhase::Released;

    if (event.action == UiAction::Home && press) {
        setLastKey("C HOME");
        goHome();
        return;
    }
    if (event.action == UiAction::Menu && press) {
        setLastKey("B MENU");
        quickMenuOpen = !quickMenuOpen;
        quickSelection = 0;
        fullRedrawRequested = true;
        return;
    }
    if (event.action == UiAction::Back && press) {
        setLastKey("A BACK");
        if (quickMenuOpen) quickMenuOpen = false;
        else if (page != DemoPage::Home) page = DemoPage::Home;
        fullRedrawRequested = true;
        return;
    }

    if (event.action == UiAction::NavigateUp && move) setLastKey("UP");
    else if (event.action == UiAction::NavigateDown && move) setLastKey("DOWN");
    else if (event.action == UiAction::NavigateLeft && move) setLastKey("LEFT");
    else if (event.action == UiAction::NavigateRight && move) setLastKey("RIGHT");
    else if (event.action == UiAction::Activate && release) setLastKey("PRESS");
    else return;

    if (quickMenuOpen) {
        if (event.action == UiAction::NavigateUp && move)
            quickSelection = quickSelection == 0 ? 2 : quickSelection - 1;
        else if (event.action == UiAction::NavigateDown && move)
            quickSelection = static_cast<uint8_t>((quickSelection + 1) % 3);
        else if (event.action == UiAction::Activate && release)
            activateQuickSelection();
        contentRedrawRequested = true;
        return;
    }

    if (page == DemoPage::Home) {
        if ((event.action == UiAction::NavigateUp ||
             event.action == UiAction::NavigateLeft) && move)
            homeSelection = homeSelection == 0 ? 3 : homeSelection - 1;
        else if ((event.action == UiAction::NavigateDown ||
                  event.action == UiAction::NavigateRight) && move)
            homeSelection = static_cast<uint8_t>((homeSelection + 1) % 4);
        else if (event.action == UiAction::Activate && release)
            activateHomeSelection();
    } else if (page == DemoPage::Monitor) {
        if (event.action == UiAction::NavigateLeft && move)
            monitorTarget = monitorTarget < 5 ? 0 : monitorTarget - 5;
        else if (event.action == UiAction::NavigateRight && move)
            monitorTarget = monitorTarget > 95 ? 100 : monitorTarget + 5;
        else if (event.action == UiAction::NavigateUp && move)
            monitorTarget = monitorTarget > 90 ? 100 : monitorTarget + 10;
        else if (event.action == UiAction::NavigateDown && move)
            monitorTarget = monitorTarget < 10 ? 0 : monitorTarget - 10;
        else if (event.action == UiAction::Activate && release)
            monitorRunning = !monitorRunning;
    } else if (page == DemoPage::Gallery) {
        if ((event.action == UiAction::NavigateLeft ||
             event.action == UiAction::NavigateUp) && move)
            galleryScene = galleryScene == 0 ? 3 : galleryScene - 1;
        else if ((event.action == UiAction::NavigateRight ||
                  event.action == UiAction::NavigateDown) && move)
            galleryScene = static_cast<uint8_t>((galleryScene + 1) % 4);
        else if (event.action == UiAction::Activate && release)
            galleryPlaying = !galleryPlaying;
    }
    if (!fullRedrawRequested) contentRedrawRequested = true;
}

} // namespace

void setup() {
    Serial.begin(115200);

    if (!display.begin<Board_Wio_Terminal, Config_Wio_Terminal_ILI9341>()) {
        Serial.println("Wio Terminal display initialization failed");
        return;
    }
    display.setRotation(3);

    if (!uiOk(inputHub.add(buttons))) {
        Serial.println("Wio Terminal input registration failed");
        return;
    }
    if (!uiOk(installStableActionMap(inputHub.actionMap()))) {
        Serial.println("Wio Terminal action map initialization failed");
        return;
    }
    if (!uiOk(inputHub.begin())) {
        Serial.println("Wio Terminal button initialization failed");
        return;
    }

    uiReady = true;
    drawPage();
    Serial.println("UI_All_Buttons ready: 5-way + A/B/C enabled");
}

void loop() {
    if (!uiReady) {
        delay(100);
        return;
    }

    const uint32_t now = millis();
    inputHub.scan(now);
    UiEvent event;
    while (inputHub.poll(event)) {
        if (event.type == UiEventType::Action) handleAction(event);
    }

    if (!quickMenuOpen && page == DemoPage::Gallery && galleryPlaying &&
        now - lastAnimationAt >= 120U) {
        lastAnimationAt = now;
        ++galleryFrame;
        drawGalleryArt();
    } else if (!quickMenuOpen && page == DemoPage::Monitor && monitorRunning &&
               now - lastAnimationAt >= 1000U) {
        lastAnimationAt = now;
        drawMonitorUptime();
    }

    if (fullRedrawRequested) {
        drawPage();
    } else {
        if (contentRedrawRequested) {
            drawContent();
            if (quickMenuOpen) drawQuickMenu();
            contentRedrawRequested = false;
        }
        if (headerRedrawRequested) {
            drawHeaderKey();
            headerRedrawRequested = false;
        }
    }
}
