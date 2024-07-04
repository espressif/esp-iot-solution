LCD Application Solution
========================================

:link_to_translation:`en:[English]`

LCD 方案介绍
-----------------------

乐鑫 HMI 智能屏 (LCD) 方案具有卓越性能和可扩展性，可与不同 ESP 主控芯片搭配。该方案在智能家居控制、家电屏幕、医疗设备、工业控制和儿童教育等多个应用场景下表现出色。优势包括高性能图形可视化，低内存占用等。此外，屏幕适配方案完善，并支持高性能 JPEG 解码和帧率优化。以下是 LCD 方案具体介绍：

- **出色的图形可视化：** 利用 ESP 芯片的高性能图形处理能力，并深度合作于 LVGL 官方，使其在 LVGL 兼容性方面表现优异。该方案提供精美的视觉效果，低内存占用，并可轻松移植到产品设计中。

- **简单的 UI 设计：** UI 编辑器 Squareline Studio 支持快速轻松地设计和开发嵌入式设备的 UI。移植简单，无需代码即可实现 UI，最大限度地减少开发时间。

- **丰富的软硬件参考：** 提供全面的 LCD 软硬件开发资料，包括详尽的指导文档和示例。此外，特定为各种 HMI 应用场景设计的 HMI 开发板可帮助开发者快速上手。

- **完善的屏幕适配：** 支持多种操作方式，包括触摸、旋钮等。支持多种外设接口，如 RGB、SPI 等。已适配了多款 LCD 驱动 IC 和 Touch 驱动 IC 并已经整理成组件，满足不同用户群体的需求。

此外，乐鑫还支持以下功能来进一步保证在 LCD 应用场景下有更丝滑的交互体验：

- **帧率优化和防撕裂技术：** 通过精心优化的帧率控制和防撕裂技术，确保图像显示的流畅性和一致性。

- **高性能 JPEG 解码：** 支持高效的图像处理，确保流畅的多媒体体验。

- **语音唤醒和识别：** 可集成先进的语音识别技术，为用户提供更便捷的交互方式。

以下是一图流方案综述：


.. image:: https://dl.espressif.com/AE/esp-iot-solution/lcd_program_architecture.png
  :alt: 方案架构图
  :align: center

LCD 常见应用场景
----------------

乐鑫 LCD 方案广泛应用于各个领域，包括但不限于：

- **旋钮屏方案：** 针对智能家电产品，传统段码屏和黑白屏升级首选。支持 Wi-Fi、蓝牙，扩展接口可实现串口通讯等功能。典型应用场景为小型家电应用中的旋钮屏。

.. image:: https://dl.espressif.com/AE/esp-iot-solution/knob_screen.png
  :alt: 旋钮屏示例
  :align: center

- **中控屏方案：** 集成 Wi-Fi、BLE、离线语音、RGB LCD 显示，支持离线命令词和连续语音识别。适用于传统 86 面板的升级迭代，构建了集设备控制、开关面板、温控面板、智能遥控器等为一体的智能家居控制中枢。

.. image:: https://dl.espressif.com/AE/esp-iot-solution/central_control.png
  :alt: 中控屏示例
  :align: center

- **可视语音方案：** 使用原生 USB 对接通用 USB 摄像头，在单颗 SoC 上同时实现摄像头数据流读取、JPEG 解码和 RGB 接口屏实时显示，无需增加额外的 USB 芯片。适用于猫眼、智能门铃门锁、电子内窥镜等场景。

.. image:: https://dl.espressif.com/AE/esp-iot-solution/visual_voice.png
  :alt: 可视语音示例
  :align: center

- **高性能多媒体方案：** 该方案采用 ESP32-P4 芯片，支持 MIPI-CSI 和 MIPI-DSI 接口，适用于各种需要高分辨率摄像头和显示的场景。该芯片还集成了多种媒体编码和压缩协议的硬件加速器、硬件像素处理加速器（PPA）和 2D-DMA，适合各种多媒体场景。

.. image:: https://dl.espressif.com/AE/esp-iot-solution/high-performance_multimedia.png
  :alt: 高性能多媒体示例
  :align: center

总结如下：

