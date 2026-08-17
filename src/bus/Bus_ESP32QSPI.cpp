#include "Bus_ESP32QSPI.h"

namespace {
constexpr uint32_t kQspiWriteCommand = 0x02000000UL;
constexpr uint32_t kQspiWriteColor = 0x32000000UL;
}

Bus_ESP32QSPI::Bus_ESP32QSPI(const Esp32QspiLcdConfig& config)
    : _config(config), _lastError(0), _pendingCommand(0), _begun(false),
      _ownsBus(false)
#if SEEED_GFX_HAS_ESP32S3_LCD
    , _io(nullptr), _host(SPI3_HOST), _transferReady(nullptr)
#endif
{}

Bus_ESP32QSPI::~Bus_ESP32QSPI() {
    end();
}

bool Bus_ESP32QSPI::begin() {
    if (_begun) return true;
#if SEEED_GFX_HAS_ESP32S3_LCD
    _transferReady = xSemaphoreCreateBinary();
    if (!_transferReady) {
        _lastError = ESP_ERR_NO_MEM;
        return false;
    }
    xSemaphoreGive(_transferReady);

    spi_bus_config_t busConfig = {};
    busConfig.sclk_io_num = _config.sclk;
    busConfig.data0_io_num = _config.data0;
    busConfig.data1_io_num = _config.data1;
    busConfig.data2_io_num = _config.data2;
    busConfig.data3_io_num = _config.data3;
    busConfig.data4_io_num = -1;
    busConfig.data5_io_num = -1;
    busConfig.data6_io_num = -1;
    busConfig.data7_io_num = -1;
    busConfig.max_transfer_sz = static_cast<int>(_config.maxTransferBytes);
    busConfig.flags = SPICOMMON_BUSFLAG_QUAD;
    esp_err_t err = spi_bus_initialize(_host, &busConfig, SPI_DMA_CH_AUTO);
    _ownsBus = err == ESP_OK;
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        _lastError = err;
        vSemaphoreDelete(_transferReady);
        _transferReady = nullptr;
        return false;
    }

    esp_lcd_panel_io_spi_config_t ioConfig = {};
    ioConfig.cs_gpio_num = _config.cs;
    ioConfig.dc_gpio_num = -1;
    ioConfig.spi_mode = 3;
    ioConfig.pclk_hz = static_cast<int>(_config.frequency);
    ioConfig.trans_queue_depth = 10;
    ioConfig.on_color_trans_done = &Bus_ESP32QSPI::onColorTransferDone;
    ioConfig.user_ctx = this;
    ioConfig.lcd_cmd_bits = 32;
    ioConfig.lcd_param_bits = 8;
    ioConfig.flags.quad_mode = true;
    err = esp_lcd_new_panel_io_spi(
        static_cast<esp_lcd_spi_bus_handle_t>(_host), &ioConfig, &_io);
    if (err != ESP_OK) {
        _lastError = err;
        if (_ownsBus) (void)spi_bus_free(_host);
        _ownsBus = false;
        vSemaphoreDelete(_transferReady);
        _transferReady = nullptr;
        return false;
    }
    _begun = true;
    _lastError = ESP_OK;
    return true;
#else
    _lastError = -1;
    return false;
#endif
}

void Bus_ESP32QSPI::end() {
#if SEEED_GFX_HAS_ESP32S3_LCD
    if (_transferReady) (void)waitForTransfer(1000);
    if (_io) {
        (void)esp_lcd_panel_io_del(_io);
        _io = nullptr;
    }
    if (_ownsBus) (void)spi_bus_free(_host);
    _ownsBus = false;
    if (_transferReady) {
        vSemaphoreDelete(_transferReady);
        _transferReady = nullptr;
    }
#endif
    _begun = false;
}

#if SEEED_GFX_HAS_ESP32S3_LCD
bool Bus_ESP32QSPI::onColorTransferDone(esp_lcd_panel_io_handle_t,
                                        esp_lcd_panel_io_event_data_t*,
                                        void* context) {
    Bus_ESP32QSPI* self = static_cast<Bus_ESP32QSPI*>(context);
    BaseType_t highPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(self->_transferReady, &highPriorityTaskWoken);
    return highPriorityTaskWoken == pdTRUE;
}
#endif

bool Bus_ESP32QSPI::acquireTransferBuffer(uint32_t timeoutMs) {
#if SEEED_GFX_HAS_ESP32S3_LCD
    if (!_transferReady) return false;
    return xSemaphoreTake(_transferReady, pdMS_TO_TICKS(timeoutMs)) == pdTRUE;
#else
    (void)timeoutMs;
    return false;
#endif
}

void Bus_ESP32QSPI::releaseTransferBuffer() {
#if SEEED_GFX_HAS_ESP32S3_LCD
    if (_transferReady) xSemaphoreGive(_transferReady);
#endif
}

bool Bus_ESP32QSPI::waitForTransfer(uint32_t timeoutMs) {
    if (!acquireTransferBuffer(timeoutMs)) return false;
    releaseTransferBuffer();
    return true;
}

bool Bus_ESP32QSPI::writeCommandData(uint8_t cmd, const void* data, size_t len) {
#if SEEED_GFX_HAS_ESP32S3_LCD
    if (!_io) return false;
    const int encoded =
        static_cast<int>(kQspiWriteCommand | (static_cast<uint32_t>(cmd) << 8));
    const esp_err_t err = esp_lcd_panel_io_tx_param(_io, encoded, data, len);
    _lastError = err;
    return err == ESP_OK;
#else
    (void)cmd;
    (void)data;
    (void)len;
    return false;
#endif
}

bool Bus_ESP32QSPI::writeColor(uint8_t cmd, const void* data, size_t len) {
#if SEEED_GFX_HAS_ESP32S3_LCD
    if (!_io || !data || len == 0) return false;
    const int encoded =
        static_cast<int>(kQspiWriteColor | (static_cast<uint32_t>(cmd) << 8));
    const esp_err_t err = esp_lcd_panel_io_tx_color(_io, encoded, data, len);
    _lastError = err;
    return err == ESP_OK;
#else
    (void)cmd;
    (void)data;
    (void)len;
    return false;
#endif
}

void Bus_ESP32QSPI::writeCommand(uint8_t cmd) {
    _pendingCommand = cmd;
    (void)writeCommandData(cmd, nullptr, 0);
}

void Bus_ESP32QSPI::writeData(uint8_t data) {
    (void)writeCommandData(_pendingCommand, &data, 1);
}

void Bus_ESP32QSPI::writeData(const uint8_t* data, size_t len) {
    (void)writeCommandData(_pendingCommand, data, len);
}

void Bus_ESP32QSPI::writePixels(const uint16_t*, size_t) {
    // Pixel transfer is performed by Driver_SPD2010, which owns the address
    // window and asynchronous DMA buffer lifetime.
}

void Bus_ESP32QSPI::writeRepeat(uint16_t, size_t) {
    // See writePixels().
}
