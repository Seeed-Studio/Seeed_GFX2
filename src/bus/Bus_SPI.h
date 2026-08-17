/**
 * @file   Bus_SPI.h
 * @brief  SPI bus implementation for Seeed_GFX v2.0
 *
 * Implements IBus using Arduino's SPI library.
 * Handles CS/DC pin control, platform-specific optimizations,
 * and DMA support for ESP32, RP2040, and STM32.
 */

#ifndef SEEED_GFX_BUS_SPI_H
#define SEEED_GFX_BUS_SPI_H

#include <Arduino.h>
#include <SPI.h>
#include "Bus_SPI_Platform.h"
#include "core/Bus.h"

class IBoard;

class Bus_SPI : public IBus {
public:
    // Constructors

    /** Create an SPI bus with explicit pin numbers
     *  @param cs    Chip select pin (-1 if not used)
     *  @param dc    Data/Command pin
     *  @param rst   Reset pin (-1 if not used)
     *  @param mosi  MOSI pin
     *  @param miso  MISO pin (-1 if not used)
     *  @param sclk  Clock pin
     *  @param freq  SPI clock frequency in Hz
     */
    Bus_SPI(int8_t cs, int8_t dc, int8_t rst,
            int8_t mosi, int8_t miso, int8_t sclk,
            uint32_t freq = 40000000, int8_t cs2 = -1);

    /** Create an SPI bus from a board definition */
    Bus_SPI(IBoard& board, uint32_t freq = 40000000);

    virtual ~Bus_SPI();

    // IBus implementation

    bool begin() override;
    void end() override;

    void beginWrite() override;
    void endWrite() override;

    void writeCommand(uint8_t cmd) override;
    void writeCommand16(uint16_t cmd) override;
    void writeData(uint8_t data) override;
    void writeData16(uint16_t data) override;
    void writeData(const uint8_t* data, size_t len) override;

    void writePixels(const uint16_t* data, size_t len) override;
    void writeRepeat(uint16_t pixel, size_t len) override;

    void beginRead() override;
    void endRead() override;
    uint8_t readData() override;

    void setFrequency(uint32_t freq) override;
    uint32_t frequency() const override { return _freqWrite; }

    bool supportsDMA() const override;
    bool enableDMA(bool enable = true) override;
    bool writePixelsDMA(const uint16_t* data, size_t len) override;
    bool dmaBusy() override;
    BusCapabilities capabilities() const override;
    bool supportsSecondaryChipSelect() const override { return _cs2 >= 0; }
    void selectChip(ChipSelectTarget target) override;

    // Configuration

    /** Set the SPI read frequency */
    void setReadFrequency(uint32_t freq) { _freqRead = freq; }

    /** Inspect configured read frequency (primarily for diagnostics/tests). */
    uint32_t readFrequency() const { return _freqRead; }

    /** Set the SPI mode */
    void setSPIMode(uint8_t mode) { _spiMode = mode; }

    /** Use HSPI port (ESP32 only) */
    void useHSPI(bool use = true) { _useHSPI = use; }

    /** Return whether the secondary SPI host was requested. */
    bool usesSecondaryHost() const { return _useHSPI; }

    /** Initialized Arduino SPI instance for devices sharing this bus. */
    SPIClass* spiInstance() const { return _spi; }

private:
#if defined(ARDUINO_ARCH_NRF52840) || defined(ARDUINO_ARCH_NRF52)
    // EasyDMA can only read RAM. Keep a small, persistent staging buffer so
    // flash-backed tables are never submitted directly and no large buffer is
    // placed on the nRF main stack.
    enum { NRF_TX_SCRATCH_BYTES = 256 };
    alignas(4) uint8_t _nrfTxScratch[NRF_TX_SCRATCH_BYTES];
#endif

    // Pins
    int8_t _cs   = -1;
    int8_t _cs2  = -1;
    int8_t _dc   = -1;
    int8_t _rst  = -1;
    int8_t _mosi = -1;
    int8_t _miso = -1;
    int8_t _sclk = -1;

    // Frequencies
    uint32_t _freqWrite = 40000000;
    uint32_t _freqRead  = 20000000;

    // SPI configuration
    uint8_t  _spiMode = SPI_MODE0;
    bool     _useHSPI = false;
    bool     _locked = true;
    ChipSelectTarget _chipTarget = ChipSelectTarget::Primary;

    // SPI instance
    SPIClass* _spi = nullptr;

    // DMA
    bool     _dmaEnabled = false;

    // Helper methods
    void _initPins();
    void _csLow();
    void _csHigh();
    void _dcLow();
    void _dcHigh();
    void _rstLow();
    void _rstHigh();
    void _hardwareReset();
    void _spiWait();
#if defined(ARDUINO_ARCH_NRF52840) || defined(ARDUINO_ARCH_NRF52)
    void _writeNrfBytes(const uint8_t* data, size_t len);
#endif
};

#endif // SEEED_GFX_BUS_SPI_H
