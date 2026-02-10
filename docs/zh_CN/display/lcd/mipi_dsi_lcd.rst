MIPI DSI LCD 详解
===========================

:link_to_translation:`en:[English]`

.. contents:: 目录
    :local:
    :depth: 2

术语表
-----------

请参阅 :ref:`LCD 术语表 <LCD_术语表>` 。

MIPI-DSI 相关术语
~~~~~~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 15 25 60

   * - 缩写
     - 全称
     - 说明
   * - MIPI-DSI
     - MIPI Display Serial Interface
     - MIPI 联盟开发的高速串行接口标准，专门用于连接处理器和显示设备
   * - DCS
     - Display Command Set
     - MIPI-DSI 定义的一套标准化显示指令集，用于控制显示模块
   * - DBI
     - Display Bus Interface
     - 将 DSI 的 Command Mode 封装成 esp-lcd 的控制 IO 层
   * - DPI
     - Display Pixel Interface
     - 将 DSI 的 Video Mode 封装成 esp-lcd 的数据面板层
   * - HS
     - High-Speed Mode
     - 高速模式，主要用于传输显示帧数据，速率一般为 80 Mbps ~ 1.5 Gbps/Lane
   * - LP
     - Low-Power Mode
     - 低功耗模式，通常用于发送 DCS 命令或控制链路状态，速率最大约 10 MHz
   * - HS-TX/HS-RX
     - High-Speed Transmitter/Receiver
     - 高速发送/接收器
   * - LP-TX/LP-RX
     - Low-Power Transmitter/Receiver
     - 低功耗发送/接收器
   * - LP-CD
     - Low-Power Contention Detector
     - 低功耗竞争检测器
   * - BTA
     - Bus Turnaround
     - 总线翻转，用于双向通信
   * - ULPS
     - Ultra Low Power State
     - 超低功耗状态，用于最大限度节省能耗
   * - LPDT
     - Low Power Data Transmission
     - 低功耗数据传输模式
   * - ESC
     - Escape Mode
     - 转义模式，MIPI-DSI 的低功耗操作模式
   * - SoT
     - Start of Transmission
     - 传输开始信号
   * - EoT
     - End of Transmission
     - 传输结束信号
   * - DI
     - Data Identifier
     - 数据标识，用于标识数据包类型
   * - ECC
     - Error Correction Code
     - 错误检测码，用于数据完整性校验
   * - DT
     - Data Type
     - 数据类型，用于标识像素数据格式
   * - HSYNC
     - Horizontal Synchronization
     - 水平同步信号
   * - VSYNC
     - Vertical Synchronization
     - 垂直同步信号
   * - DMA
     - Direct Memory Access
     - 直接内存访问，用于高效数据传输
   * - PSRAM
     - Pseudo Static RAM
     - 伪静态随机存取存储器
   * - RGB565
     - Red Green Blue 565
     - 16位色彩格式，5位红色，6位绿色，5位蓝色
   * - RGB666
     - Red Green Blue 666
     - 18位色彩格式，红绿蓝各6位
   * - RGB888
     - Red Green Blue 888
     - 24位色彩格式，红绿蓝各8位

MIPI-DSI 接口与协议介绍
---------------------------

MIPI-DSI 接口简介
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

MIPI-DSI (MIPI Display Serial Interface) 是由 MIPI 联盟开发的高速串行接口标准,专门用于连接处理器和显示设备。它具有以下特点:

* 高速传输: 每个数据通道可支持高达数 Gbps 的传输速率
* 低功耗: 采用低压差分信号，在空闲或低速传输时进入低功耗模式
* 引脚精简: 相比并行 RGB 接口，大大减少了信号线数量
* 可扩展性: 支持 1 至 4 个数据通道
* 高兼容性: 支持多种分辨率和显示设备

目前在乐鑫芯片中, ESP32-P4 拥有 MIPI-DSI 接口, 支持 2-lane × 1.5Gbps，并使用 Video 模式输出视频流。

MIPI-DSI 屏幕刷新原理
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

MIPI-DSI 是一种常用的显示接口标准，用于连接处理器和显示屏。它支持两种主要的数据传输模式：

1. Command 模式（Command Mode）
2. Video 模式（Video Mode）

其中 ESP32-P4 目前仅支持使用 Video 模式输出视频流，

在 Video 模式（Video Mode）中，主机以连续视频帧的形式传输数据，与显示屏的刷新周期（如 VSync）严格同步。显示屏无需显存，硬件设计简单。该模式被广泛用于高分辨率、高刷新率的显示场景。

