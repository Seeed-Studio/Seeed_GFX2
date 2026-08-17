/**
 * @file   Bus_SPI_Platform.h
 * @brief  Platform-specific SPI optimization macros and inlines
 *
 * Ported from TFT_eSPI Processors/ platform-specific code.
 * Provides optimized SPI register access, GPIO pin control,
 * and DMA support for each supported platform.
 *
 * Supported platforms:
 *   - ESP32 / ESP32-S2 / ESP32-S3 / ESP32-C3 / ESP32-C5 / ESP32-C6
 *   - RP2040 / RP2350
 *   - nRF52840 (Seeed XIAO nRF52840)
 *   - SAMD21 (Seeed XIAO SAMD21)
 *   - Generic (AVR, STM32, etc.)
 */

#ifndef SEEED_GFX_BUS_SPI_PLATFORM_H
#define SEEED_GFX_BUS_SPI_PLATFORM_H

#include <Arduino.h>
#include <SPI.h>
#include <new>

// 1. ESP32 Family (ESP32, S2, S3, C3, C6)
#if defined(ESP32)

  #define SEEED_SPI_INSTANCE_OWNED 1

  // ESP-IDF SPI headers for direct register access
  #include "soc/spi_reg.h"
  #include "driver/spi_master.h"
  #include "hal/gpio_ll.h"

  // --- Target-specific defines ---
  #if !defined(CONFIG_IDF_TARGET_ESP32S3) && !defined(CONFIG_IDF_TARGET_ESP32S2) && \
      !defined(CONFIG_IDF_TARGET_ESP32C3) && !defined(CONFIG_IDF_TARGET_ESP32C5) && \
      !defined(CONFIG_IDF_TARGET_ESP32C6) && \
      !defined(CONFIG_IDF_TARGET_ESP32)
    #define CONFIG_IDF_TARGET_ESP32
  #endif

  // Fix IDF problems with ESP32-S3 and ESP32-C3 register naming
  #if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3) || \
      defined(CONFIG_IDF_TARGET_ESP32C5) || defined(CONFIG_IDF_TARGET_ESP32C6)
    #ifndef REG_SPI_BASE
      #define REG_SPI_BASE(i) (((i)>1) ? (DR_REG_SPI3_BASE) : (DR_REG_SPI2_BASE))
    #endif
    #ifndef SPI_MOSI_DLEN_REG
      #define SPI_MOSI_DLEN_REG(x) SPI_MS_DLEN_REG(x)
    #endif
  #endif

  // --- SPI Port Selection ---
  // IMPORTANT: SPIClass() takes Arduino's FSPI/HSPI/VSPI bus number, not
  // ESP-IDF's spi_host_device_t value. On ESP32-S3 core 3.x, for example,
  // Arduino defines FSPI=0 and HSPI=1 while IDF defines SPI2_HOST=1 and
  // SPI3_HOST=2. Passing the IDF value creates the wrong/non-existent bus.
  // ESP32-C3/C5/C6 have one Arduino user SPI bus (FSPI).
  #if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C5) || \
      defined(CONFIG_IDF_TARGET_ESP32C6)
    #define SEEED_SPI_DEFAULT_HOST FSPI
    #define SEEED_SPI_HSPI_HOST    FSPI
  #elif defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)
    #define SEEED_SPI_DEFAULT_HOST FSPI
    #define SEEED_SPI_HSPI_HOST    HSPI
  #else
    // Original ESP32 Arduino bus identifiers.
    #define SEEED_SPI_DEFAULT_HOST VSPI
    #define SEEED_SPI_HSPI_HOST    HSPI
  #endif

  // --- Direct Register SPI Access (for internal optimization) ---
  // These are used only when the SPI port is configured at compile time.
  // The Bus_SPI class uses the Arduino SPI API by default; these are
  // exposed for future ultra-fast paths.

  // --- Fast GPIO Macros for CS/DC (inline functions) ---
  // NOTE: Use direct GPIO register access because the pin values from
  // board definitions are GPIO numbers (e.g., D7=GPIO44 on XIAO ESP32S3 Plus),
  // NOT Arduino pin numbers. digitalWrite() would incorrectly route through
  // the Arduino pin mapping.
  inline void seeed_gpio_lo(uint8_t pin) {
    if (pin >= 32) {
      GPIO.out1_w1tc.val = (1 << (pin - 32));
    } else {
      GPIO.out_w1tc = (1 << pin);
    }
  }

  inline void seeed_gpio_hi(uint8_t pin) {
    if (pin >= 32) {
      GPIO.out1_w1ts.val = (1 << (pin - 32));
    } else {
      GPIO.out_w1ts = (1 << pin);
    }
  }

  // GPIO direction setup using direct register access (avoid Arduino pin mapping)
  inline void seeed_gpio_mode_output(uint8_t pin) {
    gpio_set_direction((gpio_num_t)pin, GPIO_MODE_OUTPUT);
  }

  // --- SPI Initialization Helper ---
  // Convert GPIO number to Arduino pin number for SPIClass::begin().
  // Board definitions return GPIO numbers (e.g., D7=GPIO44 on XIAO ESP32S3 Plus),
  // but SPIClass::begin() expects Arduino pin numbers and calls
  // digitalPinToGPIONumber() internally. We need to reverse-map.
  inline int8_t seeed_gpio_to_arduino_pin(int8_t gpio) {
    if (gpio < 0) return -1;
    for (int8_t i = 0; i < NUM_DIGITAL_PINS; i++) {
      if (digitalPinToGPIONumber(i) == gpio) return i;
    }
    return gpio; // fallback: use directly if no Arduino pin maps to this GPIO
  }

  // Returns a pointer to a new SPIClass for the given host
  inline SPIClass* seeed_spi_create(int8_t host, int8_t sclk, int8_t miso, int8_t mosi, int8_t cs) {
    SPIClass* spi = new (std::nothrow) SPIClass(host);
    if (!spi) return nullptr;
    // Convert GPIO numbers to Arduino pin numbers before passing to begin()
    if (!spi->begin(seeed_gpio_to_arduino_pin(sclk),
                    seeed_gpio_to_arduino_pin(miso),
                    seeed_gpio_to_arduino_pin(mosi),
                    seeed_gpio_to_arduino_pin(cs))) {
      delete spi;
      return nullptr;
    }
    return spi;
  }

  // --- ESP32 SPI Busy Check ---
  #define SEEED_SPI_BUSY_CHECK(port)  while (*(volatile uint32_t*)(SPI_CMD_REG(port)) & SPI_USR)

  // --- ESP32 DMA Support ---
  // Keep DMA conservative on the single-host C3/C5/C6 backends.
  // The current public transfer is synchronous; do not advertise asynchronous
  // DMA until a backend owns a DMA channel and completion state.
  #define SEEED_SPI_DMA_AVAILABLE 0

  // --- SPI Frequency Helper ---
  // ESP32 can go up to 80MHz on SPI (APB clock)
  #define SEEED_SPI_MAX_FREQ 80000000

