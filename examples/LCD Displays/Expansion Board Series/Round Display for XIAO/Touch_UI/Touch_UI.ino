/**
 * XIAO ESP32-S3 + Seeed Studio Round Display touch UI example.
 *
 * Hardware handled by Seeed_GFX:
 *   GC9A01 240x240 display: CS D1, DC D3, BL D6, MOSI D10, SCLK D8
 *   CHSCX6X capacitive touch: I2C address 0x2E, interrupt D7
 *
 * Interaction:
 *   - Draw inside the central pad with a finger.
 *   - COLOR cycles through five brush colors.
 *   - UNDO removes the latest stroke.
 *   - CLEAR removes all strokes.
 *
 * Select "XIAO_ESP32S3" in Arduino IDE. No extra touch library is needed.
 */

#include <Seeed_GFX.h>
#include <Seeed_UI.h>
#include <Wire.h>
#include <stdio.h>
#include <string.h>

#if defined(ARDUINO_ARCH_ESP32) && !defined(CONFIG_IDF_TARGET_ESP32S3)
#error "Touch_UI targets XIAO ESP32S3. Select XIAO_ESP32S3 in Arduino IDE."
#endif

namespace {

constexpr uint16_t kBackground = 0x0004;
constexpr uint16_t kSurface = 0x10A3;
constexpr uint16_t kPadColor = 0x0841;
constexpr uint16_t kBorder = 0x2D7F;
constexpr uint16_t kPalette[] = {
    TFT_CYAN, TFT_YELLOW, TFT_MAGENTA, TFT_GREEN, TFT_WHITE
};
const char* const kColorNames[] = {
    "CYAN", "YELLOW", "MAGENTA", "GREEN", "WHITE"
};
constexpr size_t kPaletteSize = sizeof(kPalette) / sizeof(kPalette[0]);
constexpr size_t kMaxPaintPoints = 256;

enum CommandId : uint16_t {
    CommandColor = 1,
    CommandUndo,
    CommandClear,
};

struct PaintPoint {
    int16_t x;
    int16_t y;
    uint16_t color;
    bool startsStroke;
};

class PaintPad : public UiWidget {
public:
    PaintPad(UiLabel& status, char* statusText, size_t statusCapacity)
        : UiWidget(10), _status(status), _statusText(statusText),
          _statusCapacity(statusCapacity) {}

    void render(UiCanvas& canvas, const UiTheme&) override {
        Seeed_GFX& gfx = canvas.gfx();
        const UiRect& area = bounds();
        gfx.fillRoundRect(area.x, area.y, area.w, area.h, 12, kPadColor);
        gfx.drawRoundRect(area.x, area.y, area.w, area.h, 12, kBorder);

        if (_count == 0) {
            gfx.setTextDatum(MC_DATUM);
            gfx.setTextColor(0x7BEF);
            gfx.setTextSize(1);
            gfx.drawString("DRAW HERE", area.x + area.w / 2,
                           area.y + area.h / 2);
        } else {
            for (size_t i = 0; i < _count; ++i) {
                const PaintPoint& point = _points[i];
                if (i && !point.startsStroke) {
                    const PaintPoint& previous = _points[i - 1];
                    gfx.drawLine(previous.x, previous.y, point.x, point.y,
                                 point.color);
                }
                gfx.fillCircle(point.x, point.y, 3, point.color);
            }
        }

        // Current brush indicator, kept above the drawing.
        gfx.fillCircle(area.x + area.w - 13, area.y + 13, 6,
                       kPalette[_colorIndex]);
        gfx.drawCircle(area.x + area.w - 13, area.y + 13, 7, TFT_WHITE);
    }

    bool onEvent(UiEvent& event) override {
        const UiPoint position = {event.x, event.y};
        if (event.type == UiEventType::PointerDown) {
            if (!bounds().contains(position)) return false;
            _drawing = true;
            addPoint(event.x, event.y, true);
            return true;
        }
        if (event.type == UiEventType::PointerMove && _drawing) {
            if (bounds().contains(position)) addPoint(event.x, event.y, false);
            return true;
        }
        if (event.type == UiEventType::PointerUp ||
            event.type == UiEventType::PointerCancel) {
            const bool handled = _drawing;
            _drawing = false;
            return handled;
        }
        return false;
    }

