/**
 * @file   Bus_SPI.cpp
 * @brief  SPI bus implementation for Seeed_GFX v2.0
 *
 * Adapted from TFT_eSPI.cpp SPI transaction patterns and
 * Processors/ platform-specific code.
 * Platform optimizations are in Bus_SPI_Platform.h.
 */

#include "Bus_SPI.h"
#include "../core/Board.h"
#include "../core/Gpio.h"

// Constructor

Bus_SPI::Bus_SPI(int8_t cs, int8_t dc, int8_t rst,
                 int8_t mosi, int8_t miso, int8_t sclk,
                 uint32_t freq, int8_t cs2)
    : _cs(cs), _cs2(cs2), _dc(dc), _rst(rst)
    , _mosi(mosi), _miso(miso), _sclk(sclk)
    , _freqWrite(freq)
{
    // Default SPI mode: most ST7789 boards need MODE3
    _spiMode = SPI_MODE0;
}

Bus_SPI::Bus_SPI(IBoard& board, uint32_t freq)
    : _cs(board.pinCS()), _cs2(board.pinCS2())
    , _dc(board.pinDC()), _rst(board.pinRST())
    , _mosi(board.pinMOSI()), _miso(board.pinMISO()), _sclk(board.pinSCLK())
    , _freqWrite(freq)
{
    _spiMode = SPI_MODE0;
}

Bus_SPI::~Bus_SPI() {
    end();
}

// IBus implementation

bool Bus_SPI::begin() {
    if (_spi) return true;
    _initPins();
    _hardwareReset();

    // Initialize SPI with platform-specific port selection
#if defined(ESP32)
    if (_useHSPI) {
        _spi = seeed_spi_create(SEEED_SPI_HSPI_HOST, _sclk, _miso, _mosi, _cs);
    } else {
        _spi = seeed_spi_create(SEEED_SPI_DEFAULT_HOST, _sclk, _miso, _mosi, _cs);
    }
#elif defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_RP2350)
    _spi = seeed_spi_create(_useHSPI ? SEEED_SPI_PORT_1 : SEEED_SPI_PORT_0,
                           _sclk, _miso, _mosi, _cs);
#else
    _spi = seeed_spi_create(0, _sclk, _miso, _mosi, _cs);
#endif

    if (!_spi) return false;

    _locked = true;
    return true;
}

void Bus_SPI::end() {
    if (_spi) {
        if (!_locked) endWrite();
        _spi->end();
#if SEEED_SPI_INSTANCE_OWNED
        delete _spi;
#endif
        _spi = nullptr;
    }
    _locked = true;
    _dmaEnabled = false;
}

// Write transaction control

void Bus_SPI::beginWrite() {
    if (_spi && _locked) {
        _locked = false;
        SEEED_SPI_BEGIN_TRANS(_spi, _freqWrite, _spiMode);
        _csLow();
    }
}

void Bus_SPI::endWrite() {
    if (_spi && !_locked) {
        _locked = true;
        _spiWait();
        _csHigh();
        SEEED_SPI_END_TRANS(_spi);
    }
}

// Command write

void Bus_SPI::writeCommand(uint8_t cmd) {
    if (!_spi) return;
    const bool standalone = _locked;
    if (standalone) beginWrite();
    _dcLow();
    _spi->transfer(cmd);
    _dcHigh();
    if (standalone) endWrite();
}

void Bus_SPI::writeCommand16(uint16_t cmd) {
    if (!_spi) return;
    const bool standalone = _locked;
    if (standalone) beginWrite();
    _dcLow();
    _spi->transfer16(cmd);
    _dcHigh();
    if (standalone) endWrite();
}

// Data write

void Bus_SPI::writeData(uint8_t data) {
    if (!_spi) return;
    const bool standalone = _locked;
    if (standalone) beginWrite();
    _dcHigh();
    _spi->transfer(data);
    if (standalone) endWrite();
}

void Bus_SPI::writeData16(uint16_t data) {
    if (!_spi) return;
    const bool standalone = _locked;
    if (standalone) beginWrite();
    _dcHigh();
    _spi->transfer16(data);
    if (standalone) endWrite();
}

void Bus_SPI::writeData(const uint8_t* data, size_t len) {
    if (!_spi || !data || len == 0) return;
    const bool standalone = _locked;
    if (standalone) beginWrite();
    _dcHigh();
#if defined(ARDUINO_ARCH_NRF52840) || defined(ARDUINO_ARCH_NRF52)
    _writeNrfBytes(data, len);
#else
    while (len--) _spi->transfer(*data++);
#endif
    if (standalone) endWrite();
}

