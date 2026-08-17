#include "Bus_ESP32RGB.h"
#include <string.h>

#if SEEED_GFX_HAS_ESP32S3_LCD
#include "esp_cache.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace {
const char* rgbPanelCreateError(esp_err_t error) {
    switch (error) {
        case ESP_ERR_NO_MEM:
            return "ESP32 RGB panel creation failed: out of memory";
        case ESP_ERR_INVALID_ARG:
            return "ESP32 RGB panel creation failed: invalid configuration";
        case ESP_ERR_NOT_FOUND:
            return "ESP32 RGB panel creation failed: RGB peripheral unavailable";
        default:
            return "ESP32 RGB panel creation failed";
    }
}
} // namespace
#endif

Bus_ESP32RGB::Bus_ESP32RGB(const Esp32RgbLcdConfig& config)
    : _config(config), _lastError(0), _lastErrorMessage(nullptr),
      _begun(false), _framebuffer(nullptr)
#if SEEED_GFX_HAS_ESP32S3_LCD
    , _panel(nullptr), _frameReadySemaphore(nullptr),
      _framebuffers{nullptr, nullptr}, _framebufferCount(0),
      _frontBufferIndex(0), _writeDepth(0), _transactionDirty(false)
#endif
{}

Bus_ESP32RGB::~Bus_ESP32RGB() {
    end();
}

bool Bus_ESP32RGB::begin() {
    if (_begun) return true;
#if SEEED_GFX_HAS_ESP32S3_LCD
    _lastError = ESP_OK;
    _lastErrorMessage = nullptr;

    esp_lcd_rgb_panel_config_t panelConfig = {};
    panelConfig.clk_src = LCD_CLK_SRC_PLL160M;
    panelConfig.timings.pclk_hz = _config.frequency;
    panelConfig.timings.h_res = _config.width;
    panelConfig.timings.v_res = _config.height;
    panelConfig.timings.hsync_pulse_width = _config.hsyncPulseWidth;
    panelConfig.timings.hsync_back_porch = _config.hsyncBackPorch;
    panelConfig.timings.hsync_front_porch = _config.hsyncFrontPorch;
    panelConfig.timings.vsync_pulse_width = _config.vsyncPulseWidth;
    panelConfig.timings.vsync_back_porch = _config.vsyncBackPorch;
    panelConfig.timings.vsync_front_porch = _config.vsyncFrontPorch;
    panelConfig.timings.flags.pclk_active_neg = _config.pclkActiveNegative;
    panelConfig.data_width = 16;
    panelConfig.bits_per_pixel = 16;
    _framebufferCount = _config.frameBufferCount >= 2 ? 2 : 1;
    panelConfig.num_fbs = _framebufferCount;
    panelConfig.bounce_buffer_size_px =
        static_cast<size_t>(_config.width) * _config.bounceBufferLines;
    panelConfig.hsync_gpio_num = _config.hsync;
    panelConfig.vsync_gpio_num = _config.vsync;
    panelConfig.de_gpio_num = _config.de;
    panelConfig.pclk_gpio_num = _config.pclk;
    panelConfig.disp_gpio_num = -1;
    for (uint8_t i = 0; i < 16; ++i) {
        panelConfig.data_gpio_nums[i] = _config.data[i];
    }
    panelConfig.flags.fb_in_psram = true;

    // Arduino_GFX initializes the ST7701 command interface before starting
    // the continuously refreshing RGB peripheral. Keeping that order is
    // important on Indicator: LCD CS is on the I2C expander and I2C writes can
    // otherwise stall once the RGB engine is already consuming PSRAM.
    esp_err_t err = ESP_OK;
    if (_config.panelInit &&
        !_config.panelInit(_config.panelInitContext)) {
        err = ESP_FAIL;
        _lastErrorMessage =
            "SenseCAP Indicator LCD controller initialization failed";
    }
    if (err == ESP_OK) {
        err = esp_lcd_new_rgb_panel(&panelConfig, &_panel);
    }
    if (err != ESP_OK) {
        if (!_lastErrorMessage) {
            _lastErrorMessage = rgbPanelCreateError(err);
        }
    }
    if (err == ESP_OK) {
        if (_framebufferCount == 2) {
            err = esp_lcd_rgb_panel_get_frame_buffer(
                _panel, 2,
                reinterpret_cast<void**>(&_framebuffers[0]),
                reinterpret_cast<void**>(&_framebuffers[1]));
        } else {
            err = esp_lcd_rgb_panel_get_frame_buffer(
                _panel, 1,
                reinterpret_cast<void**>(&_framebuffers[0]));
        }
        if (err != ESP_OK || !_framebuffers[0] ||
            (_framebufferCount == 2 && !_framebuffers[1])) {
            if (err == ESP_OK) err = ESP_FAIL;
            _lastErrorMessage = "ESP32 RGB framebuffer access failed";
        }
    }
    if (err == ESP_OK) {
        const size_t frameBytes =
            static_cast<size_t>(_config.width) * _config.height * 2U;
        for (uint8_t i = 0; i < _framebufferCount; ++i) {
            memset(_framebuffers[i], 0, frameBytes);
            if (!syncFramebuffer(_framebuffers[i], frameBytes)) {
                err = static_cast<esp_err_t>(_lastError);
                break;
            }
        }
        _frontBufferIndex = 0;
        _framebuffer = _framebuffers[0];
    }
    if (err == ESP_OK && _framebufferCount == 2) {
        _frameReadySemaphore = xSemaphoreCreateBinary();
        if (!_frameReadySemaphore) {
            err = ESP_ERR_NO_MEM;
            _lastErrorMessage =
                "ESP32 RGB frame synchronization allocation failed";
        } else {
            esp_lcd_rgb_panel_event_callbacks_t callbacks = {};
            callbacks.on_frame_buf_complete =
                &Bus_ESP32RGB::onFrameBufferComplete;
            err = esp_lcd_rgb_panel_register_event_callbacks(
                _panel, &callbacks, this);
            if (err != ESP_OK) {
                _lastErrorMessage =
                    "ESP32 RGB frame callback registration failed";
            }
        }
    }
    if (err == ESP_OK) {
        err = esp_lcd_panel_reset(_panel);
        if (err != ESP_OK) {
            _lastErrorMessage = "ESP32 RGB panel reset failed";
        }
    }
    if (err == ESP_OK) {
        err = esp_lcd_panel_init(_panel);
        if (err != ESP_OK) {
            _lastErrorMessage = "ESP32 RGB panel initialization failed";
        }
    }
    if (err != ESP_OK) {
        _lastError = err;
        if (_panel) {
            (void)esp_lcd_panel_del(_panel);
            _panel = nullptr;
        }
        if (_frameReadySemaphore) {
            vSemaphoreDelete(
                static_cast<SemaphoreHandle_t>(_frameReadySemaphore));
            _frameReadySemaphore = nullptr;
        }
        _framebuffers[0] = _framebuffers[1] = nullptr;
        _framebufferCount = 0;
        _framebuffer = nullptr;
        return false;
    }
    _lastError = ESP_OK;
    _lastErrorMessage = nullptr;
    _begun = true;
    return true;
#else
    _lastError = -1;
    _lastErrorMessage = "ESP32-S3 RGB LCD peripheral is not available";
    return false;
#endif
}

