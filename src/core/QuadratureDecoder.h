/**
 * @file QuadratureDecoder.h
 * @brief Allocation-free full-step decoder for mechanical rotary encoders.
 */

#ifndef SEEED_GFX_QUADRATURE_DECODER_H
#define SEEED_GFX_QUADRATURE_DECODER_H

#include <stdint.h>

/**
 * Decode a two-bit Gray-code quadrature signal.
 *
 * A complete A-leading cycle produces +1 and a complete B-leading cycle
 * produces -1. Invalid jumps and contact bounce do not create a step.
 * update() may be called from an ISR; callers must synchronize takeDelta()
 * with that ISR.
 */
class QuadratureDecoder {
public:
    void reset(bool phaseA, bool phaseB);
    void update(bool phaseA, bool phaseB);
    int16_t takeDelta();
    int16_t pendingDelta() const { return _pending; }

private:
    volatile uint8_t _previous = 0;
    volatile int8_t _quarterSteps = 0;
    volatile int16_t _pending = 0;
};

#endif // SEEED_GFX_QUADRATURE_DECODER_H
