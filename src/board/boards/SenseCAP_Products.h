/**
 * @file SenseCAP_Products.h
 * @brief Integrated display boards for SenseCAP Watcher and Indicator.
 */

#ifndef SEEED_GFX_SENSECAP_PRODUCTS_H
#define SEEED_GFX_SENSECAP_PRODUCTS_H

#include "../../core/Board.h"
#include "../../core/QuadratureDecoder.h"
#include "../IoExpander9535.h"
#include "../../bus/Bus_ESP32QSPI.h"
#include "../../bus/Bus_ESP32RGB.h"
#include <Wire.h>

class Board_SenseCAP_Watcher : public IBoard {
public:
    Board_SenseCAP_Watcher();
    ~Board_SenseCAP_Watcher() override;

    const char* name() const override { return "SenseCAP Watcher"; }
    bool begin() override;
    int8_t pinCS() const override { return 45; }
    int8_t pinDC() const override { return -1; }
    int8_t pinRST() const override { return -1; }
    int8_t pinBacklight() const override { return 8; }
    int8_t pinMOSI() const override { return 9; }
    int8_t pinMISO() const override { return 1; }
    int8_t pinSCLK() const override { return 7; }
    int8_t pinTouchIRQ() const override { return -1; }
    IBus* createBus() override;
    void setBacklight(uint8_t brightness) override;
    void powerOn() override;
    void powerOff() override;

    /** Initialize GPIO41/42 encoder interrupts and expander P03 button. */
    bool beginControls();
    /** Stop encoder interrupts. Safe to call repeatedly. */
    void endControls();
    /** Return accumulated detents since the previous call. */
    int16_t readKnobDelta();
    /** Read the active-low knob push switch from IO expander P03. */
    bool readKnobButton(bool& pressed);
    bool controlsReady() const { return _controlsReady; }

private:
    static void knobEdgeThunk(void* context);
    void sampleKnob();

    IoExpander9535 _expander;
    bool _expanderReady;
    bool _controlsReady;
    QuadratureDecoder _knobDecoder;
};

enum class SenseCAPIndicatorPanel : uint8_t {
    GX_ST7701S,
    DX_RGB,
};

class Board_SenseCAP_Indicator : public IBoard {
public:
    explicit Board_SenseCAP_Indicator(
        SenseCAPIndicatorPanel panel = SenseCAPIndicatorPanel::GX_ST7701S);

    const char* name() const override {
        return _panel == SenseCAPIndicatorPanel::GX_ST7701S
            ? "SenseCAP Indicator GX" : "SenseCAP Indicator DX";
    }
    bool begin() override;
    int8_t pinCS() const override { return -1; }
    int8_t pinDC() const override { return -1; }
    int8_t pinRST() const override { return -1; }
    int8_t pinBacklight() const override { return 45; }
    int8_t pinMOSI() const override { return 48; }
    int8_t pinMISO() const override { return 47; }
    int8_t pinSCLK() const override { return 41; }
    IBus* createBus() override;
    void setBacklight(uint8_t brightness) override;
    void powerOff() override { setBacklight(0); }

    uint8_t touchAddress() const {
        return _panel == SenseCAPIndicatorPanel::GX_ST7701S ? 0x48 : 0x38;
    }
    SenseCAPIndicatorPanel panelVariant() const { return _panel; }

private:
    static bool initializePanelThunk(void* context);
    static bool setPanelEnabledThunk(void* context, bool enabled);
    bool initializePanel();
    bool initializeSt7701S();
    bool setSt7701SEnabled(bool enabled);
    bool sidebandCommand(uint16_t command);
    bool sidebandData(uint8_t data);
    void sidebandSend9(uint16_t value);
    bool select(bool active);

    SenseCAPIndicatorPanel _panel;
    IoExpander9535 _expander20;
    IoExpander9535 _expander39;
    IoExpander9535* _expander;
    bool _controllerSleeping;
};

#endif
