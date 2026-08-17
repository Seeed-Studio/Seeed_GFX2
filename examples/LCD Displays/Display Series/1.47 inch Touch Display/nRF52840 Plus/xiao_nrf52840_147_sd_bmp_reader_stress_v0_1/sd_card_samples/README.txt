SD Card Sample BMP Images
=========================

These sample BMP images are for use with the
xiao_nrf52840_147_sd_bmp_reader_stress_v0_1 sketch.

How to use:
  1. Format a microSD card as FAT32 (MBR partition table recommended).
  2. Copy the .bmp files from THIS folder to the ROOT directory or /img on the
     SD card. The sketch scans both locations for *.bmp.
  3. Insert the SD card into the XIAO 1.47 inch Touch Display's SD slot.
  4. Upload the sketch and open the Serial Monitor at 115200 baud.

Included files:
  1.bmp        (165174 bytes)
  2.bmp        (165174 bytes)
  image001.bmp (165174 bytes)
  image002.bmp (165174 bytes)
  test.bmp     (117814 bytes)

Supported BMP formats (per the sketch's custom decoder):
  - 16-bit BI_RGB (standard RGB555).
  - 16/32-bit BI_BITFIELDS (including RGB565 masks).
  - 24/32-bit BI_RGB (uncompressed).
  - Recommended resolution: 172x320 for exact full-screen display.
  - Larger images are center-cropped without a fixed source-width limit.

Note:
  The sketch re-reads every frame in stress mode (STRESS_READ_EVERY_FRAME = true)
  to validate SD card + high-frequency LCD refresh coexistence. For a gentler
  slideshow, set STRESS_READ_EVERY_FRAME = false in the .ino.
