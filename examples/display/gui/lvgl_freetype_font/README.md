| Supported Targets | ESP32-P4 | ESP32-S3 | ESP32-C3 |
| ----------------- | -------- | -------- | -------- |

# LVGL FreeType Font Example

This example demonstrates how to use **FreeType font rendering** with LVGL through the `esp_lvgl_adapter` component. FreeType allows you to render scalable TrueType fonts at any size, enabling multi-language support and dynamic font sizing in your LVGL applications.

## Overview

The example showcases:
- **FreeType font integration**: Load and render TrueType (.ttf) fonts dynamically
- **Multiple font sizes**: Create different font instances at various point sizes (30px and 48px)
- **Memory-mapped font assets**: Store fonts in flash partition using `esp_mmap_assets`
- **Virtual filesystem**: Access font files through LVGL's filesystem abstraction
- **Responsive UI**: Adapts layout for small displays (240x240) and larger screens
- **Interactive demo**: Button counter to demonstrate text rendering
- **Input device support**: Works with both touch panels and rotary encoders

## Why Use FreeType?

**Advantages:**
- ✅ **Scalable fonts**: Render fonts at any size without pre-generating bitmaps
- ✅ **Multi-language support**: Handle complex scripts (Arabic, Chinese, Thai, etc.)
- ✅ **Font variety**: Use any TrueType font file
- ✅ **Memory efficient**: Single font file serves all sizes
- ✅ **Professional typography**: High-quality anti-aliased text rendering

**Trade-offs:**
- ❌ Higher CPU usage for rendering (mitigated by caching)
- ❌ Requires FreeType library (included in ESP-IDF components)
- ❌ Slightly higher memory usage compared to pre-rendered fonts

## How to Use the Example

### Hardware Required

* An ESP32-P4, ESP32-S3, or ESP32-C3 development board
* A LCD panel with one of the supported interfaces:
  - **MIPI DSI**: For high-resolution displays (e.g., 1024x600)
  - **RGB**: For parallel RGB interface displays (e.g., 800x480)
  - **QSPI**: For quad-SPI displays (e.g., 360x360, 400x400)
  - **SPI**: For standard SPI displays (e.g., 240x240, 320x240)
* (Optional) Touch panel or rotary encoder for input
* A USB cable for power supply and programming

**Recommended Hardware Combinations:**