// Pixel write

void Bus_SPI::writePixels(const uint16_t* data, size_t len) {
    if (!_spi || !data || len == 0) return;
    const bool standalone = _locked;
    if (standalone) beginWrite();
    _dcHigh();
#if defined(ARDUINO_ARCH_NRF52840) || defined(ARDUINO_ARCH_NRF52)
    while (len) {
        const size_t pixelsThisBlock =
            min(len, NRF_TX_SCRATCH_BYTES / (size_t)2);
        for (size_t i = 0; i < pixelsThisBlock; ++i) {
            const uint16_t pixel = data[i];
            _nrfTxScratch[i * 2] = (uint8_t)(pixel >> 8);
            _nrfTxScratch[i * 2 + 1] = (uint8_t)pixel;
        }
        _spi->transfer(_nrfTxScratch, nullptr, pixelsThisBlock * 2);
        data += pixelsThisBlock;
        len -= pixelsThisBlock;
    }
#else
    for (size_t i = 0; i < len; ++i) SEEED_SPI_WRITE16(_spi, data[i]);
#endif
    if (standalone) endWrite();
}

void Bus_SPI::writeRepeat(uint16_t pixel, size_t len) {
    if (!_spi || len == 0) return;
    const bool standalone = _locked;
    if (standalone) beginWrite();
    _dcHigh();
    // Use platform-specific 16-bit write for speed
    // Split pixel into bytes: MSB first (standard SPI order)
    uint8_t hi = (pixel >> 8) & 0xFF;
    uint8_t lo = pixel & 0xFF;
#if defined(ARDUINO_ARCH_NRF52840) || defined(ARDUINO_ARCH_NRF52)
    const size_t scratchPixels = NRF_TX_SCRATCH_BYTES / 2;
    for (size_t i = 0; i < scratchPixels; ++i) {
        _nrfTxScratch[i * 2] = hi;
        _nrfTxScratch[i * 2 + 1] = lo;
    }
    while (len) {
        const size_t pixelsThisBlock = min(len, scratchPixels);
        _spi->transfer(_nrfTxScratch, nullptr, pixelsThisBlock * 2);
        len -= pixelsThisBlock;
    }
#else
    for (size_t i = 0; i < len; i++) {
        SEEED_SPI_WRITE_BYTE(_spi, hi);
        SEEED_SPI_WRITE_BYTE(_spi, lo);
    }
#endif
    if (standalone) endWrite();
}

// Read operations

void Bus_SPI::beginRead() {
    if (!_spi) return;
    _spiWait(); // Wait for any DMA to complete
    if (_locked) {
        _locked = false;
        SEEED_SPI_BEGIN_TRANS(_spi, _freqRead, _spiMode);
        _csLow();
    }
}

void Bus_SPI::endRead() {
    if (_spi && !_locked) {
        _locked = true;
        _csHigh();
        SEEED_SPI_END_TRANS(_spi);
    }
}

uint8_t Bus_SPI::readData() {
    return _spi ? _spi->transfer(0x00) : 0;
}

// Configuration

void Bus_SPI::setFrequency(uint32_t freq) {
    _freqWrite = freq;
}

// DMA support

bool Bus_SPI::supportsDMA() const {
    return SEEED_SPI_DMA_AVAILABLE != 0;
}

bool Bus_SPI::enableDMA(bool enable) {
    _dmaEnabled = enable && supportsDMA();
    return _dmaEnabled;
}

BusCapabilities Bus_SPI::capabilities() const {
    BusCapabilities caps;
    caps.readable = _miso >= 0;
    caps.dma = supportsDMA();
    caps.asyncPixelTransfer = supportsDMA();
    caps.hardwareTransfer = supportsDMA();
    caps.parallel = false;
    caps.secondaryChipSelect = supportsSecondaryChipSelect();
    caps.maxTransferBytes = maxTransferSize();
    return caps;
}

void Bus_SPI::selectChip(ChipSelectTarget target) {
    // Selection changes are only safe between transactions.
    if (!_locked) endWrite();
    _chipTarget = target;
}

