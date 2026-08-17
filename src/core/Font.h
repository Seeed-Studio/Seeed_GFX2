/**
 * @file   Font.h
 * @brief  Font data structures and constants for Seeed_GFX v2.0
 *
 * Defines the font information structure, font numbers,
 * text datum constants, and GFX font structures.
 */

#ifndef SEEED_GFX_FONT_H
#define SEEED_GFX_FONT_H

#include <stdint.h>
#include <stddef.h>

#ifndef pgm_read_ptr
#define pgm_read_ptr(addr) ((void*)pgm_read_dword(addr))
#endif

// Font information structure

/** Holds metadata for built-in fonts */
typedef struct {
    const uint8_t *chartbl;   /**< Pointer to character image address table */
    const uint8_t *widthtbl;  /**< Pointer to character width table */
    uint8_t  height;          /**< Font height in pixels */
    uint8_t  baseline;        /**< Distance from top to baseline */
} fontinfo;

// Font number constants

#define FONT_GLCD  1   /**< Adafruit 8-pixel font */
#define FONT_16    2   /**< 16-pixel high font */
#define FONT_32    4   /**< 32-pixel high RLE font */
#define FONT_64    6   /**< 48-pixel high RLE font */
#define FONT_7SEG  7   /**< 7-segment 48-pixel font */
#define FONT_72    8   /**< 75-pixel high RLE font */

// Text datum constants

#define TL_DATUM    0   /**< Top left (default) */
#define TC_DATUM    1   /**< Top centre */
#define TR_DATUM    2   /**< Top right */
#define ML_DATUM    3   /**< Middle left */
#define CL_DATUM    3   /**< Centre left, same as above */
#define MC_DATUM    4   /**< Middle centre */
#define CC_DATUM    4   /**< Centre centre, same as above */
#define MR_DATUM    5   /**< Middle right */
#define CR_DATUM    5   /**< Centre right, same as above */
#define BL_DATUM    6   /**< Bottom left */
#define BC_DATUM    7   /**< Bottom centre */
#define BR_DATUM    8   /**< Bottom right */
#define L_BASELINE  9   /**< Left character baseline */
#define C_BASELINE  10  /**< Centre character baseline */
#define R_BASELINE  11  /**< Right character baseline */

// GFX Font structures (from Adafruit_GFX)

/** Glyph data structure for GFX fonts */
typedef struct {
    uint32_t bitmapOffset;  /**< Byte offset into the bitmap array */
    uint8_t  width;         /**< Bitmap width in pixels */
    uint8_t  height;        /**< Bitmap height in pixels */
    uint8_t  xAdvance;      /**< Cursor advance distance (x axis) */
    int8_t   xOffset;       /**< Offset from cursor to top-left of bitmap */
    int8_t   yOffset;       /**< Offset from cursor to top-left of bitmap */
} GFXglyph;

/** GFX font structure */
typedef struct {
    uint8_t  *bitmap;       /**< Concatenated glyph bitmaps */
    GFXglyph *glyph;        /**< Array of glyph descriptors */
    uint16_t  first;        /**< First character code in range */
    uint16_t  last;         /**< Last character code in range */
    uint8_t   yAdvance;     /**< Newline distance (y axis) */
} GFXfont;

// Smooth font callback

/** Callback type for smooth font pixel color reading */
typedef uint16_t (*getColorCallback)(uint16_t x, uint16_t y);

#endif // SEEED_GFX_FONT_H