    void cycleColor() {
        _colorIndex = (_colorIndex + 1) % kPaletteSize;
        snprintf(_statusText, _statusCapacity, "BRUSH: %s",
                 kColorNames[_colorIndex]);
        _status.setText(_statusText);
        invalidate(bounds());
    }

    void undo() {
        if (_count == 0) {
            setStatus("NOTHING TO UNDO");
            return;
        }
        size_t strokeStart = _count - 1;
        while (strokeStart > 0 && !_points[strokeStart].startsStroke)
            --strokeStart;
        _count = strokeStart;
        setStatus("LAST STROKE REMOVED");
        invalidate(bounds());
    }

    void clear() {
        _count = 0;
        _drawing = false;
        setStatus("CANVAS CLEARED");
        invalidate(bounds());
    }

private:
    void setStatus(const char* text) {
        snprintf(_statusText, _statusCapacity, "%s", text);
        _status.setText(_statusText);
    }

    void addPoint(int16_t x, int16_t y, bool startsStroke) {
        // Avoid filling the fixed buffer with multiple identical samples.
        if (_count && !startsStroke) {
            const PaintPoint& previous = _points[_count - 1];
            const int32_t dx = static_cast<int32_t>(x) - previous.x;
            const int32_t dy = static_cast<int32_t>(y) - previous.y;
            if (dx * dx + dy * dy < 9) return;
        }

        bool compacted = false;
        if (_count == kMaxPaintPoints) {
            const size_t keep = kMaxPaintPoints / 2;
            memmove(_points, _points + (kMaxPaintPoints - keep),
                    keep * sizeof(PaintPoint));
            _count = keep;
            _points[0].startsStroke = true;
            compacted = true;
        }

        PaintPoint& point = _points[_count++];
        point.x = x;
        point.y = y;
        point.color = kPalette[_colorIndex];
        point.startsStroke = startsStroke;

        snprintf(_statusText, _statusCapacity, "TOUCH %3d, %3d", x, y);
        _status.setText(_statusText);

        if (compacted) {
            invalidate(bounds());
        } else {
            UiRect dirty;
            dirty.x = x - 5;
            dirty.y = y - 5;
            dirty.w = 11;
            dirty.h = 11;
            if (_count > 1 && !startsStroke) {
                const PaintPoint& previous = _points[_count - 2];
                UiRect previousArea = {static_cast<int16_t>(previous.x - 5),
                                       static_cast<int16_t>(previous.y - 5), 11, 11};
                dirty = dirty.united(previousArea);
            }
            invalidate(dirty.intersection(bounds()));
        }
    }

    UiLabel& _status;
    char* _statusText;
    size_t _statusCapacity;
    PaintPoint _points[kMaxPaintPoints] = {};
    size_t _count = 0;
    size_t _colorIndex = 0;
    bool _drawing = false;
};

class TouchPaintScreen : public UiScreen {
public:
    TouchPaintScreen()
        : _status(_statusText, 1),
          _paintPad(_status, _statusText, sizeof(_statusText)),
          _colorButton("COLOR", {handleCommand, this, CommandColor}, 20),
          _undoButton("UNDO", {handleCommand, this, CommandUndo}, 21),
          _clearButton("CLEAR", {handleCommand, this, CommandClear}, 22) {
        _root.setBackground(kBackground);
    }

    UiStatus onCreate() override {
        _status.setBounds({72, 14, 120, 16});
        _status.setColor(0xA69F);
        _paintPad.setBounds({38, 36, 164, 126});
        _colorButton.setBounds({18, 174, 64, 40});
        _undoButton.setBounds({88, 174, 64, 40});
        _clearButton.setBounds({158, 174, 64, 40});

        _root.addChild(_status);
        _root.addChild(_paintPad);
        _root.addChild(_colorButton);
        _root.addChild(_undoButton);
        _root.addChild(_clearButton);
        return UiStatus::Ok;
    }

