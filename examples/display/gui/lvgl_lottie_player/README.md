| Supported Targets | ESP32-P4 | ESP32-S3 | 
| ----------------- | -------- | -------- | 

# LVGL Lottie Player Example

## Introduction

This example demonstrates how to play Lottie JSON animations using the `esp_lv_lottie_player` component with LVGL. It shows a complete implementation including animation playback, filesystem integration, and display initialization through `esp_lvgl_adapter`.

For detailed Lottie usage and API reference, please refer to the [component README](../../../components/display/tools/esp_lv_lottie_player/README.md).

## Features

- **Lottie Animation Playback**: Load and play Lottie JSON animations from flash partition
- **Filesystem Integration**: Mount Lottie assets partition and access via LVGL filesystem
- **Memory-Mapped Assets**: Access SPIFFS partition contents via `esp_mmap_assets`
- **Loop Playback**: Infinite loop animation
- **Unified LCD Support**: Works with MIPI DSI, RGB, QSPI, and SPI panels through `hw_init`

## Build and Flash

```bash
cd examples/display/gui/lvgl_lottie_player
idf.py build flash monitor
```

## Usage

1. The example mounts the Lottie assets partition on drive `A:`
2. Finds the first available `.json` file in the partition
3. Creates a Lottie object sized to the smaller screen dimension
4. Plays the animation in an infinite loop

### Configuration

- Loop mode: Infinite loop enabled
- Animation source: First `.json` file from the `lottie` partition
- Render size: Square, based on the smaller of horizontal/vertical resolution

## Assets

By default, the build system downloads `welcome.json` and packs it into the `lottie` SPIFFS partition using `spiffs_create_partition_assets()` in `main/CMakeLists.txt`.

To use your own Lottie files:

1. Update `LOTTIE_URL` or `LOTTIE_FILE_NAME` in `examples/display/gui/lvgl_lottie_player/main/CMakeLists.txt`.
2. Ensure the file is a valid `.json` Lottie asset.
3. If the file is large, adjust the `lottie` partition size in `examples/display/gui/lvgl_lottie_player/partitions.csv`.

## Example Output

The example displays:
- Lottie animation centered on the screen
- Serial logs for LCD initialization and demo start
