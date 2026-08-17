#ifndef SEEED_GFX_GPIO_H
#define SEEED_GFX_GPIO_H

#include <Arduino.h>

#if defined(ESP32)
#include <driver/gpio.h>
#endif

inline void gfxPinModeOutput(int pin) {
    if (pin < 0) return;
#if defined(ESP32)
    gpio_set_direction(static_cast<gpio_num_t>(pin), GPIO_MODE_OUTPUT);
#else
    pinMode(pin, OUTPUT);
#endif
}

inline void gfxPinModeInput(int pin) {
    if (pin < 0) return;
#if defined(ESP32)
    gpio_set_direction(static_cast<gpio_num_t>(pin), GPIO_MODE_INPUT);
#else
    pinMode(pin, INPUT);
#endif
}

inline void gfxPinModeInputPullup(int pin) {
    if (pin < 0) return;
#if defined(ESP32)
    const gpio_num_t gpio = static_cast<gpio_num_t>(pin);
    gpio_set_direction(gpio, GPIO_MODE_INPUT);
    gpio_set_pull_mode(gpio, GPIO_PULLUP_ONLY);
#else
    pinMode(pin, INPUT_PULLUP);
#endif
}

inline void gfxDigitalWrite(int pin, bool high) {
    if (pin < 0) return;
#if defined(ESP32)
    gpio_set_level(static_cast<gpio_num_t>(pin), high ? 1 : 0);
#else
    digitalWrite(pin, high ? HIGH : LOW);
#endif
}

inline bool gfxDigitalRead(int pin) {
    if (pin < 0) return false;
#if defined(ESP32)
    return gpio_get_level(static_cast<gpio_num_t>(pin)) != 0;
#else
    return digitalRead(pin) != LOW;
#endif
}

/** Wait until a GPIO reaches the requested ready level.
 *  Returns false on timeout instead of blocking the application forever. */
inline bool gfxWaitForPin(int pin, bool readyHigh,
                          uint32_t timeoutMs = 30000,
                          uint16_t pollIntervalMs = 1) {
    if (pin < 0) return true;
    const uint32_t started = millis();
    while (gfxDigitalRead(pin) != readyHigh) {
        if (static_cast<uint32_t>(millis() - started) >= timeoutMs) return false;
        delay(pollIntervalMs);
        yield();
    }
    return true;
}

#endif // SEEED_GFX_GPIO_H
