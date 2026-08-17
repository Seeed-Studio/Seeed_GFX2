/**
 * @file Bus_ESP32QSPI.h
 * @brief Native ESP32-S3 quad-SPI LCD transport.
 */

#ifndef SEEED_GFX_BUS_ESP32_QSPI_H
#define SEEED_GFX_BUS_ESP32_QSPI_H

#include "Bus_ESP32_LCD_Common.h"
#include "../core/Bus.h"
#include <stdint.h>

#if SEEED_GFX_HAS_ESP32S3_LCD
#include "esp_lcd_panel_io.h"
#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#endif

struct Esp32QspiLcdConfig {
    int8_t sclk;
    int8_t data0;
    int8_t data1;
    int8_t data2;
    int8_t data3;
    int8_t cs;
    uint32_t frequency;
    size_t maxTransferBytes;
};

/**
 * Native quad-SPI LCD transport. Commands are encoded using the SPD2010
 * 0x02/0x32 opcode framing; the SPD2010 driver owns the panel protocol.
 */
class Bus_ESP32QSPI : public IBus {
public:
    explicit Bus_ESP32QSPI(const Esp32QspiLcdConfig& config);
    ~Bus_ESP32QSPI() override;

    bool begin() override;
    void end() override;
    void beginWrite() override {}
    void endWrite() override {}
    void writeCommand(uint8_t cmd) override;
    void writeData(uint8_t data) override;
    void writeData(const uint8_t* data, size_t len) override;
    void writePixels(const uint16_t* data, size_t len) override;
    void writeRepeat(uint16_t pixel, size_t len) override;
    void beginRead() override {}
    void endRead() override {}
    uint8_t readData() override { return 0; }
    size_t maxTransferSize() const override { return _config.maxTransferBytes; }
    // ESP LCD IO uses DMA internally, but raw pixel submission here would
    // bypass the SPD2010 driver's framebuffer/address-window protocol.
    bool supportsDMA() const override { return false; }
    BusCapabilities capabilities() const override {
        BusCapabilities caps;
        caps.dma = false;
        caps.asyncPixelTransfer = false;
        caps.hardwareTransfer = true;
        caps.maxTransferBytes = maxTransferSize();
        return caps;
    }
    void setFrequency(uint32_t freq) override { _config.frequency = freq; }
    uint32_t frequency() const override { return _config.frequency; }
    int lastError() const override { return _lastError; }

    bool writeCommandData(uint8_t cmd, const void* data, size_t len);
    bool writeColor(uint8_t cmd, const void* data, size_t len);
    bool acquireTransferBuffer(uint32_t timeoutMs = 1000);
    void releaseTransferBuffer();
    bool waitForTransfer(uint32_t timeoutMs = 1000);

#if SEEED_GFX_HAS_ESP32S3_LCD
    esp_lcd_panel_io_handle_t panelIo() const { return _io; }
#else
    void* panelIo() const { return nullptr; }
#endif

private:
    Esp32QspiLcdConfig _config;
    int _lastError;
    uint8_t _pendingCommand;
    bool _begun;
    bool _ownsBus;
#if SEEED_GFX_HAS_ESP32S3_LCD
    esp_lcd_panel_io_handle_t _io;
    spi_host_device_t _host;
    SemaphoreHandle_t _transferReady;
    static bool onColorTransferDone(esp_lcd_panel_io_handle_t,
                                    esp_lcd_panel_io_event_data_t*, void*);
#endif
};

#endif