.. figure:: https://dl.espressif.com/AE/esp-dev-kits/mipi_dsi_video.png
    :align: center
    :alt: MIPI-DSI 屏幕刷新的简要流程

    MIPI-DSI 屏幕刷新的简要流程

上图展示了 MIPI-DSI 屏幕刷新的简要流程，其中：

* 命令数据：通过 DBI 传输 MIPI-DCS 命令到 LCD 控制器，用于初始化寄存器和控制显示功能。
* 帧颜色数据：在 MIPI-DSI 视频模式下，像素数据由主控侧帧缓冲（内/外部 RAM）通过 DMA 连续输出到 LCD 面板进行显示，LCD 驱动 IC 则无需 GRAM。
* 触摸数据：触控信号由触摸屏检测后，通过 I²C 或 SPI 接口传回主控系统进行处理。

MIPI-DSI 协议
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

MIPI-DSI 可大致分为物理层（Physical Layer）、协议层（Protocol Layer）和显示命令层（DCS Layer），每层都有特定功能，协同完成从主控到显示屏的命令与像素数据传输。

各层的协作流程可概括为下图：

.. figure:: https://dl.espressif.com/AE/esp-dev-kits/mipi_dsi_layer.png
    :align: center
    :alt: MIPI-DSI 协议分层

    MIPI-DSI 协议分层

物理层（Physical Layer）
^^^^^^^^^^^^^^^^^^^^^^^^

* 功能：负责实现数据的实际传输，是协议栈的底层。采用 MIPI D-PHY 提供高速差分信号传输，支持多通道数据传输。

* 传输模式：
  
  * 高速模式（High-Speed Mode）：主要用于传输显示帧数据，也可传输命令，速率一般为 80 Mbps ~ 1.5 Gbps/Lane。
  * 低功耗模式（Low-Power Mode）：通常用于发送 DCS 命令（命令模式下）或控制链路状态，速率最大可达约 10 MHz（具体取决于芯片支持）。

* 配置：
  
  * 时钟通道 (Clock Lane)：提供同步信号。
  * 数据通道 (Data Lane)：传输实际显示数据。

* 关于通道模块（Lane Module），即 D-PHY 的详细说明
  
  * 类型
    
    .. list-table::
        :widths: 20 35 35
        :header-rows: 1

        * - lane 类型/传输角色
          - Master
          - Slave
        * - 单向时钟 Lane
          - HS-TX, LP-TX
          - HS-RX, LP-RX
        * - 单向数据 Lane
          - HS-TX, LP-TX
          - HS-RX, LP-RX
        * - 双向数据 Lane
          - HS-TX, LP-TX, HS-RX, LP-RX, LP-CD
          - HS-TX, LP-TX, HS-RX, LP-RX, LP-CD
    
.. note::
   各模块功能说明：
   
   - **LP-TX/LP-RX**：低功耗发送/接收器
   - **HS-TX/HS-RX**：高速发送/接收器  
   - **LP-CD**：低功耗竞争检测器
   
   时钟 Lane 通常是单向的，由 Master 提供，Slave 接收。双向 Lane 和 LP-CD 主要用于命令模式下的低功耗通信。
  
* 状态

    * LP 状态 (LP-00, LP-01, LP-10, LP-11)：使用单端信号，主要用于低速命令传输、控制信号、初始化或待机状态。不同的 LP 状态用于编码和链路控制。
    * HS 状态 (HS-0, HS-1)：使用差分信号，主要用于高速传输视频数据或帧内容。HS-0 和 HS-1 对应差分对上的逻辑电平，仅在 HS-TX/HS-RX 上有效。
  
* 典型电压

    * LP：典型电压约 0 – 1.2 V
    * HP：典型差分电压约 100 – 300 mV
  
* 操作模式

    * Escape Mode：只能在 Low-Power Mode，MIPI-DSI 的低功耗操作模式，用于特殊数据传输或触发特定功能。可支持以下功能：
        
        +----------------------------+--------------------------------------+----------+
        | 功能名称                   | 描述                                 | 对应命令 |
        +============================+======================================+==========+
        | LPDT（低功耗数据传输）     | 在低功耗模式下传输数据               | 0x87     |
        +----------------------------+--------------------------------------+----------+
        | ULPS（超低功耗状态）       | 进入超低功耗状态以最大限度节省能耗   | 0x84     |
        +----------------------------+--------------------------------------+----------+
        | Trigger Command            | 触发特定事件，例如状态切换或模式调整 | 0x85     |
        +----------------------------+--------------------------------------+----------+
        
        * 进入过程（信号状态机过渡示意）：LP-11 → LP-10 → LP-00 → LP-01 → LP-00
        * 退出过程：LP-10 → LP-11

    * High-Speed (Burst) Mode：始终在 High-Speed Mode ，用于高速串行数据传输，适合大带宽需求。
        
        * 进入过程：LP-11 → LP-01 → LP-00 → SoT (Start of Transmission) → HSD (80Mbps ~ 1.5Gbps)
        * 退出过程：EoT (End of Transmission) → LP-11

    * Control Mode：默认在 Low-Power Mode，用于停止状态下的命令传输和双向通信（BTA，Bus Turnaround）。
        
        * BTA 传输过程：LP-11 → LP-10 → LP-00 → LP-10 → LP-00（总线翻转状态过渡）
        * BTA 接收过程：LP-00 → LP-10 → LP-11