// 2. RP2040 (Raspberry Pi Pico)
#elif defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_RP2350)

  #define SEEED_SPI_INSTANCE_OWNED 1

  #include "hardware/spi.h"
  #include "hardware/gpio.h"
  #include "hardware/dma.h"
  #include "hardware/pio.h"
  #include "hardware/clocks.h"

  // --- SPI Port Selection ---
  // RP2040 has two SPI peripherals: spi0 and spi1
  #define SEEED_SPI_PORT_0 0
  #define SEEED_SPI_PORT_1 1

  // --- Fast GPIO Macros for CS/DC ---
  // RP2040 SIO registers are the fastest way to toggle GPIO
  #define SEEED_GPIO_LO(pin) sio_hw->gpio_clr = (1ul << (pin))
  #define SEEED_GPIO_HI(pin) sio_hw->gpio_set = (1ul << (pin))

  inline void seeed_gpio_lo(uint8_t pin) { SEEED_GPIO_LO(pin); }
  inline void seeed_gpio_hi(uint8_t pin) { SEEED_GPIO_HI(pin); }

  // --- RP2040 SPI Busy Check ---
  // Wait for tx FIFO to empty, flush rx FIFO, clear overrun
  inline void seeed_spi_busy_check(spi_inst_t* spi_port) {
    while (spi_get_hw(spi_port)->sr & SPI_SSPSR_BSY_BITS) {}
    while (spi_is_readable(spi_port)) (void)spi_get_hw(spi_port)->dr;
    spi_get_hw(spi_port)->icr = SPI_SSPICR_RORIC_BITS;
  }

  // --- RP2040 DMA Support ---
  #define SEEED_SPI_DMA_AVAILABLE 0

  // --- PIO SPI Support ---
  // RP2040 can use PIO for SPI when configured
  #define SEEED_SPI_PIO_AVAILABLE 1

  // --- SPI Frequency Helper ---
  // RP2040 SPI can go up to system clock / 2
  #define SEEED_SPI_MAX_FREQ 62500000

  // --- SPI Pointer Type ---
  // RP2040 community package uses SPIClassRP2040 which wraps spi_inst_t
  // We use the standard Arduino SPI API for portability, but expose
  // the underlying peripheral for direct access when needed

  // --- SPI Initialization Helper ---
  inline SPIClass* seeed_spi_create(int8_t port, int8_t sclk, int8_t miso, int8_t mosi, int8_t cs) {
    SPIClass* spi = (port == 0) ? new (std::nothrow) SPIClass(spi0)
                                : new (std::nothrow) SPIClass(spi1);
    if (!spi) return nullptr;
    spi->begin(sclk, miso, mosi, cs);
    return spi;
  }

