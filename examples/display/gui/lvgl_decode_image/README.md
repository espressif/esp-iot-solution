| Supported Targets | ESP32-P4 | ESP32-S3 | ESP32-C3 |
| ----------------- | -------- | -------- | -------- |

# LVGL Image Decoder Example

This example demonstrates how to decode and display multiple image formats using LVGL with the `esp_lv_decoder` component. It showcases efficient image handling for embedded displays, including memory-optimized split formats and hardware-accelerated decoding.

## Overview

The example showcases:
- **Multiple image format support**: JPG, PNG, QOI, Split-PNG (SPNG), Split-JPEG (SJPG), and PNG-JPEG (PJPG)
- **Memory-mapped assets**: Store images in flash partitions for efficient access
- **Split image formats**: Reduce RAM usage by decoding images in strips (SPNG/SJPG)
- **Hardware acceleration**: Hardware JPEG decoding on ESP32-P4
- **Interactive format switching**: Cycle through different formats using button or encoder
- **Automatic animation**: Images play continuously with format information display
- **Responsive UI**: Adapts to various screen sizes (240x240 to 1024x600)

## Supported Image Formats

| Format | Description | Chip Support | Memory Usage | Decode Speed |
|--------|-------------|--------------|--------------|--------------|
| **JPG** | Standard JPEG | ESP32-S3, ESP32-P4 | Medium | Fast |
| **PNG** | Standard PNG | ESP32-S3, ESP32-P4 | High | Medium |
| **QOI** | Quite OK Image | ESP32-S3, ESP32-P4 | Low | Very Fast |
| **SPNG** | Split PNG (16-line strips) | All | Very Low | Medium |
| **SJPG** | Split JPEG (16-line strips) | All | Very Low | Fast |
| **PJPG** | PNG-JPEG | ESP32-P4 only | Low | Very Fast |

### Format Details

**Standard Formats (JPG/PNG/QOI):**
- Full image decoded into RAM buffer
- Higher memory requirements
- Faster rendering once decoded

**Split Formats (SPNG/SJPG):**
- Image divided into horizontal strips (16 pixels height)
- Only one strip in RAM at a time
- ~40x less RAM usage compared to full decode
- Ideal for memory-constrained applications
- Slight performance trade-off

**Hardware-Accelerated (PJPG):**
- Hardware JPEG decoding
- Much faster than software decoding
- Lower CPU usage

## How to Use the Example

### Hardware Required

* A development board with:
  - **ESP32-P4**: All formats supported
  - **ESP32-S3**: All except PJPG
  - **ESP32-C3**: Only SPNG and SJPG (limited RAM)
* A LCD panel with supported interface (MIPI DSI / RGB / QSPI / SPI)
* (Optional) Touch panel or rotary encoder for interaction
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

This example uses the `hw_init` component for hardware abstraction. Refer to your board's documentation for LCD and touch panel connections.

### Image Asset Preparation

This example includes sample images (8 frames of animation) in the `assets/` directory:
- `assets/jpg/`: JPEG images (frame_0000.jpg to frame_0007.jpg)
- `assets/png/`: PNG images (frame_0000.png to frame_0007.png)

**The build system automatically:**
1. Converts images to various formats (QOI, SPNG, SJPG, PJPG)
2. Creates partition images for each format
3. Generates metadata headers (e.g., `mmap_generate_jpg.h`)
4. Flashes all partitions during `idf.py flash`

**No manual conversion required!** Just provide the source JPG and PNG files.

#### Adding Your Own Images

To use custom images:

1. Place your images in `assets/jpg/` or `assets/png/`
2. Use consistent naming (e.g., `frame_XXXX.jpg`)
3. Recommended image properties:
   - Resolution: Match your LCD or smaller
   - Color depth: 24-bit RGB
   - File size: <50KB per image for best performance

4. The build system will automatically:
   - Generate split format variants
   - Update partition images
   - Regenerate metadata headers

**Partition sizes:**
- Each format partition: 100KB (defined in `partitions.csv`)
- Adjust if you have many/large images

### Configure the Project

Run `idf.py menuconfig` and navigate to `Example Configuration`:

**LCD Interface:**
- Select interface type (MIPI DSI / RGB / QSPI / SPI)
- Configure resolution and GPIO pins

**Display Settings:**
- Choose rotation (0°/90°/180°/270°)

**Input Device:**
- Enable touch or encoder input

**Image Decoder:**
- Options are automatically selected based on target chip

### Build and Flash

1. Set the target chip:
```bash
idf.py set-target esp32p4
# or esp32s3, esp32c3
```

2. Build and flash:
```bash
idf.py -p PORT build flash monitor
```

**Build process:**
- Compiles application
- Generates image format partitions from assets
- Creates metadata headers
- Flashes application + all image partitions (6 partitions total)

**First build takes longer** due to image processing and component downloads.

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for full steps to configure and use ESP-IDF.

### Expected Output

**LCD Display:**

The screen shows:
- **Top label**: Current format name and file count (e.g., "SPNG (8 files)")
- **Center**: Animated image cycling through 8 frames
- **Bottom button**: "Next Format" to switch image formats

Images auto-play with smooth transitions (~50ms per frame).

