#ifndef SEEED_UI_WIDGETS_H
#define SEEED_UI_WIDGETS_H

#include <stddef.h>
#include "UiWidget.h"

using UiCommandHandler = void (*)(void* context, uint16_t commandId);

struct UiCommand {
    UiCommandHandler handler = nullptr;
    void* context = nullptr;
    uint16_t id = 0;
    void invoke() const { if (handler) handler(context, id); }
};

class UiLabel : public UiWidget {
public:
    UiLabel(const char* text = nullptr, UiId id = UI_ID_NONE) : UiWidget(id), _text(text) {}
    void setText(const char* text) { _text = text; invalidate(); }
    const char* text() const { return _text; }
    void setTextSize(uint8_t size) { _textSize = size; invalidate(); }
    void setColor(uint16_t color) { _color = color; _customColor = true; invalidate(); }
    void render(UiCanvas& canvas, const UiTheme& theme) override;

private:
    const char* _text = nullptr;
    uint16_t _color = 0;
    uint8_t _textSize = 0;
    bool _customColor = false;
};

class UiButton : public UiWidget {
public:
    UiButton(const char* text = nullptr, UiCommand command = UiCommand(),
             UiId id = UI_ID_NONE);
    void setText(const char* text) { _text = text; invalidate(); }
    void setCommand(UiCommand command) { _command = command; }
    void render(UiCanvas& canvas, const UiTheme& theme) override;
    bool onEvent(UiEvent& event) override;

private:
    const char* _text;
    UiCommand _command;
    bool _pressed = false;
};

struct UiMenuItem {
    UiId id = UI_ID_NONE;
    const char* title = nullptr;
    const char* subtitle = nullptr;
    UiCommand command;
    bool enabled = true;
    bool visible = true;
};

class UiMenu : public UiWidget {
public:
    UiMenu(const UiMenuItem* items = nullptr, uint16_t count = 0,
           UiId id = UI_ID_NONE);
    void setItems(const UiMenuItem* items, uint16_t count);
    uint16_t currentIndex() const { return _current; }
    uint16_t firstVisibleIndex() const { return _firstVisible; }
    void setCurrentIndex(uint16_t index);
    void setWrapNavigation(bool wrap) { _wrap = wrap; }
    void setRowHeight(int16_t rowHeight) { _rowHeight = rowHeight; invalidate(); }
    void render(UiCanvas& canvas, const UiTheme& theme) override;
    bool onEvent(UiEvent& event) override;

private:
    bool move(int direction);
    void ensureVisible();
    void activate();
    int16_t indexAt(int16_t y) const;

    const UiMenuItem* _items;
    uint16_t _count;
    uint16_t _current = 0;
    uint16_t _firstVisible = 0;
    int16_t _rowHeight = 0;
    int16_t _pressedIndex = -1;
    bool _wrap = false;
};

// The first implementation uses the same virtualized fixed-row engine for
// menus and generic lists. A richer model-backed facade can be added without
// changing focus or event semantics.
using UiListView = UiMenu;

class UiScrollView : public UiContainer {
public:
    explicit UiScrollView(int16_t step = 24, UiId id = UI_ID_NONE)
        : UiContainer(id), _step(step) { setFocusable(true); }
    void setContentExtent(int16_t extent) { _contentExtent = extent; setOffset(_offset); }
    void setOffset(int16_t offset);
    int16_t offset() const { return _offset; }
    bool acceptsPointerDrag() const override { return true; }
    bool onEvent(UiEvent& event) override;
    void layout(const UiRect& bounds) override;
    void render(UiCanvas& canvas, const UiTheme& theme) override;
private:
    int16_t _contentExtent = 0;
    int16_t _offset = 0;
    int16_t _step;
    int16_t _lastPointerY = 0;
    bool _dragging = false;
};

class UiToggle : public UiWidget {
public:
    UiToggle(const char* text = nullptr, bool checked = false,
             UiCommand command = UiCommand(), UiId id = UI_ID_NONE);
    bool checked() const { return _checked; }
    void setChecked(bool checked);
    void render(UiCanvas& canvas, const UiTheme& theme) override;
    bool onEvent(UiEvent& event) override;

private:
    void toggle();
    const char* _text;
    UiCommand _command;
    bool _checked;
    bool _pressed = false;
};

class UiValueItem : public UiWidget {
public:
    UiValueItem(const char* text, int32_t value, int32_t minimum, int32_t maximum,
                int32_t step = 1, UiCommand command = UiCommand(),
                UiId id = UI_ID_NONE);
    int32_t value() const { return _value; }
    void setValue(int32_t value);
    void render(UiCanvas& canvas, const UiTheme& theme) override;
    bool onEvent(UiEvent& event) override;

private:
    bool adjust(int direction);
    const char* _text;
    int32_t _value, _minimum, _maximum, _step;
    UiCommand _command;
};

class UiProgressBar : public UiWidget {
public:
    UiProgressBar(int32_t value = 0, int32_t minimum = 0, int32_t maximum = 100,
                  UiId id = UI_ID_NONE)
        : UiWidget(id), _value(value), _minimum(minimum), _maximum(maximum) {}
    void setValue(int32_t value);
    int32_t value() const { return _value; }
    void render(UiCanvas& canvas, const UiTheme& theme) override;

private:
    int32_t _value, _minimum, _maximum;
};

class UiStatusBar : public UiContainer {
public:
    UiStatusBar(const char* title = nullptr, UiId id = UI_ID_NONE)
        : UiContainer(id), _title(title) {}
    void setTitle(const char* title) { _title = title; invalidate(); }
    void render(UiCanvas& canvas, const UiTheme& theme) override;
private:
    const char* _title;
};

class UiDialog : public UiContainer {
public:
    UiDialog(const char* title = nullptr, const char* message = nullptr,
             UiId id = UI_ID_NONE)
        : UiContainer(id), _title(title), _message(message) {}
    void render(UiCanvas& canvas, const UiTheme& theme) override;
private:
    const char* _title;
    const char* _message;
};

#endif