**重要信号说明**

在 MIPI-DSI 通信过程中，两种关键的 Stop 信号确保了总线的正确状态管理：

.. figure:: https://dl.espressif.com/AE/esp-dev-kits/mipi_dsi_stop.png
    :align: center
    :alt: MIPI-DSI Stop 信号时序图

    MIPI-DSI Stop 信号时序图

#. **BTA-Stop 信号**
   
   当从机完成 Bus Turnaround 响应后发送此信号，表示：
   
   - 总线翻转操作已成功完成
   - 从机已释放总线控制权
   - 主机可以重新获得总线控制

#. **ESC-0X84-Stop 信号**
   
   用于指示 Lane 停止信号传输，具体功能包括：
   
   - 使系统进入超低功耗状态（ULPS）
   - 对应 Escape Mode 中的 0x84 命令
   - 实现最大程度的节能管理

.. tip::
   这些 Stop 信号是 MIPI-DSI 协议中确保通信可靠性和功耗管理的关键机制。

协议层（Protocol Layer）
^^^^^^^^^^^^^^^^^^^^^^^^^

* 功能：实现数据打包和解析，定义传输格式（如 DCS 和视频数据包），并提供错误检测机制以确保数据的正确性和可靠性。

* 数据帧结构
  
  * 短包（Short Packet）：
    
    * 长度：固定长度为 4 字节
    * 组成：数据标识（DI），1 字节；帧数据，2 字节；错误检测（ECC），1 字节。
    
    .. figure:: https://dl.espressif.com/AE/esp-dev-kits/mipi_dsi_short_packet.png
        :align: center
        :alt: MIPI-DSI 短帧组成

        MIPI-DSI 短帧组成

    * 示例：
    
    .. figure:: https://dl.espressif.com/AE/esp-dev-kits/mipi_dsi_short_packet_example.png
        :align: center
        :alt: MIPI-DSI 短帧时序

        MIPI-DSI 短帧时序

    该短帧示例的关键字段如下（按出现顺序）：

    - ESC：进入 Escape mode
    - 0x87：LPDT（低功耗数据传输模式）
    - 0x37：数据标识，对应 DCS 命令 "Set Maximum Return Packet Size"，功能：设置主机从从机接收数据时返回数据包的最大大小
    - 0x03, 0x00：帧数据，表示设置返回数据包的最大大小为 3 字节
    - 0x01：ECC（错误检测码）
  
  * 长包（Long Packet）：
    
    * 长度：长度可变，6 至 65541 字节。
    * 组成：帧头（数据标识（DI）1 字节、数据计数（WC）2 字节、错误检测（ECC）1 字节）；数据填充（0 至 65535 字节）；帧尾（校验和（Checksum）2 字节）。
    
    .. figure:: https://dl.espressif.com/AE/esp-dev-kits/mipi_dsi_long_paket.png
        :align: center
        :alt: MIPI-DSI 长帧组成

        MIPI-DSI 长帧组成

    * 示例：
    
    .. figure:: https://dl.espressif.com/AE/esp-dev-kits/mipi_dsi_long_packet_example.png
        :align: center
        :alt: MIPI-DSI 长帧时序

        MIPI-DSI 长帧时序

    该长帧示例的关键字段如下（按出现顺序）：

    - ESC：进入 Escape mode
    - 0x87：LPDT（低功耗数据传输模式）
    - 0x39：数据标识，对应 DCS 命令 "DCS Write Long"
    - 0x03, 0x00：数据计数，表示要发送 3 字节的数据命令
    - 0x09：ECC（错误检测码）
    - 0xB6, 0xB2, 0xB2：实际发送的数据命令
    - 0xEF, 0xFA：校验和字段

* 常见的数据标识（DI）