    UiWidget& root() override { return _root; }

private:
    static void handleCommand(void* context, uint16_t commandId) {
        TouchPaintScreen* screen = static_cast<TouchPaintScreen*>(context);
        if (commandId == CommandColor) screen->_paintPad.cycleColor();
        else if (commandId == CommandUndo) screen->_paintPad.undo();
        else if (commandId == CommandClear) screen->_paintPad.clear();
    }

    UiContainer _root;
    char _statusText[32] = "TOUCH & DRAW";
    UiLabel _status;
    PaintPad _paintPad;
    UiButton _colorButton;
    UiButton _undoButton;
    UiButton _clearButton;
};

Seeed_GFX display(Seeed_Product::Seeed_Round_Display_XIAO);
// PaintPad already filters samples closer than 3 px; match that threshold here
// so drag strokes stay continuous without forwarding duplicate touch noise.
TouchInput touch(display, 2, 600, 3);
UiInputHub input;
UiTheme theme;
UiApplication application(display, input, theme);
TouchPaintScreen mainScreen;
bool uiReady = false;

void showFatalError(const char* message) {
    display.fillScreen(TFT_RED);
    display.setTextDatum(MC_DATUM);
    display.setTextColor(TFT_WHITE);
    display.setTextSize(1);
    display.drawString(message ? message : "INITIALIZATION ERROR", 120, 120);
}

void printTouchBusDiagnostic() {
    uint8_t frame[5] = {0};
    const size_t received = Wire.requestFrom(static_cast<uint8_t>(0x2E),
                                             static_cast<uint8_t>(sizeof(frame)));
    size_t consumed = 0;
    while (Wire.available() && consumed < sizeof(frame)) {
        frame[consumed++] = static_cast<uint8_t>(Wire.read());
    }
    Serial.printf("Touch probe: I2C=%u/5 bytes, IRQ(D7)=%s, status=0x%02X\n",
                  static_cast<unsigned>(received),
                  digitalRead(D7) == LOW ? "LOW" : "HIGH",
                  static_cast<unsigned>(frame[0]));
}

} // namespace

void setup() {
    Serial.begin(115200);

    if (!display.begin()) {
        Serial.println(display.lastResult().message);
        return;
    }
    display.setRotation(0);
    Serial.println("Round Display initialized: CHSC6X uses SDA=D4, SCL=D5, IRQ=D7");
    printTouchBusDiagnostic();

    theme.colors.background = kBackground;
    theme.colors.surface = kSurface;
    theme.colors.textPrimary = TFT_WHITE;
    theme.colors.textSecondary = 0x7BEF;
    theme.colors.accent = 0x0451;
    theme.colors.focus = kBorder;
    theme.colors.disabled = 0x3186;
    theme.bodyTextSize = 1;
    theme.smallTextSize = 1;
    theme.metrics.cornerRadius = 8;
    theme.metrics.touchSlop = 5;
    theme.metrics.minTouchTarget = 40;

    if (!uiOk(input.add(touch))) {
        showFatalError("INPUT REGISTRATION FAILED");
        return;
    }

    const UiStatus status = application.begin(mainScreen);
    if (!uiOk(status)) {
        Serial.printf("UI init failed: %u\n", static_cast<unsigned>(status));
        showFatalError("UI INITIALIZATION FAILED");
        return;
    }
    uiReady = true;
    Serial.println("Touch_UI ready - touch the drawing pad or a button");
}

void loop() {
    if (!uiReady) {
        delay(100);
        return;
    }
    const UiStatus status = application.tick(millis());
    if (!uiOk(status)) {
        Serial.printf("UI tick error: %u\n", static_cast<unsigned>(status));
        delay(100);
    }
    delay(2);
}
