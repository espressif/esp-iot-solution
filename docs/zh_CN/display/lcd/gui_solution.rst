GUI 优化解决方案
========================================

:link_to_translation:`en:[English]`

.. _ESP LVGL Adapter:

ESP LVGL Adapter
-------------------------

ESP LVGL Adapter 是面向 ESP-IDF 的统一 LVGL 适配层，兼容 LVGL v8/v9，提供显示注册、防撕裂、线程安全访问和输入设备接入等核心能力，帮助开发者快速构建高性能 GUI 应用。组件支持 RGB、MIPI DSI、SPI、QSPI、I2C、I80 及单色显示等多种显示类型，支持多屏管理，并提供多种防撕裂模式与 TE 同步机制，可根据接口类型、旋转角度和帧缓冲数量灵活平衡流畅度、内存占用与显示稳定性。在支持的芯片上，它还可结合 PPA/DMA2D 加速、立即刷新、区域对齐、FPS 统计和 Dummy Draw 等能力，进一步优化显示性能与调试体验。

此外，ESP LVGL Adapter 还支持触摸、按键和旋钮等输入设备适配，提供多点触控独立控制、任务/中断唤醒通知，以及与 Tickless Light Sleep 配合的手动睡眠和 Auto Sleep 机制，可在保留 UI 状态的同时实现低功耗运行。通过 Kconfig 按需启用后，组件还能无缝集成文件系统桥接、图片解码和 FreeType 字体渲染，并针对 FreeType 栈占用、Flash 占用以及 Flash 加密场景下的 DMA 对齐约束做了适配与优化，为 ESP 平台上的 LVGL 应用提供更完整、更稳健的开箱即用支持。

组件链接：https://components.espressif.com/components/espressif/esp_lvgl_adapter

相关示例：

- :example:`display/gui/lvgl_common_demo`
- :example:`display/gui/lvgl_dummy_draw`
- :example:`display/gui/lvgl_decode_image`
- :example:`display/gui/lvgl_freetype_font`
- :example:`display/gui/lvgl_multi_screen`
- :example:`display/gui/lvgl_mono_demo`

.. _ESP LVGL EAF Player:

ESP LVGL EAF Player
-------------------------

esp_lv_eaf_player 是一款专为 LVGL v8/v9 设计的轻量级高效 EAF 动画播放器。它可以将压缩动画序列无缝集成到 LVGL 项目中。EAF 格式支持多种压缩方法，包括 RLE、Huffman 编码和 JPEG 压缩，在保持高质量动画的同时最大限度地减少内存占用。

特性：

- 支持多种压缩方式：RLE、Huffman、JPEG
- 针对嵌入式优化：内存占用小，解码效率高
- LVGL 集成：与 LVGL 控件系统无缝集成
- 动画控制：播放、暂停、重启和循环控制

适用场景：

- 使用 LVGL v8/v9 需要播放高质量动画的用户
- 需要在资源受限平台上实现高效动画播放

EAF 文件转换：可使用在线转换工具将 GIF 或其他动画格式转换为 EAF 格式：https://esp32-gif.espressif.com/

组件链接：https://components.espressif.com/components/espressif/esp_lv_eaf_player

相关示例：

- :example:`display/gui/lvgl_eaf_player`

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

.. _esp_lv_lottie_player:

esp_lv_lottie_player
-------------------------

esp_lv_lottie_player 是一款专为 LVGL v8/v9 设计的轻量级高效 Lottie 动画播放器。它可以将基于矢量的动画无缝集成到 LVGL 项目中。通过利用 ThorVG 作为渲染引擎，该模块确保了现代嵌入式 GUI 应用程序的高性能和灵活性。

适用场景：

- 使用 LVGL v8/v9 需要播放 Lottie 动画的用户

组件链接：https://components.espressif.com/components/espressif/esp_lv_lottie_player

相关示例：

- :example:`display/gui/lvgl_lottie_player`

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

.. _esp_emote_expression:

esp_emote_expression
-----------------------------------

esp_emote_expression 是基于 esp_emote_gfx 构建的表情与界面描述系统，提供完整的资源解析、加载与生命周期管理能力，并支持第三方自定义控件扩展。可作为 AI 交互设备的基础 UI 框架使用。

系统覆盖 AI 对话常见的 UI 场景，包括：

- 表情与动画显示
- 文本渲染（系统提示、用户消息等）
- 状态图标（说话中、聆听中等）
- 二维码展示
- 弹窗与提示组件

适用场景：

- AI 交互设备的基础 UI 框架
- 需要完整 UI 资源管理和控件扩展的嵌入式设备

已适配的仓库：

- xiaozhi：https://github.com/78/xiaozhi-esp32

- rainmaker agent：https://github.com/espressif/esp-agents-firmware

快速开始：

- gfx-gen-tool：https://gfx-gen-tool.pages.dev/

- 本地预置资源：https://components.espressif.com/components/espressif2022/esp_emote_assets

组件链接：https://components.espressif.com/components/espressif2022/esp_emote_expression
