# ChangeLog

## v0.4.3 (2026-06-24)

* Added missing `esp32c61` and `esp32h4` entries to the supported targets list in the component manifest.

## v0.4.2 (2026-06-09)

* Add SQOI (split QOI) format support for LVGL v9 with RGBA-to-BGRA conversion

## v0.4.1 (2026-06-05)

* Add `CONFIG_ESP_LV_DECODER_PJPG_HW_ALPHA_BLEND` (LVGL v9): decode transparent PJPG images to ARGB8888 on RGB888 displays so per-pixel alpha blending can run on the PPA hardware instead of the CPU. RGB565 displays keep using RGB565A8.

## v0.4.0 (2026-05-06)

* Added ESP32-S31 to the supported targets list.

## v0.3.5 (2026-03-13)

* Fixed PNG decoder not using LVGL image cache by removing the erroneous `no_cache` flag, making PNG consistent with JPEG and QOI cache behavior.

## v0.3.4 (2026-02-26)

* Used `jpeg_decoder_get_info` from `esp_driver_jpeg` instead of the software `jpeg_decode_header` for JPEG header parsing in the hardware decode path, eliminating the mixing of software and hardware JPEG interfaces.

## v0.3.3 (2026-01-22)

* Fixed missing `esp_mm` and `esp_driver_jpeg` dependency declarations in CMakeLists.txt which caused build failures when using the component from the component registry.

## v0.3.2 (2025-11-17)

* Fix the issue where the buffer space is insufficient when reading the image header.

## v0.3.1 (2025-10-21)

* Added mutex protection for the PJPG global buffer.
* Added boundary checks to prevent buffer overflow.
* Fixed the memory reallocation issue.
* Prioritized detecting SP format using the file extension.

## v0.3.0 (2025-09-12)

* Adapted PJPG format for parsing transparency scenarios
* Added hardware JPEG support
* Integrated compatibility with LVGL v9

## v0.2.0 (2025-04-08)

* Added support for LVGL v9 JPG format.

## v0.1.2 (2024-10-18)

* Added support for parsing split PNG images from the filesystem.
* Added support for JPG and QOI formats.

## v0.1.1 (2024-07-31)

* Added support for parsing standard PNG images form filesystem.

## v0.1.0 Initial Version (2024-06-27)

* Added support for parsing split PNG images from variable.
* Added support for parsing standard PNG images from variable.