.. list-table::
   :header-rows: 1
   :widths: 15 30 55

   * - 数据标识 (Data ID)
     - 名称
     - 说明
   * - 0x05
     - DCS Short Write, 0 Param
     - 发送一个 DCS 命令，无参数（例如 Sleep Out `0x11`）
   * - 0x15
     - DCS Short Write, 1 Param
     - 发送一个 DCS 命令，带 1 个参数
   * - 0x39
     - DCS Long Write
     - 发送一个 DCS 命令，附带多个字节数据（常用于内存写入）
   * - 0x06
     - DCS Short Read, 0 Param
     - 读取一个 DCS 状态或寄存器，无参数
   * - 0x16
     - DCS Short Read, 1 Param
     - 读取一个 DCS 状态或寄存器，带 1 个参数
   * - 0x37
     - DCS Read Response
     - 显示器返回的 DCS 读数据
   * - 0x03
     - Generic Short Write, 0 Param
     - 发送通用命令，无参数
   * - 0x13
     - Generic Short Write, 1 Param
     - 发送通用命令，带 1 个参数
   * - 0x23
     - Generic Short Write, 2 Param
     - 发送通用命令，带 2 个参数
   * - 0x29
     - Generic Long Write
     - 发送通用命令，附带多个字节数据
   * - 0x04
     - Generic Read, 0 Param
     - 通用读命令，无参数
   * - 0x14
     - Generic Read, 1 Param
     - 通用读命令，带 1 个参数
   * - 0x24
     - Generic Read, 2 Param
     - 通用读命令，带 2 个参数

显示命令层（DCS Layer）
^^^^^^^^^^^^^^^^^^^^^^^^

* 功能：DCS（Display Command Set）是 MIPI-DSI 定义的一套标准化显示指令集，主控通过这些命令与显示屏交互，用于实现显示模块的基本控制（如开关机、休眠、背光、显示开关）、显示参数配置（如像素格式、显示模式、地址设置），以及显存访问（如写入图像数据、读取状态信息）。DCS 命令通过短写、长写和读命令的形式在 DSI 总线上传输，是应用层与显示硬件之间的主要控制接口。

* 常见命令：

.. list-table::
   :header-rows: 1
   :widths: 10 25 40

   * - 命令码
     - 名称
     - 功能说明
   * - 0x01
     - Software Reset
     - 软件复位
   * - 0x11
     - Sleep Out
     - 退出休眠模式
   * - 0x28
     - Display Off
     - 关闭显示
   * - 0x29
     - Display On
     - 打开显示
   * - 0x2A
     - Column Address Set
     - 设置列地址范围
   * - 0x2B
     - Page Address Set
     - 设置页地址范围
   * - 0x2C
     - Memory Write
     - 写入显存数据
   * - 0x2E
     - Memory Read
     - 读取显存数据
   * - 0x36
     - Address Mode
     - 设置内存扫描方向与翻转方式
   * - 0x3A
     - Pixel Format Set
     - 设置像素格式（如 RGB565/888）

色彩格式
---------------------------

.. figure:: https://dl.espressif.com/AE/esp-dev-kits/mipi_dsi_color_format.png
    :align: center
    :alt: MIPI-DSI 色彩格式

    MIPI-DSI 色彩格式

大多数 MIPI-DSI LCD 支持多种输入色彩格式，如 **RGB565、RGB666、RGB888**。  
主控可通过 **DCS 命令 0x3A（Pixel Format Set）** 配置显示的颜色深度，例如：

- 设置为 **RGB565（16-bit）** 时，可发送 ``Command: 0x3A``，``Parameter: 0x55`` （具体参数值需参考 LCD 控制器规格书）。

像素数据通常通过 **长数据包（Long Packet）** 传输，数据包中包含像素数据和对应的数据类型（Data Type）。  
例如，RGB565 格式的像素流一般使用 **DT = 0x0E（Packed Pixel Stream, 16-bit）**。

说明
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- **16-bit 与 18-bit** 格式常用于降低带宽和存储占用  
- **24-bit** 格式适用于高质量显示场景

常见色彩格式与数据类型
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 15 15 20 25

   * - 色彩格式
     - 位深
     - DCS 参数（例）
     - Data Type (DT)
   * - RGB565
     - 16-bit
     - 0x55
     - 0x0E
   * - RGB666
     - 18-bit
     - 0x66
     - 0x1E / 0x2E
   * - RGB888
     - 24-bit
     - 0x77
     - 0x3E

MIPI-DSI 驱动流程
---------------------------

MIPI-DSI LCD 驱动流程可大致分为四个部分：初始化 DSI 总线、配置 DBI 接口、移植驱动组件和初始化 LCD 设备。

DSI、DBI 与 DPI
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* DSI：负责 PHY、Host、Bridge 的底层初始化，它是 DBI 与 DPI 的公共“母线”。  