// 3. nRF52840 (Seeed XIAO nRF52840)
#elif defined(ARDUINO_ARCH_NRF52840) || defined(ARDUINO_ARCH_NRF52)

  #define SEEED_SPI_INSTANCE_OWNED 0

  // --- Fast GPIO Macros for CS/DC ---
  // nRF52840 uses standard Arduino GPIO (digitalWrite is optimized by the core)
  inline void seeed_gpio_lo(uint8_t pin) { digitalWrite(pin, LOW); }
  inline void seeed_gpio_hi(uint8_t pin) { digitalWrite(pin, HIGH); }

  // --- DMA Support ---
  // nRF52840 has EasyDMA internally, but SPIClass::transfer() is blocking.
  // Do not advertise it as asynchronous DMA through the public API.
  #define SEEED_SPI_DMA_AVAILABLE 0

  // --- SPI Frequency Helper ---
  #define SEEED_SPI_MAX_FREQ 32000000

  // --- SPI Initialization Helper ---
  inline SPIClass* seeed_spi_create(int8_t, int8_t, int8_t, int8_t, int8_t) {
    SPI.begin();
    return &SPI;
  }

// 4. SAMD21 (Seeed XIAO SAMD21)
#elif defined(ARDUINO_ARCH_SAMD)

  #define SEEED_SPI_INSTANCE_OWNED 0

  // --- Fast GPIO Macros for CS/DC ---
  // SAMD21 supports fast register-based GPIO via digitalPinToPort()
  #define SEEED_GPIO_LO(pin) (digitalPinToPort(pin)->OUTCLR.reg = digitalPinToBitMask(pin))
  #define SEEED_GPIO_HI(pin) (digitalPinToPort(pin)->OUTSET.reg = digitalPinToBitMask(pin))

  inline void seeed_gpio_lo(uint8_t pin) { SEEED_GPIO_LO(pin); }
  inline void seeed_gpio_hi(uint8_t pin) { SEEED_GPIO_HI(pin); }

  // --- SPI Busy Check ---
  // SAMD21 SPI library has waitForTransfer()
  #define SEEED_SPI_BUSY_CHECK(spi) (spi).waitForTransfer()

  // --- DMA Support ---
  // SAMD21 DMA available via Adafruit_ZeroDMA
  #define SEEED_SPI_DMA_AVAILABLE 0

  // --- SPI Frequency Helper ---
  #define SEEED_SPI_MAX_FREQ 24000000

  // --- SPI Initialization Helper ---
  inline SPIClass* seeed_spi_create(int8_t, int8_t, int8_t, int8_t, int8_t) {
#if defined(ARDUINO_WIO_TERMINAL) && defined(LCD_SPI)
    // Wio Terminal's built-in ILI9341 is on SPI3 (sercom7); the variant wires
    // LCD_SPI -> SPI3 (Seeed SAMD core declares the SPIClass SPI3 instance).
    // The default SPI instance drives SPI0, whose SCK/MOSI are not connected
    // to the LCD, so init commands never reach the panel -> blank screen.
    LCD_SPI.begin();
    return &LCD_SPI;
#else
    SPI.begin();
    return &SPI;
#endif
  }