void Bus_ESP32RGB::beginWrite() {
#if SEEED_GFX_HAS_ESP32S3_LCD
    if (!_begun || _framebufferCount < 2) return;
    if (_writeDepth++ != 0) return;

    const uint8_t drawIndex = static_cast<uint8_t>(_frontBufferIndex ^ 1U);
    const size_t frameBytes =
        static_cast<size_t>(_config.width) * _config.height * 2U;
    memcpy(_framebuffers[drawIndex], _framebuffers[_frontBufferIndex],
           frameBytes);
    _framebuffer = _framebuffers[drawIndex];
    _transactionDirty = false;
#endif
}

void Bus_ESP32RGB::endWrite() {
#if SEEED_GFX_HAS_ESP32S3_LCD
    if (!_begun || _framebufferCount < 2 || _writeDepth == 0) return;
    if (--_writeDepth != 0) return;

    const uint8_t drawIndex = static_cast<uint8_t>(_frontBufferIndex ^ 1U);
    if (!_transactionDirty) {
        _framebuffer = _framebuffers[_frontBufferIndex];
        return;
    }

    const size_t frameBytes =
        static_cast<size_t>(_config.width) * _config.height * 2U;
    if (!syncFramebuffer(_framebuffers[drawIndex], frameBytes)) {
        _framebuffer = _framebuffers[_frontBufferIndex];
        _transactionDirty = false;
        return;
    }

    SemaphoreHandle_t frameReady =
        static_cast<SemaphoreHandle_t>(_frameReadySemaphore);
    while (frameReady && xSemaphoreTake(frameReady, 0) == pdTRUE) {}

    const esp_err_t err = esp_lcd_panel_draw_bitmap(
        _panel, 0, 0, _config.width, _config.height,
        _framebuffers[drawIndex]);
    if (err != ESP_OK) {
        _lastError = err;
        _lastErrorMessage = "ESP32 RGB framebuffer switch failed";
        _framebuffer = _framebuffers[_frontBufferIndex];
        _transactionDirty = false;
        return;
    }

    // The completion callback means the previously scanned framebuffer is
    // safe to reuse. Waiting here prevents the next UI frame from modifying
    // a buffer while RGB DMA is still reading it.
    if (!frameReady ||
        xSemaphoreTake(frameReady, pdMS_TO_TICKS(100)) != pdTRUE) {
        _lastError = ESP_ERR_TIMEOUT;
        _lastErrorMessage = "ESP32 RGB framebuffer switch timed out";
    } else {
        _lastError = ESP_OK;
        _lastErrorMessage = nullptr;
    }
    _frontBufferIndex = drawIndex;
    _framebuffer = _framebuffers[_frontBufferIndex];
    _transactionDirty = false;
#endif
}

