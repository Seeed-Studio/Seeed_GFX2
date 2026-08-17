#ifndef SEEED_GFX_PRODUCT_DETECTION_H
#define SEEED_GFX_PRODUCT_DETECTION_H

#include "../core/Product.h"

namespace Seeed_Product {

/**
 * Detect the integrated display product used by the portable examples.
 *
 * Wio Terminal is selected from its Arduino board macro. On ESP32-S3,
 * Watcher and Indicator variants are distinguished by their I2C devices;
 * the shared Wire controller is rebound between their different pin pairs.
 * CUSTOM is returned when detection is unavailable or ambiguous.
 */
Product detectIntegratedDisplayProduct();

namespace DetectionDetail {
Product classifyIntegratedDisplay(bool watcherExpander,
                                  bool indicatorExpander,
                                  bool indicatorGxTouch,
                                  bool indicatorDxTouch);
}

}

#endif