.. list-table:: LCD 方案概览
   :widths: 10 20 15 20 40 40
   :header-rows: 1

   * - 方案类别
     - 方案名称
     - 主控
     - 屏幕
     - 功能
     - 应用场景
   * - 旋钮屏
     - `2.1 英寸旋钮屏方案（启明） <https://www.bilibili.com/video/BV1TL411w7Vi>`__
     - ESP32-S3
     - 2.1 英寸、480 x 480 RGB 接口屏
     - 支持 Wi-Fi、蓝牙，扩展接口可实现串口通讯、按键、USB 摄像头等功能
     - 智能家电产品，传统段码屏和黑白屏升级首选
   * - 旋钮屏
     - `1.28 英寸旋钮屏方案 <https://www.bilibili.com/video/BV1nG4y1d7Ja>`__
     - ESP32-C3
     - 1.28 英寸、240 x 240 SPI 接口屏
     - 带按压开关的旋转编码器，小封装紧凑设计
     - 小型家电应用中的旋钮屏和小尺寸显示屏的应用场景
   * - 中控屏
     - `智能语音触控面板（86 盒）方案 <https://www.bilibili.com/video/BV12G4y1o7A7>`__
     - ESP32-S3
     - 3.95 英寸、480 x 480 RGB 接口屏
     - 集成 Wi-Fi、BLE、离线语音、RGB LCD 显示，支持离线命令词和连续语音识别
     - 传统 86 面板的升级迭代，构建了集设备控制、开关面板、温控面板、智能遥控器等为一体的智能家居控制中枢
   * - 中控屏
     - `智能中控开关方案 <https://www.bilibili.com/video/BV1g44y1S7zn>`__
     - ESP32-S3
     - 7 英寸、800 x 480 RGB 接口屏
     - 多点触摸屏实现手势动作识别，支持 Wi-Fi CSI 人体接近感应，用于家庭可视面板的搭建
     - 智能家庭中的快捷开关控制，如场景模式切换和灯开关
   * - 可视语音方案
     - `可视语音方案 <https://www.bilibili.com/video/BV1ZM411k7tZ>`__
     - ESP32-S3
     - 4.3 英寸、800 x 480 RGB 接口屏
     - 使用原生 USB 对接通用 USB 摄像头，在单颗 SoC 上同时实现摄像头数据流读取、JPEG 解码和 RGB 接口屏实时显示，无需增加额外的 USB 芯片，本地视频解码及屏幕刷新分辨率可达 800 x 480 @15 FPS
     - 猫眼，智能门铃门锁，电子内窥镜等使用场景
   * - 高性能多媒体方案
     - `高性能多媒体方案 <https://www.bilibili.com/video/BV18m421s7p4/>`__
     - ESP32-P4
     - 8 英寸、800 x 1280 MIPI-DSI 接口屏
     - 支持 MIPI-CSI 和 MIPI-DSI 接口，适用于各种需要高分辨率摄像头和显示的场景, 应用多种媒体编码和压缩协议的硬件加速器、硬件像素处理加速器（PPA）和 2D-DMA，适合各种多媒体场景
     - 高性能多媒体场景

LCD 参考方案
----------------

ESP-BOX
^^^^^^^^^^^^^^^^^

| **描述:**

语音助手、触摸屏控制器、传感器、红外控制器和智能 Wi-Fi 网关开发的家电控制平台。

| **硬件:**

- 开发板： `ESP32-S3-BOX-3 <https://github.com/espressif/esp-box/blob/master/docs/hardware_overview/esp32_s3_box_3/hardware_overview_for_box_3.md>`__

| **相关链接:**

- 代码仓库： `esp-box <https://github.com/espressif/esp-box/tree/master/examples/factory_demo>`__
- 相关视频： `ESP32-S3-BOX-3 惊喜开箱！ <https://www.bilibili.com/video/BV1aC4y177rf/?spm_id_from=333.999.0.0&vd_source=2dce370e48c5640c913869575b606ebe>`__

| **特性:**

- 基于 LVGL GUI 框架
- 双 mic 远场语音交互，中英文 AI 离线语识别, 可支持 200 多条语音命令
- 集成端到端 AIoT 开发框架 ESP-RainMaker
- Pmod™ 兼容接头支持外围模块，可拓展传感器、红外控制器等
- PSRAM 要求 8 线（8M）

ESP32-C3 旋钮屏
^^^^^^^^^^^^^^^^^

| **描述:**

圆形旋钮屏方案，集成洗衣机、调光器、温控器等常用场景

| **硬件:**

- 开发板： `ESP32-C3-LCDkit <https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32c3/esp32-c3-lcdkit/user_guide.html>`__

| **相关链接:**

