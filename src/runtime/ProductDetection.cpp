#include "ProductDetection.h"

#if defined(ARDUINO_ARCH_ESP32)
#include <Arduino.h>
#include <Wire.h>
#include "../board/IoExpander9535.h"
#endif

namespace Seeed_Product {
namespace DetectionDetail {

Product classifyIntegratedDisplay(bool watcherExpander,
                                  bool indicatorExpander,
                                  bool indicatorGxTouch,
                                  bool indicatorDxTouch) {
    if (watcherExpander && !indicatorExpander)
        return SENSECAP_WATCHER;
    if (!watcherExpander && indicatorExpander) {
        if (indicatorGxTouch && !indicatorDxTouch)
            return SENSECAP_INDICATOR_GX;
        if (indicatorDxTouch && !indicatorGxTouch)
            return SENSECAP_INDICATOR_DX;
    }
    return CUSTOM;
}

}

#if defined(ARDUINO_ARCH_ESP32)
namespace {

bool probeAddress(TwoWire& wire, uint8_t address) {
    wire.beginTransmission(address);
    return wire.endTransmission() == 0;
}

bool beginEsp32Wire(TwoWire& wire, int sda, int scl) {
    // ESP32 Arduino cores keep the existing pins when begin() is called on an
    // already-started controller. Product detection probes Watcher first and
    // Indicator second, so explicitly stop the previous candidate bus before
    // binding Wire to the next product's pins.
    wire.end();
    delay(1);
    return wire.begin(sda, scl, 400000);
}

bool prepareIndicatorTouch(uint8_t expanderAddress) {
    IoExpander9535 expander(Wire, expanderAddress);
    if (!expander.begin()) return false;
    // Mirror Board_SenseCAP_Indicator::begin(): reset the touch/panel rail,
    // then enable the two required rails while preserving every other latch.
    if (!expander.pinModeOutput(7, false)) return false;
    delay(5);
    if (!expander.writePin(7, true)) return false;
    if (!expander.pinModeOutput(8, true)) return false;
    if (!expander.pinModeOutput(10, true)) return false;
    delay(20);
    return true;
}

}
#endif

Product detectIntegratedDisplayProduct() {
#if defined(SEEED_SENSECAP_WATCHER) || defined(ARDUINO_SENSECAP_WATCHER)
    return SENSECAP_WATCHER;
#elif defined(SEEED_SENSECAP_INDICATOR_GX) || \
      defined(ARDUINO_SENSECAP_INDICATOR_GX)
    return SENSECAP_INDICATOR_GX;
#elif defined(SEEED_SENSECAP_INDICATOR_DX) || \
      defined(ARDUINO_SENSECAP_INDICATOR_DX)
    return SENSECAP_INDICATOR_DX;
#elif defined(ARDUINO_WIO_TERMINAL)
    return WIO_TERMINAL_PRODUCT;
#elif defined(ARDUINO_ARCH_ESP32)
    bool indicator = false;
    bool gxTouch = false;
    bool dxTouch = false;

    // Watcher: PCA9535 at 0x21 on the product control bus.
    if (beginEsp32Wire(Wire, 47, 48) && probeAddress(Wire, 0x21))
        return SENSECAP_WATCHER;

    // Indicator GX/DX: TCA9535 at 0x20 (some revisions use 0x39),
    // followed by FT6x36 at 0x48 for GX or 0x38 for DX.
    if (beginEsp32Wire(Wire, 39, 40)) {
        uint8_t expanderAddress = 0;
        if (probeAddress(Wire, 0x20)) expanderAddress = 0x20;
        else if (probeAddress(Wire, 0x39)) expanderAddress = 0x39;
        indicator = expanderAddress != 0;
        if (indicator && prepareIndicatorTouch(expanderAddress)) {
            gxTouch = probeAddress(Wire, 0x48);
            dxTouch = probeAddress(Wire, 0x38);
        }
    }

    return DetectionDetail::classifyIntegratedDisplay(
        false, indicator, gxTouch, dxTouch);
#else
    return CUSTOM;
#endif
}

}
