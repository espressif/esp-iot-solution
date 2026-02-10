[![Component Registry](https://components.espressif.com/components/espressif/esp_lv_eaf_player/badge.svg)](https://components.espressif.com/components/espressif/esp_lv_eaf_player)

# EAF Player for LVGL

## Introduction
`esp_lv_eaf_player` is a lightweight and efficient EAF (Emote Animation Format) player designed specifically for **LVGL v8/v9**. It enables seamless integration of compressed animation sequences into your LVGL projects. The EAF format supports multiple compression methods including RLE, Huffman coding, and JPEG compression, providing high-quality animations with minimal memory footprint.

## Features
- **Multiple Compression Support**: RLE, Huffman, JPEG compression methods
- **Optimized for Embedded**: Minimal memory footprint and efficient decoding
- **LVGL Integration**: Seamless integration with LVGL widget system
- **Frame Control**: Support for frame-by-frame control and loop management
- **Color Format Support**: RGB565 color format optimization for embedded displays
- **Animation Control**: Play, pause, restart, and loop control

## Add to Project

Add it to an ESP-IDF project via:

```bash
idf.py add-dependency espressif/esp_lv_eaf_player
```

Or declare it in `idf_component.yml`:

```yaml
dependencies:
  espressif/esp_lv_eaf_player: "*"
```

## EAF File Conversion

To convert GIF or other animation formats to EAF format, you can use the online conversion tool:

- **EAF Converter**: [https://esp32-gif.espressif.com/](https://esp32-gif.espressif.com/)

This tool allows you to upload your animation files and convert them to the EAF format optimized for embedded systems.

## Usage Example

```c
#include "lv_eaf.h"

// Create EAF animation object
lv_obj_t * eaf_anim = lv_eaf_create(lv_screen_active());

// Set EAF data source (from embedded data)
extern const lv_image_dsc_t my_animation_eaf;
lv_eaf_set_src(eaf_anim, &my_animation_eaf);

// Configure animation
lv_eaf_set_loop_count(eaf_anim, -1);  // Infinite loop
lv_eaf_set_frame_delay(eaf_anim, 50); // 50ms per frame

// Position the animation
lv_obj_center(eaf_anim);
```

### Loading from a file on LVGL FS

```c
lv_obj_t * eaf_anim = lv_eaf_create(lv_screen_active());
lv_eaf_set_src(eaf_anim, "E:/animations/intro.eaf"); // 'E' drive mounted via esp_lvgl_adapter
```

> Make sure the LVGL FS drive is mounted before calling `lv_eaf_set_src`.

## API Reference

### Object Creation
- `lv_obj_t * lv_eaf_create(lv_obj_t * parent)` - Create EAF animation object

### Source Management
- `void lv_eaf_set_src(lv_obj_t * obj, const void * src)` - Set EAF data source

### Animation Control
- `void lv_eaf_restart(lv_obj_t * obj)` - Restart animation from first frame
- `void lv_eaf_pause(lv_obj_t * obj)` - Pause animation
- `void lv_eaf_resume(lv_obj_t * obj)` - Resume paused animation

### Configuration
- `void lv_eaf_set_loop_count(lv_obj_t * obj, int32_t count)` - Set loop count (-1 for infinite)
- `void lv_eaf_set_frame_delay(lv_obj_t * obj, uint32_t delay)` - Set frame delay in milliseconds
- `void lv_eaf_set_loop_enabled(lv_obj_t * obj, bool enable)` - Enable/disable looping

### Status Queries
- `bool lv_eaf_is_loaded(lv_obj_t * obj)` - Check if EAF is loaded
- `int32_t lv_eaf_get_total_frames(lv_obj_t * obj)` - Get total frame count
- `int32_t lv_eaf_get_current_frame(lv_obj_t * obj)` - Get current frame index
- `int32_t lv_eaf_get_loop_count(lv_obj_t * obj)` - Get current loop count
- `uint32_t lv_eaf_get_frame_delay(lv_obj_t * obj)` - Get frame delay
- `bool lv_eaf_get_loop_enabled(lv_obj_t * obj)` - Check if looping is enabled


## License

This component is provided under the Apache-2.0 license. See LICENSE file for details.

## Contributing

Contributions are welcome! Please ensure your changes:
- Follow the existing code style
- Include appropriate error handling
- Update documentation as needed
- Test with various EAF file formats

For questions or bug reports, please open an issue in the [esp-iot-solution](https://github.com/espressif/esp-iot-solution) repository.
