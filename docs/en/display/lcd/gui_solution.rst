GUI Optimization Solutions
========================================

:link_to_translation:`zh:[中文]`

.. _lvgl-tearing-prevention-and-frame-rate-optimization:

LVGL Tearing Prevention and Frame Rate Optimization
---------------------------------------------------

- Supports tearing prevention mechanism based on full_fresh, direct_mode, and partial_mode
- Supports full screen rotation optimization for RGB interface and MIPI-DSI interface

Applicable Scenarios:

- Need to balance tearing prevention and frame rate optimization

- Scenarios requiring full screen rotation in LVGL

Related Examples:

- https://github.com/espressif/esp-iot-solution/tree/master/examples/display/lcd/mipi_dsi_avoid_tearing
- https://github.com/espressif/esp-iot-solution/tree/master/examples/display/lcd/rgb_avoid_tearing

.. _lvgl-decoder-component:

LVGL Image Decoder Component
----------------------------

- Supports LVGL v8 and v9 versions
- Supports PNG, JPG, QOI, PJPG, SJPG, SPNG format decoding with optimized decoding speed
- Supports JPEG hardware decoding for ESP32-P4
- Supports JPEG decoding acceleration for ESP32-S3

Description of special formats:

- PJPG format: Based on PNG format conversion, supports transparency and hardware JPEG decoding
- SJPG format, SPNG format: Segmented decoding, suitable for ESP32-C series with small RAM and no PSRAM
- QOI format: Compared to software JPEG, QOI decoding is faster but with lower compression ratio

Component Link: https://github.com/espressif/esp-iot-solution/tree/master/components/display/tools/esp_lv_decoder

Related Example: https://github.com/espressif/esp-iot-solution/tree/master/examples/hmi/perf_benchmark

.. _mmap-file-system-and-script-tool:

MMAP File System and Script Tool
--------------------------------

- Automatically packages user files
- Script automatically converts formats, supporting SJPG, SPNG, QOI, PJPG formats, etc.
- Reads materials through file system using mmap method

Applicable Scenarios:

- When SPIFFS and other file systems do not meet reading speed requirements, mmap can be used to read materials.
- Need image materials in SJPG, SPNG, QOI, PJPG and other formats

Component Link: https://components.espressif.com/components/espressif/esp_mmap_assets

Related Example: https://github.com/espressif/esp-iot-solution/tree/master/examples/hmi/perf_benchmark

.. _Thorvg component:

Thorvg Component
----------------

Supports Lottie animation and TVG vector graphics parsing and rendering, noglic version supported

Component Link: https://components.espressif.com/components/espressif/thorvg

Related Example:

- Lottie: https://components.espressif.com/components/espressif/thorvg/versions/0.13.8/examples/thorvg-example

.. _freetype-label-component:

freetype_label Component
------------------------

User component based on freetype, supports font size management, rendering, coloring, supports Chinese and English, supports swap16

Applicable Scenarios:

- Supports direct rendering of images and files

Component Link: https://github.com/espressif/idf-extra-components/tree/master/freetype

Related Example: https://github.com/espressif/esp-iot-solution/tree/master/examples/hmi/lvgl_freetype

.. _tinyGL 3D graphics library:

tinyGL 3D Graphics Library
--------------------------

- tinyGL is a lightweight 3D graphics library that supports a subset of OpenGL ES 1.1, suitable for ESP32-P4 platform.

Applicable Scenarios:

- Need to implement 3D graphics rendering.

Component Link: https://github.com/espressif2022/TinyGL

.. _SDL3 development library:

SDL3 Development Library
------------------------

SDL is a cross-platform development library designed to provide low-level access to audio, keyboard, mouse, joystick, and graphics hardware via OpenGL/Direct3D/Metal/Vulkan. It is used by video playback software, emulators, and many popular games including Valve's award-winning games and Humble Bundle games.

Applicable Scenarios:

- Users with emulator development, game porting, and cross-platform application development requirements.

Component Link: https://components.espressif.com/components/georgik/sdl/versions/3.1.2~9

Related Example: https://github.com/georgik/esp32-sdl3-test/tree/main

.. _Lottie player:

Lottie Player Component
-----------------------

lottie_player is a lightweight and efficient Lottie animation player designed specifically for LVGL v8. It can seamlessly integrate vector-based animations into LVGL projects. By leveraging ThorVG as the rendering engine, this module ensures high performance and flexibility for modern embedded GUI applications.

Applicable Scenarios:

- Users using LVGL v8 who need to play Lottie animations

Component Link: https://components.espressif.com/components/espressif2022/lottie_player

.. _esp_emote_gfx:

esp_emote_gfx
-------------

esp_emote_gfx is a lightweight animation rendering library for resource-constrained platforms, supporting conversion of GIF to custom EAF animation format for playback, with advantages of fast decoding, small memory footprint, and flexible display, suitable for efficient dynamic graphics playback needs in HMI scenarios.

Features:

- Supports text scrolling effects
- Supports simultaneous playback of multiple animations with independent frame rate control
- Adapts to multiple screen sizes

Applicable Scenarios:

- Guide animations, boot animations, emoticons and text rendering on various screens

- Dynamic UI component display on resource-constrained platforms (ESP32-C2/C3, etc.)

Component Link: https://components.espressif.com/components/espressif2022/esp_emote_gfx/