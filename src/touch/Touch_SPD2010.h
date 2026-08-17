/**
 * @file Touch_SPD2010.h
 * @brief SPD2010 integrated capacitive touch controller.
 *
 * Protocol adapted from Espressif's Apache-2.0 esp_lcd_touch_spd2010
 * component. This standalone Wire implementation avoids an ESP-IDF component
 * dependency in Arduino builds.
 */

#ifndef SEEED_GFX_TOUCH_SPD2010_H
#define SEEED_GFX_TOUCH_SPD2010_H

#include <Arduino.h>
#include <Wire.h>
#include "../core/Touch.h"

class Touch_SPD2010 : public ITouch {
public:
    Touch_SPD2010(TwoWire& wire, int8_t sda, int8_t scl,
                  uint16_t width = 412, uint16_t height = 412,
                  uint8_t address = 0x53, uint32_t frequency = 400000);

    const char* name() const override { return "SPD2010 Touch"; }
    bool begin(IBus& bus) override;
    bool read(TouchPoint& point) override;
    uint8_t readMulti(TouchPoint* points, uint8_t maxPts) override;
    bool isPressed() override;
    uint8_t maxPoints() const override { return 10; }
    uint8_t gesture() const override { return _gesture; }
    void setRotation(uint8_t rotation) override { _rotation = rotation & 3U; }

private:
    bool writeBytes(const uint8_t* data, size_t len);
    bool readBytes(uint8_t* data, size_t len);
    bool writeCommand(uint8_t command);
    bool readStatus(uint8_t status[4]);
    bool clearInterrupt();
    void rotate(uint16_t& x, uint16_t& y) const;

    TwoWire& _wire;
    int8_t _sda;
    int8_t _scl;
    uint16_t _width;
    uint16_t _height;
    uint8_t _address;
    uint32_t _frequency;
    uint8_t _rotation;
    uint8_t _gesture;
    bool _initialized;
};

#endif
