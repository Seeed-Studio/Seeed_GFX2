#ifndef SEEED_GFX_PRODUCT_CATALOG_H
#define SEEED_GFX_PRODUCT_CATALOG_H

#include "../core/Product.h"
#include "../core/Result.h"
#include "../core/Panel.h"
#include <stddef.h>

class DisplayInstance;

enum class ProductPanelMode : uint8_t {
    Default = 0,
    Colorful,
    BWRY,
};

struct ProductDescriptor {
    Seeed_Product::Product id;
    const char* stableId;
    const char* name;
    /** User-visible logical resolution. */
    uint16_t width;
    uint16_t height;
    /** Default frame-buffer bits per pixel, not the physical color count. */
    uint8_t colorDepth;
    ProductPanelMode mode;
    /** Optional controller/frame-buffer geometry; zero means width/height. */
    uint16_t storageWidth;
    uint16_t storageHeight;
    EPaperColorSystem colorSystem;
    uint8_t nativeColorCount;
    uint8_t nativeGrayLevels;
    /** Optional portrait/marketing orientation when native transport differs. */
    uint16_t portraitWidth;
    uint16_t portraitHeight;

    constexpr ProductDescriptor(
        Seeed_Product::Product productId,
        const char* productStableId,
        const char* productName,
        uint16_t visibleWidth,
        uint16_t visibleHeight,
        uint8_t frameBufferDepth,
        ProductPanelMode panelMode,
        uint16_t controllerStorageWidth = 0,
        uint16_t controllerStorageHeight = 0,
        EPaperColorSystem nativeColorSystem = EPaperColorSystem::Unknown,
        uint8_t physicalColorCount = 0,
        uint8_t physicalGrayLevels = 0,
        uint16_t marketingPortraitWidth = 0,
        uint16_t marketingPortraitHeight = 0)
        : id(productId)
        , stableId(productStableId)
        , name(productName)
        , width(visibleWidth)
        , height(visibleHeight)
        , colorDepth(frameBufferDepth)
        , mode(panelMode)
        , storageWidth(controllerStorageWidth)
        , storageHeight(controllerStorageHeight)
        , colorSystem(nativeColorSystem)
        , nativeColorCount(physicalColorCount)
        , nativeGrayLevels(physicalGrayLevels)
        , portraitWidth(marketingPortraitWidth)
        , portraitHeight(marketingPortraitHeight) {}

    constexpr uint16_t driverWidth() const {
        return storageWidth != 0 ? storageWidth : width;
    }

    constexpr uint16_t driverHeight() const {
        return storageHeight != 0 ? storageHeight : height;
    }
};

class ProductCatalog {
public:
    static const ProductDescriptor* find(Seeed_Product::Product product);
    static GfxResult create(Seeed_Product::Product product, DisplayInstance& instance);
    static size_t count();
};

#endif // SEEED_GFX_PRODUCT_CATALOG_H