* DBI：将 DSI 的 Command Mode 封装成 esp-lcd 的 控制 IO 层

* DPI：将 DSI 的 Video Mode 封装成 esp-lcd 的 数据面板层，负责帧缓冲到屏的持续刷新

初始化 DSI 总线
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

    esp_lcd_dsi_bus_handle_t dsi_bus = NULL;
    esp_lcd_dsi_bus_config_t bus_config = {
        .bus_id = 0,                                   // DSI 控制器编号
        .num_data_lanes = 2,                           // 数据通道数量(1-2)
        .phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,   // PHY 时钟源
        .lane_bit_rate_mbps = 1300,                    // 每通道比特率(Mbps)
    };
    ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_config, &dsi_bus));

其中数据通道数量（num_data_lanes）值取决于 LCD IC 所使用并支持的通道数，部分 LCD IC 可通过寄存器配置所使用的通道数，而一些 LCD IC 所使用的通道数是固定的。值的注意的是，ESP32-P4 最大支持 2 lanes，即支持 1-lane 和 2-lane 两种 LCD 通道配置。

每通道比特率(lane_bit_rate_mbps)计算方法：

+ 示例计算:
  
  * 分辨率: 800×1280（hspw=4, hbp=20, hfp=20；vspw=4, vbp=20, vfp=20）
  * 刷新率: 60Hz
  * 色深: 24bpp
  * 数据通道数: 2

* 计算原始数据速率
  
  * pixel_clock = 水平总像素 × 垂直总像素 × 刷新率
  * raw_data_rate = pixel_clock × bits_per_pixel
    
    示例计算：
    
    * pixel_clock = (800+20+20+4) × (1280+20+20+4) × 60 = 62.4MHz
    * raw_data_rate = 62.4M × 24 = 1497.6Mbps

* 考虑 DSI 协议开销
  
  * 8b/10b 编码: 实际数据率需要乘以 1.25
  * 协议包头和 ECC: 额外增加约 10-15% 开销
  * protocol_data_rate = raw_data_rate × 1.25 × 1.15
    
    示例计算：
    
    * protocol_data_rate = 1497.6 × 1.25 × 1.15 = 2153.55Mbps

* 分配到各数据通道
  
  * lane_bit_rate = protocol_data_rate ÷ num_data_lanes
    
    示例计算：
    
    * lane_bit_rate = 2153.55 ÷ 2 = 1076.78Mbps

* 预留带宽余量(建议 20%)
  
  * final_bit_rate = lane_bit_rate × 1.2
    
    示例计算：
    
    * final_bit_rate = 1076.78 × 1.2 ≈ 1292Mbps

因此建议 lane_bit_rate_mbps 值取 final_bit_rate，在示例计算场景下为 `1300` 左右。注意，在 ESP32P4 平台下，lane_bit_rate_mbps 最大值不能超过 `1500`，同时推荐最小值不要低于 `480`。

配置 DBI 接口
~~~~~~~~~~~~~

.. code-block:: c

    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_dbi_io_config_t io_config = {
        .virtual_channel = 0,           // 虚拟通道号（0-3），大多数 LCD 仅支持 `0` 通道，通常设为 `0` 即可
        .lcd_cmd_bits = 8,              // 命令位宽
        .lcd_param_bits = 8,            // 参数位宽
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(dsi_bus, &io_config, &io_handle));

其中命令和参数位宽必须与面板规格匹配，虚拟通道用于区分多个设备。

移植驱动组件
~~~~~~~~~~~~

移植 MIPI-DSI LCD 驱动组件的基本原理包含以下三点：

  1. 基于数据类型为 ``esp_lcd_dbi_io_config_t`` 的接口设备句柄发送指定格式的命令及参数。
  2. 实现并创建一个 LCD 设备，然后通过注册回调函数的方式实现结构体 `esp_lcd_panel_t <https://github.com/espressif/esp-idf/blob/release/v5.1/components/esp_lcd/interface/esp_lcd_panel_interface.h>`_ 中的各项功能。
  3. 实现一个函数用于提供数据类型为 ``esp_lcd_panel_handle_t`` 的 LCD 设备句柄，使得应用程序能够利用 `LCD 通用 APIs <https://github.com/espressif/esp-idf/blob/release/v5.1/components/esp_lcd/include/esp_lcd_panel_ops.h>`_ 来操作 LCD 设备

下面是 ``esp_lcd_panel_handle_t`` 各项功能的实现说明以及和 `LCD 通用 APIs <https://github.com/espressif/esp-idf/blob/release/v5.1/components/esp_lcd/include/esp_lcd_panel_ops.h>`_ 的对应关系：

