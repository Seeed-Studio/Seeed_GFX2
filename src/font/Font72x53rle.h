#ifndef SEEED_GFX_FONT72X53RLE_H
#define SEEED_GFX_FONT72X53RLE_H

// This legacy alternative uses the same public symbols as Font72rle and
// therefore cannot be linked at the same time.  It is intentionally not part
// of the library build.  Keep the availability macro so sketches can select a
// different bundled font without accidentally including a missing .c file.
#define SEEED_GFX_FONT72X53_AVAILABLE 0

#endif