- 代码仓库： `esp32-c3-lcdkit <https://github.com/espressif/esp-dev-kits/tree/master/esp32-c3-lcdkit/examples/knob_panel>`__
- 相关视频：
    - `ESP32-C3 旋钮屏 Demo <https://www.bilibili.com/video/BV1nG4y1d7Ja/?share_source=copy_web&vd_source=0f97bc013051255d733b8e1e7bf54008>`__
    - `ESP32-C3-LCDKit旋钮屏开发板 <https://www.bilibili.com/video/BV1GM4y177A6/?spm_id_from=333.999.0.0&vd_source=2dce370e48c5640c913869575b606ebe>`__

| **特性:**

- 基于 LVGL GUI 框架
- 圆屏 UI 显示（非触摸），旋转编码器控制

智能语音触控面板（86 盒）
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

| **描述:**

可用于传统 86 面板的升级迭代，构建了集设备控制、开关面板、温控面板、智能遥控器等为一体的智能家居控制中枢

| **硬件:**

- 开发板： `ESP32-S3-LCD-EV-Board <https://docs.espressif.com/projects/espressif-esp-dev-kits/en/latest/esp32s3/esp32-s3-lcd-ev-board/user_guide.html>`__
- 屏幕： LCD 子板 2（480x480）

| **相关链接:**

- 代码仓库： `esp32-s3-lcd-ev-board/86-box Smart Panel Example <https://github.com/espressif/esp-dev-kits/tree/master/esp32-s3-lcd-ev-board/examples/86box_smart_panel>`__
- 相关视频： `ESP32-S3 智能语音触控面板 <https://www.bilibili.com/video/BV12G4y1o7A7/?share_source=copy_web&vd_source=0f97bc013051255d733b8e1e7bf54008>`__

| **特性:**

- 基于 LVGL GUI 框架
- 双 mic 远场语音交互，中英文 AI 离线语音识别, 可支持 200 多条语音命令
- PSRAM 要求 8 线（R8），并开启 120M

电子可视门铃
^^^^^^^^^^^^

| **描述:**

使用原生 USB 对接通用 USB 摄像头，在单颗 SoC 上同时实现摄像头数据流读取、JPEG 解码和 RGB 接口屏实时显示，无需增加额外的 USB 芯片，本地视频解码及屏幕刷新分辨率可达 800x480@15 FPS

| **硬件:**

- 开发板： `ESP32-S3-LCD-EV-Board <https://docs.espressif.com/projects/espressif-esp-dev-kits/en/latest/esp32s3/esp32-s3-lcd-ev-board/user_guide.html>`__
- 屏幕： LCD 子板 3（800x480）

| **相关链接:**

- 代码仓库： `esp32-s3-lcd-ev-board/USB Camera LCD Example <https://github.com/espressif/esp-dev-kits/tree/master/esp32-s3-lcd-ev-board/examples/usb_camera_lcd>`__
- 相关视频： `ESP32-S3 驱动 RGB 接口屏 + USB CDC 摄像头 Demo <https://www.bilibili.com/video/BV1ZM411k7tZ/?share_source=copy_web&vd_source=0f97bc013051255d733b8e1e7bf54008>`__

| **特性:**

- USB 摄像头数据流读取，需要支持 Bulk 模式
- JPEG 解码
- 800x480 RGB LCD 显示
- PSRAM 要求 8 线（R8），并开启 120M

智能中控开关
^^^^^^^^^^^^

| **描述:**

通过多点触摸屏实现双指叩击、拍一拍等手势动作识别，可用于智能家庭中的快捷开关控制，如场景模式切换和灯开关。 结合 Wi-Fi CSI人体接近感应功能，还可以实现屏幕接近亮屏和远离息屏的自动开关控制

| **硬件:**

- 开发板： `ESP32-S3-LCD-EV-Board <https://docs.espressif.com/projects/espressif-esp-dev-kits/en/latest/esp32s3/esp32-s3-lcd-ev-board/user_guide.html>`__
- 屏幕： 7 英寸、RGB 接口、800x480 分辨率

| **相关链接:**

- 相关视频： `ESP32-S3驱动超大RGB接口屏 <https://www.bilibili.com/video/BV1g44y1S7zn/?share_source=copy_web&vd_source=0f97bc013051255d733b8e1e7bf54008>`__

| **特性:**

- 7 英寸超大 LCD 屏幕，支持多点触摸
- Wi-Fi CSI 人体接近感应
- PSRAM 要求 8 线（R8），并开启 120M

高性能多媒体方案
^^^^^^^^^^^^^^^^^^^^

| **描述:**

