GUI Optimization Solutions
========================================

:link_to_translation:`zh:[中文]`

.. _ESP LVGL Adapter:

ESP LVGL Adapter
----------------

ESP LVGL Adapter is a high-performance LVGL adaptation layer designed specifically for the entire ESP32 chip family. It provides deep optimizations for screen tearing and frame rate, and seamlessly integrates advanced features such as image decoding, FreeType font rendering, and Dummy Draw mode, delivering a true out-of-the-box experience for developers.

Component Link: https://components.espressif.com/components/espressif/esp_lvgl_adapter

Related Example:

- https://github.com/espressif/esp-iot-solution/tree/master/examples/display/gui/lvgl_common_demo

- https://github.com/espressif/esp-iot-solution/tree/master/examples/display/gui/lvgl_dummy_draw

- https://github.com/espressif/esp-iot-solution/tree/master/examples/display/gui/lvgl_decode_image

- https://github.com/espressif/esp-iot-solution/tree/master/examples/display/gui/lvgl_freetype_font

- https://github.com/espressif/esp-iot-solution/tree/master/examples/display/gui/lvgl_multi_screen

.. _Thorvg component:

Thorvg Component
----------------

Supports Lottie animation and TVG vector graphics parsing and rendering, noglic version supported

Component Link: https://components.espressif.com/components/espressif/thorvg

Related Example:

- Lottie: https://components.espressif.com/components/espressif/thorvg/versions/0.13.8/examples/thorvg-example

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