对于大多数 MIPI-DSI LCD，其驱动 IC 的命令及参数与上述实现说明中的兼容，因此可以通过以下步骤完成移植：

1. 在 `LCD 驱动组件 <https://github.com/espressif/esp-iot-solution/blob/master/docs/zh_CN/display/lcd/lcd_development_guide.rst#%E9%A9%B1%E5%8A%A8%E5%8F%8A%E7%A4%BA%E4%BE%8B>`_ 中选择一个型号相似的 MIPI-DSI LCD 驱动组件。
2. 通过查阅目标 LCD 驱动 IC 的数据手册，确认其与所选组件中各功能使用到的命令及参数是否一致，若不一致则需要修改相关代码。
3. 即使 LCD 驱动 IC 的型号相同，不同制造商的屏幕也通常需要使用各自提供的初始化命令配置。因此，需要修改初始化函数 ``init()`` 中发送的命令和参数。这些初始化命令通常以特定的格式存储在一个静态数组中。此外，需要注意不要在初始化命令中包含一些特殊的命令，例如 ``LCD_CMD_COLMOD(3Ah)`` 和 ``LCD_CMD_MADCTL(36h)``，这些命令是由驱动组件进行管理和使用的。
4. 可使用编辑器的字符搜索和替换功能，将组件中的 LCD 驱动 IC 名称替换为目标名称，如将 ``ek79007`` 替换为 ``ili9881``

初始化 LCD 设备
~~~~~~~~~~~~~~~

下面以 `EK79007 <https://components.espressif.com/components/espressif/esp_lcd_ek79007>`_ 为例的代码说明：

.. code-block:: c

    #include "esp_lcd_panel_vendor.h"   // 依赖的头文件
    #include "esp_lcd_panel_ops.h"
    #include "esp_lcd_ek79007.h"        // 目标驱动组件的头文件

    // static const ek79007_lcd_init_cmd_t lcd_init_cmds[] = {
    //  {cmd, { data }, data_size, delay_ms}
    // {0xE0, (uint8_t []){0x00}, 1, 0},
    // {0xE1, (uint8_t []){0x93}, 1, 0},
    // {0xE2, (uint8_t []){0x65}, 1, 0},
    // {0xE3, (uint8_t []){0xF8}, 1, 0},
    //     ...
    // };

        ESP_LOGI(TAG, "MIPI DSI PHY Powered on");
        esp_ldo_channel_handle_t ldo_mipi_phy = NULL;
        esp_ldo_channel_config_t ldo_mipi_phy_config = {
            .chan_id = 3,
            .voltage_mv = 2500,
        };
        ESP_ERROR_CHECK(esp_ldo_acquire_channel(&ldo_mipi_phy_config, &ldo_mipi_phy));

        ESP_LOGI(TAG, "Initialize MIPI DSI bus");
        esp_lcd_dsi_bus_handle_t mipi_dsi_bus = NULL;
        esp_lcd_dsi_bus_config_t bus_config = EK79007_PANEL_BUS_DSI_2CH_CONFIG();
        ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_config, &mipi_dsi_bus));

        ESP_LOGI(TAG, "Install panel IO");
        esp_lcd_panel_io_handle_t mipi_dbi_io = NULL;
        esp_lcd_dpi_panel_config_t dpi_config = { 
            .virtual_channel = 0,
            .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
            .dpi_clock_freq_mhz = 60,                 // 像素时钟频率
            .in_color_format = LCD_COLOR_FMT_RGB888,  // 色彩格式
            .video_timing = {
                .h_size = 800,                        // 水平分辨率
                .v_size = 1280,                       // 垂直分辨率
                .hsync_pulse_width = 4,               // HSYNC 脉冲宽度
                .hsync_back_porch = 20,               // 水平后肩
                .hsync_front_porch = 20,              // 水平前肩
                .vsync_pulse_width = 4,               // VSYNC 脉冲宽度
                .vsync_back_porch = 20,               // 垂直后肩
                .vsync_front_porch = 20,              // 垂直前肩
            },
            .flags.use_dma2d = true,                  // 使用 2D DMA 加速
        };
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(mipi_dsi_bus, &dbi_config, &mipi_dbi_io));

        ESP_LOGI(TAG, "Install EK79007S panel driver");
        esp_lcd_panel_handle_t panel_handle = NULL;
        ek79007_vendor_config_t vendor_config = {
            .flags = {
                .use_mipi_interface = 1,
            },
            .mipi_config = {
                .dsi_bus = mipi_dsi_bus,
                .dpi_config = &dpi_config,
            },
        };
        const esp_lcd_panel_dev_config_t panel_config = {
            .reset_gpio_num = EXAMPLE_LCD_IO_RST,           // Set to -1 if not use
            .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,     // Implemented by LCD command `36h`
            .bits_per_pixel = EXAMPLE_LCD_BIT_PER_PIXEL,    // Implemented by LCD command `3Ah` (16/18/24)
            .vendor_config = &vendor_config,
        };
        ESP_ERROR_CHECK(esp_lcd_new_panel_ek79007(mipi_dbi_io, &panel_config, &panel_handle));
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