bool Bus_ESP32RGB::flushFramebuffer(const void* address, size_t length) {
#if SEEED_GFX_HAS_ESP32S3_LCD
    if (!address || length == 0) return true;
    if (_framebufferCount == 2 && _writeDepth != 0) {
        _transactionDirty = true;
        return true;
    }
    return syncFramebuffer(address, length);
#else
    (void)address;
    (void)length;
    _lastError = -1;
    _lastErrorMessage = "ESP32-S3 RGB LCD peripheral is not available";
    return false;
#endif
}

#if SEEED_GFX_HAS_ESP32S3_LCD
bool Bus_ESP32RGB::syncFramebuffer(const void* address, size_t length) {
    if (!address || length == 0) return true;
    const esp_err_t err = esp_cache_msync(
        const_cast<void*>(address), length,
        ESP_CACHE_MSYNC_FLAG_DIR_C2M |
        ESP_CACHE_MSYNC_FLAG_TYPE_DATA |
        ESP_CACHE_MSYNC_FLAG_UNALIGNED);
    _lastError = err;
    _lastErrorMessage = err == ESP_OK
        ? nullptr : "ESP32 RGB framebuffer cache write-back failed";
    return err == ESP_OK;
}

bool Bus_ESP32RGB::onFrameBufferComplete(
    esp_lcd_panel_handle_t, const esp_lcd_rgb_panel_event_data_t*,
    void* context) {
    Bus_ESP32RGB* self = static_cast<Bus_ESP32RGB*>(context);
    if (!self || !self->_frameReadySemaphore) return false;
    BaseType_t taskWoken = pdFALSE;
    xSemaphoreGiveFromISR(
        static_cast<SemaphoreHandle_t>(self->_frameReadySemaphore),
        &taskWoken);
    return taskWoken == pdTRUE;
}
#endif

void Bus_ESP32RGB::end() {
#if SEEED_GFX_HAS_ESP32S3_LCD
    if (_panel) {
        // A generic ESP RGB panel without disp_gpio_num reports
        // ESP_ERR_NOT_SUPPORTED here. The return value is intentionally
        // ignored because deleting the panel is the actual shutdown step.
        (void)esp_lcd_panel_disp_on_off(_panel, false);
        (void)esp_lcd_panel_del(_panel);
        _panel = nullptr;
    }
    if (_frameReadySemaphore) {
        vSemaphoreDelete(
            static_cast<SemaphoreHandle_t>(_frameReadySemaphore));
        _frameReadySemaphore = nullptr;
    }
    _framebuffers[0] = _framebuffers[1] = nullptr;
    _framebufferCount = 0;
    _writeDepth = 0;
    _transactionDirty = false;
#endif
    _framebuffer = nullptr;
    _begun = false;
}

void Bus_ESP32RGB::setFrequency(uint32_t freq) {
    _config.frequency = freq;
#if SEEED_GFX_HAS_ESP32S3_LCD
    if (_panel) {
        const esp_err_t err = esp_lcd_rgb_panel_set_pclk(_panel, freq);
        _lastError = err;
        _lastErrorMessage = err == ESP_OK
            ? nullptr : "ESP32 RGB pixel clock update failed";
    }
#endif
}

bool Bus_ESP32RGB::setDisplayEnabled(bool enabled) {
#if SEEED_GFX_HAS_ESP32S3_LCD
    if (!_panel) return false;
    if (_config.panelSetEnabled &&
        !_config.panelSetEnabled(_config.panelInitContext, enabled)) {
        _lastError = ESP_FAIL;
        _lastErrorMessage =
            "RGB panel controller enable state change failed";
        return false;
    }
    esp_err_t err = esp_lcd_panel_disp_on_off(_panel, enabled);
    if (err == ESP_ERR_NOT_SUPPORTED) {
        // SenseCAP Indicator has no RGB DISP_EN GPIO. esp_lcd_panel_init()
        // already starts continuous transmission, while Panel_TFT controls
        // the visible on/off state through GPIO45 backlight. On wake, request
        // an RGB DMA restart to recover synchronization if it was disturbed.
        err = enabled ? esp_lcd_rgb_panel_restart(_panel) : ESP_OK;
    }
    _lastError = err;
    _lastErrorMessage = err == ESP_OK
        ? nullptr : "ESP32 RGB display enable state change failed";
    return err == ESP_OK;
#else
    (void)enabled;
    _lastError = -1;
    _lastErrorMessage = "ESP32-S3 RGB LCD peripheral is not available";
    return false;
#endif
}