支持 MIPI-CSI 和 MIPI-DSI 接口，适用于各种需要高分辨率摄像头和显示的场景, 应用多种媒体编码和压缩协议的硬件加速器、硬件像素处理加速器（PPA）和 2D-DMA，适合各种多媒体场景

| **硬件:**

- 开发板: ESP32-P4_Function_EV_Board
- 屏幕：8 英寸 800 x 1280 液晶屏（IC: ILI9881C ）

| **相关链接:**

- 相关视频： `挑战用 ESP32-P4 做一部智能手机 <https://www.bilibili.com/video/BV18m421s7p4/>`__

| **特性:**

- 支持 MIPI-DSI 和 MIPI-CSI 接口
- 多种媒体编码和压缩协议的硬件加速器
- 硬件像素处理加速器（PPA）和 2D-DMA

LCD 参考资料
^^^^^^^^^^^^^^

- LCD 软件参考

  - `ESP LCD 驱动库 <https://github.com/espressif/esp-idf/tree/master/components/esp_lcd>`_
  - `Arduino LCD 驱动库 <https://github.com/esp-arduino-libs/ESP32_Display_Panel>`_
  - `ESP LCD 驱动文档 <https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-reference/peripherals/lcd/index.html>`_
  - `ESP LCD 例程 <https://github.com/espressif/esp-idf/tree/master/examples/peripherals/lcd>`_
  - `ESP-BOX AIoT 开发框架 <https://github.com/espressif/esp-box>`_

- LCD 方案 & 开发指南

  - `ESP-HMI 智能屏方案 <https://www.espressif.com/zh-hans/solutions/hmi/smart-displays>`_
  - `快速入门 GUI（上） <https://www.bilibili.com/read/cv19147389?spm_id_from=333.999.0.0>`_
  - `快速入门 GUI（下） <https://www.bilibili.com/read/cv19354117?spm_id_from=333.999.0.0>`_
  - `ESP LCD 开发指南 <https://docs.espressif.com/projects/espressif-esp-iot-solution/zh_CN/latest/display/screen.html>`_

- LCD 相关开发板购买

  - `ESP32-S3-LCD-EV-Board <https://docs.espressif.com/projects/espressif-esp-dev-kits/zh_CN/latest/esp32s3/esp32-s3-lcd-ev-board/user_guide.html>`_：目前支持 800 x 480 4.3 寸（RGB） 和 480 x 480 3.95 寸（RGB）两种子板，支持电容触屏。 `购买链接 <https://item.taobao.com/item.htm?spm=a1z10.5-c.w4002-8715811636.23.4bc567d8eBiLiI&id=680580609719>`_
  - `ESP32-S3-BOX <https://github.com/espressif/esp-box/blob/master/docs/hardware_overview/esp32_s3_box/hardware_overview_for_box.md>`_ ： 240 x 320 2.4 寸（SPI）ILI9342，支持电容触屏。 `购买链接 <https://item.taobao.com/item.htm?spm=a1z10.1-c.w4004-10057817917.13.36b47931Jgygni&id=732842971319&skuId=5456418374248>`__
  - `ESP32-S3-BOX-Lite <https://github.com/espressif/esp-box/blob/master/docs/getting_started_cn.md>`_ : 240 x 320 2.4 寸（SPI）ST7789V，不支持触屏，但是屏上有 3 个按键。 `购买链接 <https://item.taobao.com/item.htm?spm=a1z10.3-c.w4002-8715811646.10.1b7719b0Af0aPp&id=658634202331>`__
  - `ESP32-C3-LCDkit <https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32c3/esp32-c3-lcdkit/user_guide.html>`_ : 240 x 240 1.28 寸（SPI）GC9A01，不支持触屏。 `购买链接 <https://item.taobao.com/item.htm?spm=a1z10.5-c.w4002-8715811636.21.74eb570eCig1wf&id=722089055506>`__

- 模组/开发板资料及选项参考

  - `ESP32-S3 技术规格书 <https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_cn.pdf>`_
  - `ESP32-S3-WROOM-1 <https://www.espressif.com/sites/default/files/documentation/esp32-s3-wroom-1_wroom-1u_datasheet_cn.pdf>`_
  - `ESP32-C3 技术规格书 <https://www.espressif.com/sites/default/files/documentation/esp32-c3_datasheet_cn.pdf>`_
  - `ESP32-C3-MINI-1 <https://www.espressif.com/sites/default/files/documentation/esp32-c3-mini-1_datasheet_cn.pdf>`_
  - `乐鑫产品选型工具 <https://products.espressif.com/>`_