# reTerminal E1004 SD-card slideshow

This example shows photos from the microSD card on the E1004 13.3-inch Spectra 6 ePaper display. It reuses the E1004 SD-image decoder and Seeed_GFX2 E6 dithering path.

The images remain ordinary files on the card: the example decodes them and calls the library's E6 dithering API at runtime, so it does not duplicate the conversion algorithm or require generated C arrays. The repository's `bmp2array4bit` utility is for embedding sprites in firmware, not for this SD-card workflow.

## Hardware and controls

- Board: reTerminal E1004 (ESP32-S3, 8 MB PSRAM); enable **OPI PSRAM** in the board options.
- Display: 1200 × 1600 T133A01 Spectra 6. The display and SD card share SPI.
- Rear keys, as named in the published E1004 schematic:
  - **KEY0 / GPIO3**: refresh the current image.
  - **KEY1 / GPIO4**: show the next image.
  - **KEY2 / GPIO5**: show the previous image.
- A scheduled wake advances to the next image after six hours. Manual key actions also start a new six-hour interval.
- The ESP32-S3 enters deep sleep after each display update. The ePaper retains the image without continuous refresh.

The E1004 schematic labels the rear buttons `ESP_IO3/KEY0`, `ESP_IO4/KEY1`, and `ESP_IO5/KEY2`. Some files in the separate OSHW firmware repository use a different key-pin mapping; this example follows the schematic nets.

The capacitive front-bezel buttons are **not supported by this example**. Seeed_GFX2 and the E1004 board class in the available firmware sources do not expose their controller, pin/interface mapping, or wake API. Do not assign GPIOs to those controls without verified hardware/API documentation.

## SD card

Format the microSD card as **FAT32** (the reTerminal E Series supports cards up to 32 GB). Create `/img/` and copy supported images there:

- JPEG/JPG: baseline, 8-bit, YCbCr or grayscale
- BMP: uncompressed 24-bit BGR or 4-bit indexed
- PNG: standard PNG color types; alpha is composited on white

Images in `/img/` are shown in case-insensitive filename order. If `/img/` contains no supported images, the card root is scanned.

The 1200 × 1600 RGB decode can use several megabytes of PSRAM. If a large image cannot be decoded, resize it on a computer before copying it to the card; the SD-image decoder and E6 conversion path are reused from the existing Color6 example.

## Build and flash

1. Install Seeed_GFX2 as an Arduino library and open `reTerminal_E1004_SDcard_Slideshow.ino`.
2. Select the **XIAO ESP32S3** board package/target used by the E1004 examples, enable **OPI PSRAM**, and use the 8 MB flash setting documented by the official E-Series deep-sleep example.
3. Connect the E1004 over USB-C, select its serial port, and upload the sketch. Debug output uses UART1 on GPIO43/44 at 115200 baud.
4. Insert the prepared microSD card and power on. The first image is shown immediately; subsequent automatic changes occur every six hours.

The E1004 is powered by an ESP32-S3, so the build/flash target is the ESP32-S3 Arduino platform, not a Linux/Raspberry Pi image.
