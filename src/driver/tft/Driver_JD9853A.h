/**
 * @file Driver_JD9853A.h
 * @brief JD9853A driver for the 172x320 XIAO 1.47" Touch Display.
 *
 * The panel accepts the ST7789-compatible DCS command path used by the
 * verified XIAO-Display-Board examples, but its native orientation and
 * inversion state differ. Keep those panel-specific rules out of the generic
 * ST7789 driver.
 */

#ifndef SEEED_GFX_DRIVER_JD9853A_H
#define SEEED_GFX_DRIVER_JD9853A_H

#include "Driver_ST7789.h"

class Driver_JD9853A : public Driver_ST7789 {
public:
    Driver_JD9853A(uint16_t w = 172, uint16_t h = 320,
                   uint8_t rgbOrder = TFT_MAD_BGR);

    const char* name() const override { return "JD9853A"; }
    bool init(IBus& bus) override;
    void setRotation(uint8_t rotation) override;
    void setAddrWindow(uint16_t xs, uint16_t ys,
                       uint16_t xe, uint16_t ye) override;

private:
    uint16_t _nativeWidth;
    uint16_t _nativeHeight;
    uint8_t _jdRgbOrder;
    int16_t _jdColstart;
    int16_t _jdRowstart;
};

#endif
