# LVGL EAF Player Example

## Introduction

This example demonstrates how to play EAF (Emote Animation Format) animations using the `esp_lv_eaf_player` component with LVGL. It shows a complete implementation including animation playback, play/pause control, and filesystem integration.

For detailed EAF usage and API reference, please refer to the [component README](../../../components/display/tools/esp_lv_eaf_player/README.md).

## Features

- **EAF Animation Playback**: Load and play EAF animations from flash partition
- **Interactive Controls**: Play/pause button for animation control
- **Frame Information**: Display total frame count and animation status
- **Filesystem Integration**: Mount EAF assets partition and access via LVGL filesystem
- **Loop Playback**: Infinite loop animation with configurable frame delay

## Build and Flash

```bash
cd examples/display/gui/lvgl_eaf_player
idf.py build flash monitor
```

## Usage

1. The example automatically mounts the EAF assets partition on drive `E:`
2. Loads the first available EAF file from the partition
3. Displays the animation with frame information at the top
4. Use the play/pause button at the bottom to control playback

### Configuration

- Frame delay: 30ms per frame (~33 FPS)
- Loop mode: Infinite loop enabled
- Animation source: First file from the EAF partition

## Assets

Place your `.eaf` files in the `assets/eaf/` directory. The build system will automatically pack them into the flash partition during compilation.

## Example Output

The example displays:
- Animation frame count at the top
- EAF animation in the center
- Play/pause control button at the bottom