bool Bus_SPI::writePixelsDMA(const uint16_t* data, size_t len) {
    // Never claim a blocking SPI loop is DMA. A platform backend must expose
    // a real queued transfer before this function is allowed to succeed.
    if (!_dmaEnabled || !supportsDMA() || !data || len == 0 || !_spi) return false;
    const bool standalone = _locked;
    if (standalone) beginWrite();
    _dcHigh();

#if defined(SEEED_SPI_DMA_SUBMIT)
    const bool submitted = SEEED_SPI_DMA_SUBMIT(_spi, data, len, _chipTarget);
    if (!submitted && standalone) endWrite();
    return submitted;
#else
    if (standalone) endWrite();
    return false;
#endif
}

bool Bus_SPI::dmaBusy() {
#if defined(SEEED_SPI_DMA_BUSY)
    return _dmaEnabled && _spi && SEEED_SPI_DMA_BUSY(_spi);
#else
    return false;
#endif
}

// Private helpers

void Bus_SPI::_initPins() {
    if (_cs >= 0) {
        gfxPinModeOutput(_cs);
        seeed_gpio_hi(_cs);
    }
    if (_cs2 >= 0) {
        gfxPinModeOutput(_cs2);
        seeed_gpio_hi(_cs2);
    }
    if (_dc >= 0) {
        gfxPinModeOutput(_dc);
        seeed_gpio_hi(_dc);
    }
    if (_rst >= 0) {
        gfxPinModeOutput(_rst);
    }
}

void Bus_SPI::_csLow() {
    if ((_chipTarget == ChipSelectTarget::Primary ||
         _chipTarget == ChipSelectTarget::Both) && _cs >= 0) {
        seeed_gpio_lo(_cs);
    }
    if ((_chipTarget == ChipSelectTarget::Secondary ||
         _chipTarget == ChipSelectTarget::Both) && _cs2 >= 0) {
        seeed_gpio_lo(_cs2);
    }
}

void Bus_SPI::_csHigh() {
    if (_cs >= 0) {
        seeed_gpio_hi(_cs);
    }
    if (_cs2 >= 0) {
        seeed_gpio_hi(_cs2);
    }
}

void Bus_SPI::_dcLow() {
    if (_dc >= 0) {
        seeed_gpio_lo(_dc);
    }
}

void Bus_SPI::_dcHigh() {
    if (_dc >= 0) {
        seeed_gpio_hi(_dc);
    }
}

void Bus_SPI::_rstLow() {
    if (_rst >= 0) {
        seeed_gpio_lo(_rst);
    }
}

void Bus_SPI::_rstHigh() {
    if (_rst >= 0) {
        seeed_gpio_hi(_rst);
    }
}

void Bus_SPI::_hardwareReset() {
    if (_rst >= 0) {
        _rstHigh();
        delay(5);
        _rstLow();
        delay(20);
        _rstHigh();
        delay(150);
    }
}

void Bus_SPI::_spiWait() {
    // Wait for any pending SPI transfer to complete
    // Platform-specific busy check
#if defined(ESP32)
    // ESP32: SPI hardware manages this internally for Arduino SPI API.
    // For direct register access, would use SEEED_SPI_BUSY_CHECK(port).
    // The Arduino beginTransaction/endTransaction pattern handles this.
#elif defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_RP2350)
    // RP2040: flush TX FIFO and clear RX overrun
    // This is handled by the SPI library, but we ensure it here
    if (_spi) {
        // The Arduino SPI wrapper handles this in endTransaction
    }
#elif defined(ARDUINO_ARCH_NRF52840) || defined(ARDUINO_ARCH_NRF52)
    // The Seeed/Adafruit nRF52 SPIClass initializes nrfx_spim without an
    // event handler, so its transfer() overloads are blocking. There is no
    // waitForTransfer() method in that core.
#elif defined(ARDUINO_ARCH_SAMD)
    // SAMD21: wait for transfer complete
    if (_spi) {
        _spi->waitForTransfer();
    }
#endif
}

#if defined(ARDUINO_ARCH_NRF52840) || defined(ARDUINO_ARCH_NRF52)
void Bus_SPI::_writeNrfBytes(const uint8_t* data, size_t len) {
    while (len) {
        const size_t block = min(len, (size_t)NRF_TX_SCRATCH_BYTES);
        // The caller may point into flash (init tables/fonts), while nRF
        // EasyDMA accepts RAM only. Always stage the bytes before transfer.
        memcpy(_nrfTxScratch, data, block);
        _spi->transfer(_nrfTxScratch, nullptr, block);
        data += block;
        len -= block;
    }
}
#endif
