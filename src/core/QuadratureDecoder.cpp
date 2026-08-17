#include "QuadratureDecoder.h"
#include <limits.h>

namespace {
// Index: previous AB in bits 3:2, current AB in bits 1:0.
const int8_t kTransition[16] = {
     0, -1,  1,  0,
     1,  0,  0, -1,
    -1,  0,  0,  1,
     0,  1, -1,  0,
};
}

void QuadratureDecoder::reset(bool phaseA, bool phaseB) {
    _previous = static_cast<uint8_t>(
        (phaseA ? 2U : 0U) | (phaseB ? 1U : 0U));
    _quarterSteps = 0;
    _pending = 0;
}

void QuadratureDecoder::update(bool phaseA, bool phaseB) {
    const uint8_t current = static_cast<uint8_t>(
        (phaseA ? 2U : 0U) | (phaseB ? 1U : 0U));
    const uint8_t previous = _previous;
    const uint8_t index = static_cast<uint8_t>((previous << 2) | current);
    _previous = current;

    const int8_t movement = kTransition[index];
    if (!movement) {
        // A simultaneous two-bit change is not a legal quadrature edge. Drop
        // any partial step so a missed edge cannot later become a false
        // detent. A repeated sample keeps the legitimate partial step.
        if (current != previous) {
            _quarterSteps = 0;
        }
        return;
    }
    _quarterSteps = static_cast<int8_t>(_quarterSteps + movement);

    if (_quarterSteps >= 4) {
        if (_pending < INT16_MAX) {
            _pending = static_cast<int16_t>(_pending + 1);
        }
        _quarterSteps = 0;
    } else if (_quarterSteps <= -4) {
        if (_pending > INT16_MIN) {
            _pending = static_cast<int16_t>(_pending - 1);
        }
        _quarterSteps = 0;
    }
}

int16_t QuadratureDecoder::takeDelta() {
    const int16_t result = _pending;
    _pending = 0;
    return result;
}