| Chip | LCD Interface | Development Board |
|------|---------------|-------------------|
| ESP32-P4 | MIPI DSI | [ESP32-P4-Function-EV-Board](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32p4/esp32-p4-function-ev-board/index.html) |
| ESP32-S3 | RGB | [ESP32-S3-LCD-EV-Board](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-lcd-ev-board/index.html) |
| ESP32-S3 | QSPI | [EchoEar](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/echoear/index.html) |
| ESP32-S3 | SPI | [ESP32-S3-BOX-3](https://github.com/espressif/esp-box/blob/master/docs/hardware_overview/esp32_s3_box_3/hardware_overview_for_box_3.md) |
| ESP32-C3 | SPI | [ESP32-C3-LCDkit](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32c3/esp32-c3-lcdkit/index.html) |

### Hardware Connection

This example uses the `hw_init` component for hardware abstraction. Connection details depend on your board and LCD interface type. Refer to your board's documentation for GPIO mappings.

### Font Asset Preparation

This example **automatically downloads** the TrueType font file (`DejaVuSans.ttf`) from the internet during the build process. The build system:

1. **Downloads the font** from GitHub: `https://github.com/espressif/esp-docs/raw/.../DejaVuSans.ttf`
2. **Saves to build directory** (`build/font_assets/`)
3. **Creates a SPIFFS partition image** containing the font file
4. **Generates metadata** (`mmap_generate_fonts.h`) with file information
5. **Flashes the partition** to the "fonts" partition (defined in `partitions.csv`)

**No manual steps required!** The font is downloaded and flashed automatically when you run `idf.py build flash`.

> **Note:** An internet connection is required during the first build to download the font file. The downloaded font is cached in the build directory and will not be re-downloaded unless you run `idf.py fullclean`.

#### Using Your Own Fonts

To use different fonts:

**Option 1: Download from URL**
1. Edit `main/CMakeLists.txt` and change the `FONT_URL` to your font's URL:
   ```cmake
   set(FONT_URL "https://your-server.com/path/to/YourFont.ttf")
   set(FONT_FILE "YourFont.ttf")
   ```
2. Update the font filename in `main.c`:
   ```c
   #define FONT_FILE_NAME "YourFont.ttf"
   ```

**Option 2: Use Local Font Files**
1. Create a local directory (e.g., `local_fonts/`) and place your `.ttf` files there
2. Modify `main/CMakeLists.txt` to use local files instead of downloading:
   ```cmake
   set(FONT_ASSETS_DIR "${CMAKE_CURRENT_LIST_DIR}/../local_fonts")
   # Comment out or remove the file(DOWNLOAD ...) lines
   ```
3. Update the font filename in `main.c` accordingly

**Supported font formats:**
- TrueType fonts (.ttf)
- OpenType fonts (.otf) may work but .ttf is recommended

### Configure the Project

Run `idf.py menuconfig` and navigate to `Example Configuration`:

**LCD Interface:**
- Select your LCD interface type (MIPI DSI / RGB / QSPI / SPI)
- Configure resolution and timing

**Display Settings:**
- Choose screen rotation (0°/90°/180°/270°)

**Input Device:**
- Enable touch panel or encoder
- Configure I2C/GPIO settings

### Build and Flash

1. Set the target chip:
```bash
idf.py set-target esp32p4
# or
idf.py set-target esp32s3
# or
idf.py set-target esp32c3
```

2. Build and flash the project:
```bash
idf.py -p PORT build flash monitor
```

The build process will:
- Compile the application code
- Download `DejaVuSans.ttf` from the internet (if not already cached)
- Generate the font partition image from the downloaded font
- Flash both the application and font partition
- Start the serial monitor

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for full steps to configure and use ESP-IDF to build projects.

### Expected Output

**LCD Display:**

The screen shows a blue-themed interface with:
- **Title**: "DejaVuSans.ttf" (rendered in 30px FreeType font)
- **Sample text 1**: "Sample text size 30" (30px font)
- **Sample text 2**: "Sample text size 48" (48px font, hidden on small displays)
- **Button**: "Press me" (cyan colored)

On small displays (≤240px), the layout automatically switches to compact mode.

**Serial Console:**
```
I (xxx) freetype_demo: Selected LCD interface: MIPI DSI
I (xxx) freetype_demo: Initializing LCD: 1024x600
I (xxx) freetype_demo: Init touch
I (xxx) freetype_demo: Encoder group configured
```

**Interaction:**
- **Touch mode**: Tap the "Press me" button
- **Encoder mode**: Rotate encoder, then press to activate
- The text labels update to show "Pressed X" or "Count X"

## Code Structure

**Main Components:**

1. **Font Asset Mounting** (`mount_font_assets()`):
   - Initializes memory-mapped assets from "fonts" partition
   - Mounts virtual filesystem with drive letter 'F'
   - Builds font file path (e.g., "F:DejaVuSans.ttf")

2. **FreeType Initialization** (`init_fonts()`):
   - Creates font instances at 30px and 48px sizes
   - Uses `ESP_LV_ADAPTER_FT_FONT_FILE_CONFIG` macro for configuration
   - Retrieves LVGL font pointers for use in UI

3. **UI Creation** (`create_freetype_screen()`):
   - Creates responsive layout
   - Applies FreeType fonts to labels and buttons
   - Handles compact mode for small displays
   - Sets up encoder input group if applicable

**Key Files:**
- `main.c`: Application logic
- `main/CMakeLists.txt`: Build configuration with font download URL
- `mmap_generate_fonts.h`: Auto-generated font metadata (do not edit manually)
- `partitions.csv`: Partition table including 800KB "fonts" partition
- `build/font_assets/DejaVuSans.ttf`: Downloaded font file (340KB, auto-generated)

## Technical Details

### Font Partition Layout

```
Partition Table (partitions.csv):
┌─────────────────────────────────────┐
│ nvs       (24KB)                    │
│ phy_init  (4KB)                     │
│ factory   (2000KB) - Application    │
│ fonts     (800KB)  - Font Files     │ ← FreeType fonts stored here
└─────────────────────────────────────┘
```

### Memory-Mapped Asset Access

The example uses `esp_mmap_assets` component to:
- Map font partition into memory without copying
- Provide file-like access through virtual filesystem
- Support FreeType's file reading operations
- Minimize RAM usage (fonts stay in flash)

### FreeType Configuration

Fonts are initialized with the following parameters:
```c
esp_lv_adapter_ft_font_config_t cfg = {
    .type = ESP_LV_ADAPTER_FT_FONT_FILE,      // Load from file
    .file = {
        .name = "F:DejaVuSans.ttf",     // Virtual filesystem path
        .size = 30,                      // Font size in pixels
    },
    .cache_max_faces = 1,                // Max cached font faces
    .cache_max_sizes = 1,                // Max cached sizes per face
    .cache_max_bytes = 16 * 1024,        // 16KB cache
    .style = ESP_LV_ADAPTER_FT_FONT_STYLE_NORMAL,
};
```

## Troubleshooting

**Fonts not displaying (boxes or garbled text):**
- Verify font partition flashed correctly: `idf.py partition-table`
- Check serial logs for "Failed to mount font filesystem" errors
- Ensure font was downloaded successfully (check `build/font_assets/` directory)
- Verify `mmap_generate_fonts.h` was regenerated

**Build error: "Failed to download font" or network error:**
- Check your internet connection
- Verify the font URL is accessible: `curl -I <FONT_URL>`
- If behind a proxy, configure CMake proxy settings
- Alternative: Download manually and place in `build/font_assets/DejaVuSans.ttf`

**Build error: "Failed to create partition image":**
- Check that font was downloaded to `build/font_assets/DejaVuSans.ttf`
- Verify partition table allocates enough space (current: 800KB)
- Ensure font file is not corrupted
- Try running `idf.py fullclean` and rebuild

**Application crashes on font initialization:**
- Increase heap size in menuconfig (FreeType needs memory)
- Enable PSRAM if available
- Check font file format (must be valid .ttf)
- Verify `cache_max_bytes` is reasonable

**Poor rendering performance:**
- Increase FreeType cache size
- Reduce number of different font sizes
- Use smaller font sizes if possible
- Enable PSRAM and increase cache

**Wrong font displayed:**
- Check `FONT_FILE_NAME` matches actual file in `assets/`
- Verify case sensitivity (Linux filesystems are case-sensitive)
- Ensure `mmap_generate_fonts.h` matches actual files

**Touch/encoder not working:**
- Verify input device works in other examples first
- Check GPIO configuration
- For encoder: verify group configuration and focus

For any technical queries, please open an [issue](https://github.com/espressif/esp-iot-solution/issues) on GitHub. We will get back to you soon.

