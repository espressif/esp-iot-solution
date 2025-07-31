| Supported Targets | ESP32-P4 |
| ----------------- | -------- |

# HDMI Video Renderer

This example demonstrates how to display a video in MP4/AVI format on an HDMI monitor.

## How to use the example

## ESP-IDF Required

### Hardware Required

* An ESP32-P4-Function-EV-Board.
* An [ESP-HDMI-Bridge](https://oshwhub.com/esp-college/esp-hdmi-bridge).
* An SD card that stores MP4 videos.
* A USB-C cable for power supply and programming.
* Please refer to the following steps for the connection:
    * **Step 1**. According to the table below, connect the pins on the back of the screen adapter board to the corresponding pins on the development board.

        | ESP-HDMI-Bridge      | ESP32-P4-Function-EV-Board |
        | -------------------- | -------------------------- |
        | 5V (any one)         | 5V (any one)               |
        | GND (any one)        | GND (any one)              |

    * **Step 2**. Connect the FPC of LCD through the `MIPI_DSI` interface.
    * **Step 3**. Use a USB-C cable to connect the `USB-UART` port to a PC (Used for power supply and viewing serial output).
    * **Step 4**. Turn on the power switch of the board.

### Configure the Project

Run `idf.py menuconfig` and navigate to `HDMI MP4 Player Configuration` menu.

- Navigate to `Video File Configuration` submenu
- Set the `Video File Name` to match your video file name stored in the SD card
- Default value is `test_video.mp4`
- Make sure the file name matches exactly, including the `.mp4` or `.avi` extension

### Test Video Downloads

This repository provides a test MP4 file encoded with MJPEG video and AAC audio for testing and compatibility purposes.

- **Video Codec**: MJPEG  
- **Audio Codec**: AAC  
- **Frame Rate**: 20fps(RGB888)
- **File Format**: `.mp4`  
- **Download Link**: [test_video.mp4](https://dl.espressif.com/AE/esp-dev-kits/test_video.mp4)

This test file is useful for validating MJPEG decoding, AAC audio support, and embedded system playback compatibility.

### Build and Flash

Run `idf.py set-target esp32p4` to select the target chip.

Run `idf.py -p PORT build flash monitor` to build, flash and monitor the project. A fancy animation will show up on the LCD as expected.

The first time you run `idf.py` for the example will cost extra time as the build system needs to address the component dependencies and downloads the missing components from registry into `managed_components` folder.

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for full steps to configure and use ESP-IDF to build projects.

## Troubleshooting

For any technical queries, please open an [issue](https://github.com/espressif/esp-iot-solution/issues) on GitHub. We will get back to you soon.

## Important Notes

### Video Format Requirements

1. **MP4 Container Format**
   - Currently only supports MP4 files with MJPEG video encoding
   - Other video codecs (H.264, H.265, etc.) are not supported at this time
   - Audio tracks in MP4 files are supported

2. **Video Resolution and Format**
   - For YUV420 format videos:
     * Width must be divisible by 16
     * Height must be divisible by 16
     * This alignment requirement ensures optimal performance and compatibility
   - For YUV422 format videos:
     * Width must be divisible by 16
     * Height must be divisible by 8
     * Provides better color quality than YUV420 but requires more bandwidth
   - For YUV444 format videos:
     * Width must be divisible by 8
     * Height must be divisible by 8
     * Offers the highest color quality but requires the most bandwidth

3. **Display Buffer Configuration**
   - When using LCD internal buffer mode:
     * Video resolution must exactly match the HDMI output resolution
     * This ensures optimal performance and prevents display artifacts
     * Example: If HDMI is set to 1280x720, the MP4 must also be 1280x720
   - When using external buffer mode:
     * More flexible resolution support
     * Automatic scaling is available
     * May have slightly lower performance compared to internal buffer mode

### FAQ

#### Blue Screen Flickering Issues

If you encounter blue screen flickering during video playback, this is typically caused by **insufficient PSRAM bandwidth**. Here are the recommended solutions:

1. **Use RGB565 Format**
   - If your display supports RGB565, consider switching from RGB888 to RGB565 format

2. **Optimize Video Parameters**
   - **Lower Resolution**: Reduce video resolution (e.g., from 1280x720 to 1024x768)
   - **Reduce Frame Rate**: Lower the video frame rate (e.g., from 30fps to 20fps or 15fps)
   - **Increase Compression**: Use higher JPEG compression quality to reduce frame sizes

3. **AVI Format Audio Limitations**
   - **AVI videos with audio are not recommended** due to higher bandwidth requirements
   - If playing AVI files causes blue screen flickering, **disable audio playback**
   - For audio playback, use MP4 format with AAC encoding instead

These optimizations help balance video quality with available system bandwidth to ensure stable playback.

## Video Conversion with FFmpeg

### Installation
```bash
# Windows: Download from https://ffmpeg.org/download.html
# macOS: brew install ffmpeg  
# Linux: sudo apt install ffmpeg
```

### Basic MJPEG Conversion
```bash
# Convert any video to MJPEG MP4
ffmpeg -i input.mp4 -c:v mjpeg -c:a aac output.mp4
```

### Recommended Settings

**High Quality (1280x720, RGB888 displays):**
```bash
ffmpeg -i input.mp4 -c:v mjpeg -q:v 3 -vf scale=1280:720 -r 20 -c:a aac output.mp4
```

**Balanced (1024x768, recommended):**
```bash
ffmpeg -i input.mp4 -c:v mjpeg -q:v 5 -vf scale=1024:768 -r 20 -c:a aac output.mp4
```

**Low Bandwidth (800x600, RGB565 or troubleshooting):**
```bash
ffmpeg -i input.mp4 -c:v mjpeg -q:v 8 -vf scale=800:600 -r 15 -c:a aac output.mp4
```

### Key Parameters
- `-q:v`: Quality (1-31, lower = better quality, larger file)
- `-vf scale=W:H`: Resolution adjustment
- `-r`: Frame rate (15-25 fps recommended)
- `-an`: Remove audio if causing issues