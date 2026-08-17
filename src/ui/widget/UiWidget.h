#ifndef SEEED_UI_WIDGET_H
#define SEEED_UI_WIDGET_H

#include "../UiEvent.h"
#include "../UiTypes.h"
#include "../render/UiDirtyRegion.h"

class UiCanvas;
struct UiTheme;
class UiContainer;

class UiWidget {
public:
    explicit UiWidget(UiId id = UI_ID_NONE) : _id(id) {}
    virtual ~UiWidget() = default;

    UiId id() const { return _id; }
    void setId(UiId id) { _id = id; }
    const UiRect& bounds() const { return _bounds; }
    UiWidget* parent() const { return _parent; }
    UiWidget* nextSibling() const { return _nextSibling; }

    bool visible() const { return _visible; }
    bool enabled() const { return _enabled; }
    bool focusable() const { return _focusable; }
    bool focused() const { return _focused; }

    void setVisible(bool visible);
    void setEnabled(bool enabled);
    void setFocusable(bool focusable);
    void setBounds(const UiRect& bounds);
    virtual void layout(const UiRect& bounds) { setBounds(bounds); }

    virtual void render(UiCanvas& canvas, const UiTheme& theme) = 0;
    virtual bool onEvent(UiEvent& event);
    virtual UiWidget* hitTest(UiPoint point);
    virtual UiContainer* asContainer() { return nullptr; }
    virtual bool acceptsPointerDrag() const { return false; }
    virtual void setInvalidationSink(IUiInvalidationSink* sink);

    void invalidate();
    void invalidate(const UiRect& screenRect);

protected:
    virtual void onFocusChanged(bool) {}

private:
    friend class UiContainer;
    friend class UiFocusManager;
    void setFocusedInternal(bool focused);

    UiId _id;
    UiRect _bounds;
    UiWidget* _parent = nullptr;
    UiWidget* _nextSibling = nullptr;
    IUiInvalidationSink* _sink = nullptr;
    bool _visible = true;
    bool _enabled = true;
    bool _focusable = false;
    bool _focused = false;
};

class UiContainer : public UiWidget {
public:
    explicit UiContainer(UiId id = UI_ID_NONE) : UiWidget(id) {}
    void addChild(UiWidget& child);
    UiWidget* firstChild() const { return _firstChild; }
    UiContainer* asContainer() override { return this; }
    void setBackground(uint16_t color) { _background = color; _drawBackground = true; invalidate(); }
    void clearBackground() { _drawBackground = false; invalidate(); }
    void render(UiCanvas& canvas, const UiTheme& theme) override;
    UiWidget* hitTest(UiPoint point) override;
    void setInvalidationSink(IUiInvalidationSink* sink) override;

private:
    UiWidget* _firstChild = nullptr;
    UiWidget* _lastChild = nullptr;
    uint16_t _background = 0;
    bool _drawBackground = false;
};

#endif