**时序参数说明：**

MIPI-DSI 的时序参数定义与 RGB 接口的 `SYNC 模式` 在时序图上是一致的。两者都使用 HSYNC 和 VSYNC 同步信号，并且都需要配置消隐区域（porch）参数。关于时序参数的详细说明、参数与时序图符号的对应关系以及配置方法，请参考 :doc:`RGB LCD 详解 <rgb_lcd>` 中的"时序参数对应关系说明"部分。

注意，在 ESP32P4 中要求为 MIPI DSI PHY 提供 2.5V 的稳定电源。其中时钟频率计算参考上文 pixel_clock 计算方式。色彩格式、分辨率、脉冲宽度和前后肩参数需要严格遵循面板规格书要求。

然后通过移植好的驱动组件创建 LCD 设备并获取数据类型为 ``esp_lcd_panel_handle_t`` 的句柄，然后使用 `LCD 通用 APIs <https://github.com/espressif/esp-idf/blob/release/v5.1/components/esp_lcd/include/esp_lcd_panel_ops.h>`_ 来初始化 LCD 设备。

关于 ``MIPI-DSI`` 接口配置参数更加详细的说明，请参考 `ESP-IDF 编程指南 <https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/api-reference/peripherals/lcd/index.html>`_ 。下面是一些关于使用函数 ``esp_lcd_panel_draw_bitmap()`` 刷新 LCD 图像的说明：

  - 该函数是通过内存拷贝的方式刷新帧缓存里的图像数据，也就是说该函数调用完成后帧缓存内的图像数据也已经更新完成，而 ``MIPI-DSI`` 接口本身是通过 DMA 从帧缓存中获取图像数据来刷新 LCD，这两个过程是异步进行的。
  - 该函数会判断传入参数 ``color_data`` 的值是否为 ``MIPI-DSI`` 接口内部的帧缓存地址，若是，则不会进行上述的内存拷贝操作，而是直接将 ``MIPI-DSI`` 接口的 DMA 传输地址设置为该缓存地址，从而在具有多个帧缓存的情况下实现切换的功能

除了 `LCD 通用 APIs <https://github.com/espressif/esp-idf/blob/release/v5.1/components/esp_lcd/include/esp_lcd_panel_ops.h>`_ 之外， `MIPI-DSI 接口驱动 <https://github.com/espressif/esp-idf/blob/release/v5.1/components/esp_lcd/src/esp_lcd_panel_mipi_dsi.c>`_ 中还提供了一些特殊功能的函数，下面是一些常用函数的使用说明：

  * esp_lcd_dpi_panel_get_frame_buffer()：获取帧缓存的地址，可用数量由配置参数 ``num_fbs`` 决定，用于多缓冲防撕裂。
  * esp_lcd_dpi_panel_set_pattern()：设置预定义的图案到屏幕上，用于测试或调试目的。
  * esp_lcd_dpi_panel_set_color_conversion()：为 DPI 面板设置颜色转换配置。
  * esp_lcd_dpi_panel_register_event_callbacks()：注册多种事件的回调函数，示例代码及说明如下：

.. code-block:: c

    static bool example_on_color_trans_event(esp_lcd_panel_handle_t panel, const esp_lcd_dpi_panel_event_callbacks_t *edata, void *user_ctx)
    {
        /* 可以在此处进行一些操作 */

        return false;
    }

    static bool example_on_refresh_event(esp_lcd_panel_handle_t panel, const esp_lcd_dpi_panel_event_callbacks_t *edata, void *user_ctx)
    {
        /* 可以在此处进行一些操作 */

        return false;
    }

    esp_lcd_dpi_panel_event_callbacks_t cbs = {
        .on_color_trans_done = example_on_color_trans_event,   // 当用户的颜色缓冲区被复制到内部帧缓冲区时触发
        .on_refresh_done = example_on_refresh_event,  		   // 当内部帧缓冲区的内容完成刷新并显示到屏幕上时触发
    };
    ESP_ERROR_CHECK(esp_lcd_dpi_panel_register_event_callbacks(panel_handle, &cbs, &example_user_ctx));

常见问题
---------------------------

