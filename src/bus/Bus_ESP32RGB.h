/**
 * @file Bus_ESP32RGB.h
 * @brief Native ESP32-S3 RGB/DPI LCD transport.
 */

#ifndef SEEED_GFX_BUS_ESP32_RGB_H
#define SEEED_GFX_BUS_ESP32_RGB_H

#include "Bus_ESP32_LCD_Common.h"
#include "../core/Bus.h"
#include <stdint.h>

#if SEEED_GFX_HAS_ESP32S3_LCD
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#endif

struct Esp32RgbLcdConfig {
    uint16_t width;
    uint16_t height;
    uint32_t frequency;
    int8_t hsync;
    int8_t vsync;
    int8_t de;
    int8_t pclk;
    int8_t data[16];
    uint16_t hsyncBackPorch;
    uint16_t hsyncFrontPorch;
    uint16_t hsyncPulseWidth;
    uint16_t vsyncBackPorch;
    uint16_t vsyncFrontPorch;
    uint16_t vsyncPulseWidth;
    bool pclkActiveNegative;
    uint16_t bounceBufferLines;
    uint8_t frameBufferCount;
    bool (*panelInit)(void* context);
    bool (*panelSetEnabled)(void* context, bool enabled);
    void* panelInitContext;
};

/** ESP32-S3 RGB LCD peripheral with one or two PSRAM-backed RGB565 framebuffers. */
class Bus_ESP32RGB : public IBus {
public:
    explicit Bus_ESP32RGB(const Esp32RgbLcdConfig& config);
    ~Bus_ESP32RGB() override;

    bool begin() override;
    void end() override;
    void beginWrite() override;
    void endWrite() override;
    void writeCommand(uint8_t) override {}
    void writeData(uint8_t) override {}
    void writeData(const uint8_t*, size_t) override {}
    void writePixels(const uint16_t*, size_t) override {}
    void writeRepeat(uint16_t, size_t) override {}
    void beginRead() override {}
    void endRead() override {}
    uint8_t readData() override { return 0; }
    size_t maxTransferSize() const override {
        return static_cast<size_t>(_config.width) * _config.height * 2U;
    }
    // The RGB peripheral continuously scans its framebuffer via DMA; that is
    // not the queued buffer-lifetime contract exposed by pushImageDMA().
    bool supportsDMA() const override { return false; }
    void setFrequency(uint32_t freq) override;
    uint32_t frequency() const override { return _config.frequency; }
    int lastError() const override { return _lastError; }
    const char* lastErrorMessage() const override { return _lastErrorMessage; }
    BusCapabilities capabilities() const override {
        BusCapabilities caps;
        caps.readable = true;
        caps.dma = false;
        caps.asyncPixelTransfer = false;
        caps.hardwareTransfer = true;
        caps.continuousScanout = true;
        caps.parallel = true;
        caps.maxTransferBytes = maxTransferSize();
        return caps;
    }
    bool isParallel() const override { return true; }

    uint16_t* framebuffer() const { return _framebuffer; }
    uint16_t framebufferWidth() const { return _config.width; }
    uint16_t framebufferHeight() const { return _config.height; }
    bool flushFramebuffer(const void* address, size_t length);
    bool setDisplayEnabled(bool enabled);

#if SEEED_GFX_HAS_ESP32S3_LCD
    esp_lcd_panel_handle_t panelHandle() const { return _panel; }
#else
    void* panelHandle() const { return nullptr; }
#endif

private:
    Esp32RgbLcdConfig _config;
    int _lastError;
    const char* _lastErrorMessage;
    bool _begun;
    uint16_t* _framebuffer;
#if SEEED_GFX_HAS_ESP32S3_LCD
    static bool onFrameBufferComplete(
        esp_lcd_panel_handle_t panel,
        const esp_lcd_rgb_panel_event_data_t* eventData,
        void* context);
    bool syncFramebuffer(const void* address, size_t length);

    esp_lcd_panel_handle_t _panel;
    void* _frameReadySemaphore;
    uint16_t* _framebuffers[2];
    uint8_t _framebufferCount;
    uint8_t _frontBufferIndex;
    uint16_t _writeDepth;
    bool _transactionDirty;
#endif
};

#endif
