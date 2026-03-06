| Supported Targets | ESP32-P4 | ESP32-S3 | ESP32-C3 |
| ----------------- | -------- | -------- | -------- |

# LVGL Filesystem Assets Example

This example shows how to package a JPEG image and a LVGL binfont into a flash filesystem image, mount it at runtime, decode the image through `esp_lv_decoder`, and load the font through `lv_binfont_create()`.

## What It Shows

- Package assets from `fs_assets/` into `SPIFFS`, `FATFS`, or `LittleFS`
- Mount the selected filesystem at `/storage`
- Load `A:/storage/wifi_icon.jpeg`
- Load `A:/storage/kaiti_24.bin`

The shipped binfont is a subset font, not a complete font library. It only covers the current demo UI text, including `乐鑫 ESP32`.

## Binfont

This example uses LVGL's binfont loader. The font file is:

- `fs_assets/kaiti_24.bin`

It is mounted as:

- `A:/storage/kaiti_24.bin`

If you rename an asset file, update the corresponding path in `main/fs_backend.c`.

## Generate a Binfont

The shipped font is an uncompressed subset binfont. To generate one:

```bash
lv_font_conv \
  --font /path/to/your_font.ttf \
  --size 24 \
  --bpp 4 \
  --format bin \
  --no-compress \
  -o kaiti_24.bin \
  -r 0x20-0x7F \
  --symbols "乐鑫"
```

Notes:

- `-r 0x20-0x7F` keeps the ASCII range
- `--symbols "乐鑫"` adds the Chinese glyphs used by this demo

To expand the font, add more characters to `--symbols` or widen the `-r` ranges. Whether the font is complete depends on the selected character set.
The on-screen asset details are derived from the configured runtime paths, so the displayed filenames stay in sync with `main/fs_backend.c`.

## Filesystem Choice

Select the filesystem in `LVGL FS Assets Demo Configuration`:

- `SPIFFS`: simplest choice for static GUI assets
- `LittleFS`: lightweight choice for embedded flash storage
- `FATFS`: suitable if your project already uses FATFS or needs wear levelling based storage

The example uses a fixed `partitions.csv` with:

- `storage_spiffs`
- `storage_fatfs`
- `storage_littlefs`

Only the selected filesystem is packaged and mounted.

## Assets

Current files in `fs_assets/`:

- `wifi_icon.jpeg`
- `kaiti_24.bin`

They are packaged by:

- `spiffs_create_partition_image(...)`
- `fatfs_create_spiflash_image(...)`
- `littlefs_create_partition_image(...)`

## Build

```bash
idf.py set-target esp32s3
idf.py build flash monitor
```

Replace `esp32s3` with `esp32p4` or `esp32c3` as needed.

## Result

After boot:

- the selected filesystem is mounted at `/storage`
- the JPEG is loaded from `A:/storage/wifi_icon.jpeg`
- the binfont is loaded from `A:/storage/kaiti_24.bin`
- the screen shows `乐鑫 ESP32`

## Key Files

- `main/main.c`
- `main/fs_backend.c`
- `main/Kconfig.projbuild`
- `main/CMakeLists.txt`
- `partitions.csv`
- `fs_assets/`