问题：屏幕无图像显示，同时无其他报错日志怎么办
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

需要检查 SPIRAM SPEED 值是否配置到 200M。驱动 MIPI-DSI 接口屏幕需要 PSRAM 有更高的带宽。

问题：屏幕初始化命令无法正常全部发出
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

芯片 DSI 的数据引脚(DSI_DATAN0 和 DSI_DATAP0)和时钟引脚(DSI_CLKN 和 DSI_CLKP)与屏幕的数据引脚和时钟引脚没有正确连接。

问题：屏幕画面扭曲错位
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

这种情况一般是 lane_bit_rate_mbps 与 pixel_clock 不匹配，或 lane_bit_rate_mbps 与屏幕要求速率不匹配造成的，可调整 lane_bit_rate_mbps 数值并测试，具体可参考上文 pixel_clock 计算方式。

问题：屏幕闪蓝屏，并出现日志："can't fetch data from external memory fast enough, underrun happens"
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

该问题出现的原因是 PSRAM 带宽不足，可以考虑降低 lane_bit_rate_mbps 和 pixel_clock 数值，如果在 RGB888 颜色格式场景下，可以考虑更改为 RGB565 格式。同时考虑配置以下选项以改善 PSRAM 带宽：

    - ``CONFIG_SPIRAM_XIP_FROM_PSRAM=y``
    - ``CONFIG_CACHE_L2_CACHE_256KB=y``
    - ``CONFIG_CACHE_L2_CACHE_LINE_128B=y``
    - ``COMPILER_OPTIMIZATION_PERF=y``

问题：屏幕画面撕裂如何解决
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

参考 `LCD 屏幕撕裂详解 <https://github.com/espressif/esp-iot-solution/blob/master/docs/zh_CN/display/lcd/lcd_screen_tearing.rst>`_，相关例程可参考 `mipi_dsi_avoid_tearing <https://github.com/espressif/esp-iot-solution/tree/master/examples/display/lcd/mipi_dsi_avoid_tearing>`_。

问题：帧率优化配置
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

可考虑配置以下选项以优化帧率：

    - ``CONFIG_SPIRAM_XIP_FROM_PSRAM=y``
    - ``CONFIG_CACHE_L2_CACHE_256KB=y``
    - ``CONFIG_CACHE_L2_CACHE_LINE_128B=y``
    - ``COMPILER_OPTIMIZATION_PERF=y``
    - LVGL 相关配置：
        - ``CONFIG_LV_MEMCPY_MEMSET_STD=y``
        - ``CONFIG_LV_MEM_CUSTOM=y``

问题：如何驱动默认 4 lane 的屏幕
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

大多数 LCD IC 都支持通过寄存器调整 lane 数，但是不同 IC 的寄存器地址与操作并不相同，需要根据具体 IC 的 datasheet 或者直接咨询屏厂确认能否或者是否可调整 lane 数。例如 EK79007 中，可通过配置 0xB2 寄存器选择使用 2 lane/4 lane。

    .. figure:: https://dl.espressif.com/AE/esp-dev-kits/mipi_dsi_lane_num.png
        :align: center
        :alt: 通道数配置

        通道数配置

问题：如何确认屏幕所支持的 lane bit rate 范围
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

在高速串行通信里，UI 通常指信号比特（bit）在物理通道上的时间单位，也就是一个 bit 时间（bit period）。例如如果帧率或链路速率是 1 Gbps，那么一个 UI ≈ 1 纳秒（ns）。如果速率是 500 Mbps，那么 UI ≈ 2 ns，以此类推。因此可根据 LCD IC datasheet 中 AC ELECTRICAL CHARACTERISTIC - High speed transmission 来推算 lane bit rate 范围，如图所示，其 lane bit rate (单个 Data Lane 的传输速率) 范围为 20Mbps 至 500Mbps。

    .. figure:: https://dl.espressif.com/AE/esp-dev-kits/mipi_dsi_speed.png
        :align: center
        :alt: 速率范围

        速率范围


相关文档以及示例
---------------------------

- `MIPI DSI 规范 <https://www.mipi.org/specifications/dsi>`_
- `ESP-IDF MIPI DSI LCD 编程指南 <https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32p4/api-reference/peripherals/lcd/dsi_lcd.html>`_
- `ESP LCD 驱动库 <https://github.com/espressif/esp-idf/tree/master/components/esp_lcd>`_
- `ESP LCD 示例代码 <https://github.com/espressif/esp-idf/tree/master/examples/peripherals/lcd>`_
- `ESP LCD FAQ <https://docs.espressif.com/projects/esp-faq/zh_CN/latest/software-framework/peripherals/lcd.html>`_ 