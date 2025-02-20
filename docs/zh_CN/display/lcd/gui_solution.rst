GUI 优化解决方案
========================================

:link_to_translation:`en:[English]`

.. _lvgl-tearing-prevention-and-frame-rate-optimization:

LVGL 防撕裂与帧率优化
-----------------------------------

- 支持基于 full_fresh、direct_mode、partial_mode 三种模式的防撕裂机制
- 支持针对 RGB 接口和 MIPI-DSI 接口的全屏幕旋转优化

适用场景：

- 需要兼顾防撕裂和帧率优化

- 需要进行 LVGL 整屏旋转的场景

相关示例：

- https://github.com/espressif/esp-iot-solution/tree/master/examples/display/lcd/mipi_dsi_avoid_tearing
- https://github.com/espressif/esp-iot-solution/tree/master/examples/display/lcd/rgb_avoid_tearing

.. _lvgl-decoder-component:

LVGL 图像解码组件
-------------------------

- 支持 LVGL v8 和 v9 版本
- 支持 PNG、JPG、QOI、PJPG、SJPG、SPNG 等格式解码，并优化了解码速率
- 针对 ESP32-P4 支持 JPEG 硬件解码
- 针对 ESP32-S3 支持 JPEG 解码加速

对于其中特殊格式的说明：

- PJPG 格式： 基于 PNG 格式转换，支持透明度，支持硬件 JPEG 解码
- SJPG 格式、SPNG 格式：分段解码，适用于 ESP32-C 系列 RAM 较小且没有 PSRAM 的场景
- QOI 格式：对比软件 JPEG，QOI 解码速度更快，但是压缩率更低

组件链接：https://github.com/espressif/esp-iot-solution/tree/master/components/display/tools/esp_lv_decoder

相关示例：https://github.com/espressif/esp-iot-solution/tree/master/examples/hmi/perf_benchmark

.. _mmap-file-system-and-script-tool:

基于 MMAP 的文件系统与脚本工具
-----------------------------------

- 自动打包用户文件
- 脚本自动转换格式，支持 SJPG，SPNG，QOI，PJPG 格式等
- 通过文件系统以 mmap 的方式读取素材

适用场景：

- 不满足于 SPIFFS 等文件系统的读取速度，可使用 mmap 的方式读取素材。
- 需要 SJPG，SPNG，QOI，PJPG 等格式的图片素材

组件链接：https://components.espressif.com/components/espressif/esp_mmap_assets

相关示例：https://github.com/espressif/esp-iot-solution/tree/master/examples/hmi/perf_benchmark

.. _Thorvg component:

Thorvg 组件
-------------------------

支持 Lottie 动画和 TVG 矢量图形解析渲染刷图，noglic 版本支持

组件链接：https://components.espressif.com/components/espressif/thorvg

相关示例：

- Lottie：https://components.espressif.com/components/espressif/thorvg/versions/0.13.8/examples/thorvg-example

.. _freetype-label-component:

freetype_label 组件
-------------------------

基于 freetype 的用户组件，支持字体字号管理，渲染，着色，支持中英文，支持 swap16

适用场景：

- 支持图片和文件的直接渲染

组件链接：https://github.com/espressif/idf-extra-components/tree/master/freetype

相关示例：https://github.com/espressif/esp-iot-solution/tree/master/examples/hmi/lvgl_freetype

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