**Serial Console:**
```
I (xxx) decode_demo: LCD: MIPI DSI
I (xxx) decode_demo: Init LCD 1024x600
I (xxx) decode_demo: Init touch
I (xxx) decode_demo: SPNG ready (8 files)
I (xxx) decode_demo: SJPG ready (8 files)
I (xxx) decode_demo: PJPG ready (8 files)  // ESP32-P4 only
```

**Interaction:**
- **Touch**: Tap "Next Format" button to switch formats
- **Encoder**: Rotate encoder (accumulate 3 clicks) then press button to switch
- Format cycles through available options based on chip

**Chip-Specific Behavior:**
- **ESP32-P4**: All formats available (JPG → PNG → QOI → SPNG → SJPG → PJPG)
- **ESP32-S3**: 5 formats (JPG → PNG → QOI → SPNG → SJPG)
- **ESP32-C3**: 2 formats only (SPNG → SJPG)

## Code Structure

**Main Components:**

1. **Format Context** (`image_format_ctx_t`):
   - Each format has: label, partition name, drive letter, file count
   - Tracks current playback index
   - Manages mmap assets and filesystem handles

2. **Image Player** (`image_player_ctx_t`):
   - Controls format switching
   - Updates UI labels
   - Manages playback timer
   - Handles encoder accumulation

3. **Format Mounting** (`mount_format()`):
   - Initializes mmap_assets for partition
   - Mounts virtual filesystem (drive letters: J, P, Q, S, T, U)
   - Validates stored files

4. **UI Creation** (`start_image_player()`):
   - Creates LVGL image widget
   - Sets up format label and button
   - Configures encoder group
   - Starts 50ms timer for frame updates

**Key Functions:**
- `show_next_frame()`: Updates image widget source (e.g., "S:frame_0001.png")
- `next_available_format()`: Cycles to next valid format
- `prepare_formats()`: Mounts all available format partitions

## Partition Layout

```
Partition Table (partitions.csv):
┌──────────────────────────────────────┐
│ nvs       (24KB)                     │
│ phy_init  (4KB)                      │
│ factory   (1000KB) - Application     │
│ jpg       (100KB)  - JPEG images     │ ← 8 JPG files
│ png       (100KB)  - PNG images      │ ← 8 PNG files
│ qoi       (100KB)  - QOI images      │ ← 8 QOI (converted)
│ spng      (100KB)  - Split PNG       │ ← 8 SPNG (converted)
│ sjpg      (100KB)  - Split JPEG      │ ← 8 SJPG (converted)
│ pjpg      (100KB)  - PJPEG           │ ← 8 PJPG (converted, ESP32-P4)
└──────────────────────────────────────┘
```

**Total flash usage**: ~1.6MB (adjust partition sizes if needed)

## Technical Details

### Split Image Format

SPNG/SJPG splits images horizontally:
```
Original Image (480x320)
┌─────────────────────┐
│ Strip 0 (480x16)    │ ← Decoded to RAM
├─────────────────────┤
│ Strip 1 (480x16)    │ ← Decoded when needed
├─────────────────────┤
│ Strip 2 (480x16)    │
│       ...           │
│ Strip 19 (480x16)   │
└─────────────────────┘
```

**Configuration:**
- `MMAP_SPLIT_HEIGHT 16`: Each strip is 16 pixels tall
- Smaller strips = less RAM, more decode operations
- Optimal for most embedded displays

### Virtual Filesystem Mapping

Each format gets a drive letter:
```c
'J' → JPG partition  → "J:frame_0000.jpg"
'P' → PNG partition  → "P:frame_0000.png"
'Q' → QOI partition  → "Q:frame_0000.png"
'S' → SPNG partition → "S:frame_0000.png"
'T' → SJPG partition → "T:frame_0000.jpg"
'U' → PJPG partition → "U:frame_0000.png"
```

LVGL image widget loads from virtual path.

## Troubleshooting

**Images not showing:**
- Check serial logs for "Failed to mount" errors
- Verify partition table flashed: `idf.py partition-table`
- Ensure assets exist in `assets/jpg/` and `assets/png/`
- Rebuild to regenerate partition images

**Build error: "Partition too small":**
- Images exceed 100KB per partition
- Reduce image sizes or increase partition size in `partitions.csv`
- Use compression (JPEG quality 85-90)

**Poor animation performance:**
- Reduce image resolution
- Increase timer interval in code (currently 50ms)
- Enable PSRAM if available

**Format switching doesn't work:**
- Check that partitions flashed successfully
- Verify touch/encoder input works
- Look for "No other format available" in logs
- Some formats may be unavailable on your chip

**PJPG not available:**
- Only supported on ESP32-P4

**Memory allocation failures:**
- Enable PSRAM for large displays
- Use split formats (SPNG/SJPG) instead
- Reduce LVGL buffer sizes

**Encoder not switching formats:**
- Requires 3 rotation steps (threshold)
- Adjust `ENCODER_THRESHOLD` if too sensitive
- Check encoder GPIO configuration

## Choosing the Right Format

**For ESP32-P4:**
- Use **PJPG** for best performance (hardware accelerated)
- Use **SJPG** if PJPG not available

**For ESP32-S3:**
- Use **SJPG** for good balance (low RAM, fast)
- Use **JPG** if you have PSRAM and need speed

**For ESP32-C3:**
- Use **SJPG** for save memory resources
- Keep images small

For any technical queries, please open an [issue](https://github.com/espressif/esp-iot-solution/issues) on GitHub. We will get back to you soon.

