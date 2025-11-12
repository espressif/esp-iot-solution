GUI 优化解决方案
========================================

:link_to_translation:`en:[English]`

.. _ESP LVGL Adapter:

ESP LVGL Adapter
-------------------------

ESP LVGL Adapter 是专为 ESP32 全系列芯片设计的高性能 LVGL 适配层，针对屏幕撕裂与帧率进行了深度优化，并无缝集成了图片解码、FreeType 字体渲染和 Dummy Draw 等高级特性，为开发者提供真正的开箱即用体验。

组件链接：https://components.espressif.com/components/espressif/esp_lvgl_adapter

相关示例：

- https://github.com/espressif/esp-iot-solution/tree/master/examples/display/gui/lvgl_common_demo

- https://github.com/espressif/esp-iot-solution/tree/master/examples/display/gui/lvgl_dummy_draw

- https://github.com/espressif/esp-iot-solution/tree/master/examples/display/gui/lvgl_decode_image

- https://github.com/espressif/esp-iot-solution/tree/master/examples/display/gui/lvgl_freetype_font

- https://github.com/espressif/esp-iot-solution/tree/master/examples/display/gui/lvgl_multi_screen

.. _Thorvg component:

Thorvg 组件
-------------------------

支持 Lottie 动画和 TVG 矢量图形解析渲染刷图，noglic 版本支持

组件链接：https://components.espressif.com/components/espressif/thorvg

相关示例：

- Lottie：https://components.espressif.com/components/espressif/thorvg/versions/0.13.8/examples/thorvg-example

.. _tinyGL 3D graphics library:

tinyGL 3D 图形库
-------------------------

- tinyGL 是一个轻量级的 3D 图形库，支持 OpenGL ES 1.1 的子集，适用于 ESP32-P4 平台。

适用场景：

- 需要实现 3D 图形渲染。

组件链接：https://github.com/espressif2022/TinyGL

.. _SDL3 development library:

SDL3 开发库
-------------------------

SDL 是一个跨平台的开发库，旨在通过 OpenGL/Direct3D/Metal/Vulkan 提供对音频、键盘、鼠标、摇杆和图形硬件的低级访问。它被视频播放软件、模拟器以及包括 Valve 获奖游戏在内的许多流行游戏和 Humble Bundle 游戏使用。

适用场景：

- 有模拟器开发、游戏移植和跨平台应用开发需求的用户。

组件链接：https://components.espressif.com/components/georgik/sdl/versions/3.1.2~9

相关示例：https://github.com/georgik/esp32-sdl3-test/tree/main

.. _Lottie player:

Lottie 播放组件
-------------------------

lottie_player 是一款专为 LVGL v8 设计的轻量级高效 Lottie 动画播放器。它可以将基于矢量的动画无缝集成到 LVGL 项目中。通过利用 ThorVG 作为渲染引擎，该模块确保了现代嵌入式 GUI 应用程序的高性能和灵活性。

适用场景：

- 使用 LVGL v8 需要播放 Lottie 动画的用户

组件链接：https://components.espressif.com/components/espressif2022/lottie_player

.. _esp_emote_gfx:

esp_emote_gfx
-----------------------------------

esp_emote_gfx 是一款面向资源受限平台的轻量级动画渲染库，支持将 GIF 转换为自定义 EAF 动画格式进行播放，具备解码快、内存占用小、显示灵活的优势，适用于 HMI 场景中的高效动态图形播放需求。

特性：

- 支持文字滚动效果
- 支持同时播放多个动图，帧率独立控制
- 适应多尺寸屏幕

适用场景：

- 各类屏幕上的引导动效、开机动画、表情和文字渲染等

- 资源受限平台（ESP32-C2/C3 等）上的动态 UI 组件展示

组件链接：https://components.espressif.com/components/espressif2022/esp_emote_gfx/