// 5. STM32
#elif defined(ARDUINO_ARCH_STM32)

  #define SEEED_SPI_INSTANCE_OWNED 0

  inline void seeed_gpio_lo(uint8_t pin) { digitalWrite(pin, LOW); }
  inline void seeed_gpio_hi(uint8_t pin) { digitalWrite(pin, HIGH); }

  #define SEEED_SPI_DMA_AVAILABLE 0
  #define SEEED_SPI_MAX_FREQ 50000000

  inline SPIClass* seeed_spi_create(int8_t, int8_t, int8_t, int8_t, int8_t) {
    SPI.begin();
    return &SPI;
  }

// 6. Generic (AVR and all other Arduino platforms)
#else

  #define SEEED_SPI_INSTANCE_OWNED 0

  inline void seeed_gpio_lo(uint8_t pin) { digitalWrite(pin, LOW); }
  inline void seeed_gpio_hi(uint8_t pin) { digitalWrite(pin, HIGH); }

  #define SEEED_SPI_DMA_AVAILABLE 0
  #define SEEED_SPI_MAX_FREQ 4000000

  inline SPIClass* seeed_spi_create(int8_t, int8_t, int8_t, int8_t, int8_t) {
    SPI.begin();
    return &SPI;
  }

#endif

// A board package or application may supply a real asynchronous backend
// without patching Bus_SPI.  Both hooks are required so completion state is
// truthful; a blocking SPI loop must never be advertised as DMA.
#if defined(SEEED_SPI_DMA_SUBMIT) && defined(SEEED_SPI_DMA_BUSY)
  #undef SEEED_SPI_DMA_AVAILABLE
  #define SEEED_SPI_DMA_AVAILABLE 1
#endif

// Common SPI write helpers (platform-optimized)

/**
 * Write a single byte via SPI, platform-optimized.
 * Default implementation uses spi->transfer(). Override per platform
 * for direct register access where available.
 */
#ifndef SEEED_SPI_WRITE_BYTE
  #define SEEED_SPI_WRITE_BYTE(spi, data) (spi)->transfer(data)
#endif

/**
 * Write a 16-bit word via SPI, byte-swapped for display endianness.
 * Most color displays expect MSB-first, so we swap bytes:
 *   color 0x1234 -> SPI bytes 0x12, 0x34
 * But the Arduino SPI library sends MSB-first, so transfer16(0x1234)
 * sends 0x12 then 0x34. The display wants the high byte first.
 * For 16-bit color (RGB565), the display expects the pixel in the
 * order: RRRRRGGG GGGBBBBB, which is exactly transfer16() order.
 *
 * For displays that need byte-swapped pixels, use SEEED_SPI_WRITE16_SWAP.
 */
#ifndef SEEED_SPI_WRITE16
  #define SEEED_SPI_WRITE16(spi, data) (spi)->transfer16(data)
#endif

#ifndef SEEED_SPI_WRITE16_SWAP
  #define SEEED_SPI_WRITE16_SWAP(spi, data) (spi)->transfer16(((data) >> 8) | ((data) << 8))
#endif

// SPI transaction helpers (cross-platform)

#if defined(SPI_HAS_TRANSACTION)
  #define SEEED_SPI_BEGIN_TRANS(spi, freq, mode) \
    (spi)->beginTransaction(SPISettings((freq), MSBFIRST, (mode)))
  #define SEEED_SPI_END_TRANS(spi) (spi)->endTransaction()
#else
  #define SEEED_SPI_BEGIN_TRANS(spi, freq, mode) \
    do { (void)(freq); (spi)->setBitOrder(MSBFIRST); (spi)->setDataMode(mode); } while(0)
  #define SEEED_SPI_END_TRANS(spi) ((void)0)
#endif

#endif // SEEED_GFX_BUS_SPI_PLATFORM_H
