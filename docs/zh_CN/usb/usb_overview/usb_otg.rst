
USB-OTG 外设介绍
------------------

:link_to_translation:`en:[English]`

ESP32-S2/S3/P4 等芯片内置 USB-OTG 外设，它包含了 USB 控制器和 USB PHY，支持通过 USB 线连接到 PC，实现 USB Host 和 USB Device 功能。

USB-OTG 传输速率
^^^^^^^^^^^^^^^^^^^

ESP32-S2/S3/P4 USB-OTG Full Speed 总线传输速率为 12 Mbps，但由于 USB 传输存在一些校验和同步机制，实际的有效传输速率将低于 12 Mbps。具体数值和传输类型相关，如下表所示：

.. list-table::
   :header-rows: 1

   * - **传输类型**
     - **控制**
     - **中断**
     - **批量**
     - **同步**
   * - 适用场合
     - 设备初始化和管理
     - 鼠标和键盘
     - 打印机和批量存储
     - 流式音频和视频
   * - 支持低速
     - 有
     - 有
     - 无
     - 无
   * - 校验重传
     - 有
     - 有
     - 有
     - 无
   * - 保证传输速度
     - 无
     - 无
     - 无
     - 有
   * - 使用固定带宽
     - 有（10%）
     - 有（90%）
     - 无
     - 有（90%）
   * - 减少延迟时间
     - 无
     - 有
     - 无
     - 有
   * - 传输的最大尺寸
     - 64字节
     - 64字节
     - 64字节
     - ~512字节
   * - 每毫秒传输包数量
     - *
     - 1
     - 19
     - 1
   * - 理论有效速率
     - *
     - 64000 Bytes/s
     - 1216000 Bytes/s
     - 512000 Bytes/s


..

对于 ESP32-P4 USB-OTG High Speed 总线传输速率为 480 Mbps，与 Full Speed 相比，在传输的最大尺寸、每毫秒传输包数量与理论有效速率上有所不同，如下表所示：

.. list-table::
   :header-rows: 1

   * - **传输类型**
     - **控制**
     - **中断**
     - **批量**
     - **同步**
   * - 传输的最大尺寸
     - 64字节
     - 1024字节
     - 512字节
     - 1024字节
   * - 每微帧传输包数量
     - *
     - 6
     - 13
     - 7
   * - 理论有效速率
     - *
     - 49152000 Bytes/s
     - 53248000 Bytes/s
     - 57344000 Bytes/s
..


   * 传输速率的计算公式为：传输速率 (Bytes/s) = 传输的最大尺寸 * 每微帧传输包数量 * 8000
   * 控制传输用于传输设备控制信息，包含多个阶段，有效传输速率需要按照协议栈的实现来计算。


USB-OTG 外设内置功能
^^^^^^^^^^^^^^^^^^^^^^^^^

使用 USB OTG Console 下载固件和打印 LOG
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

ESP32-S2/S3 等内置 USB-OTG 外设的芯片，ROM Code 中内置了 USB 通信设备类 (CDC) 的功能，该功能可用于替代 UART 接口，实现 Log、Console 和固件下载功能。


#. 由于 USB OTG Console 默认为关闭状态，如需使用它下载固件，需要通过以下方法完成初次下载：

   #. 在 ``menuconfig`` 中先使能 USB OTG Console 功能，然后编译固件
   #. 手动芯片的 Boot 控制引脚拉低，然后将芯片通过 USB 线连接到 PC，进入下载模式。PC 会发现新的串口设备，Windows 为 ``COM*``\ ，Linux 为 ``/dev/ttyACM*``\ , MacOS 为 ``/dev/cu*``\ 。
   #. 使用 esptool 工具（或直接使用 idf.py flash）配置设备对应的串口号下载固件。

#. 初次下载完成以后，USB OTG Console 功能将自动使能，即可通过 USB 线连接到 PC，PC 会发现新的串口设备，Windows 为 ``COM*``\ ，Linux 为 ``/dev/ttyACM*``\ , MacOS 为 ``/dev/cu*``\ ，LOG 数据将从该虚拟串口打印。

#. 用户无需再手动拉低 Boot 控制引脚，使用 esptool 工具（或直接使用 idf.py flash）配置设备对应的串口号即可下载固件，下载期间，esptool 通过 USB 控制协议自动将设备 Reset 并切换到下载模式。

..

   更多详细信息，请参考：\ `USB OTG Console <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/api-guides/usb-otg-console.html>`_


使用 USB OTG DFU 下载固件
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

ESP32-S2/S3/P4 等内置 USB-OTG 外设的芯片，ROM Code 中内置了 USB DFU（Device Firmware Upgrade）功能，可用于实现标准的 DFU 下载模式。


#. 使用 DFU 下载固件，用户每次都需要手动进入下载模式，将芯片的 Boot 控制引脚拉低，然后通过 USB 线连接到 PC。
#. 在工程目录下运行指令 ``idf.py dfu`` 生成 DFU 固件，然后使用 ``idf.py dfu-flash`` 下载固件。
#. 如果存在多个 DFU 设备，用户可以使用 ``idf.py dfu-list`` 查看 DFU 设备列表，然后使用 ``idf.py dfu-flash --path <path>`` 指定下载端口。

..

   更多详细信息，请参考：\ `Device Firmware Upgrade via USB <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/api-guides/dfu.html>`_


使用 USB-OTG 外设进行 USB Host 开发
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

USB-OTG 外设支持 USB Host 功能，用户可以通过 USB 接口直接连接到外部 USB 设备。ESP-IDF 从 v4.4 版本开始，已经支持 USB Host Driver，用户可以参考 `ESP-IDF USB Host <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/usb_host.html>`_\ ，开发 USB Class Driver。

此外乐鑫也已经官方支持 USB Host HID，USB Host MSC，USB Host CDC，USB Host UVC 等设备类驱动，用户可以直接使用这些驱动进行应用开发。

USB Host 方案详情，请参考 :doc:`USB Host Solution <./usb_host_solutions>` 。

使用 USB-OTG 外设进行 USB Device 开发
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

USB-OTG 外设支持 USB Device 功能，乐鑫已经官方适配了 TinyUSB 协议栈，用户可以直接使用基于 TinyUSB 开源协议栈开发的 USB 标准设备或自定义设备，例如 HID，MSC，CDC，ECM, UAC 等。

USB Device 方案详情，请参考 :doc:`USB Device Solution <./usb_device_solutions>` 。

动态切换 USB-OTG 外设模式
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

USB-OTG 外设支持动态切换 USB-OTG 外设模式，用户可以通过动态的注册 USB Host Driver 或 USB Device Driver，实现 USB-OTG 外设的动态切换。

* 示例代码: USB OTG 手动切换 :example:`usb/otg/usb_host_device_mode_manual_switch`

使用 USB-OTG 主动断开与主机的连接
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

可以通过将 USB OTG VBUSVALID 信号拉低，主动断开与主机的连接，从而实现 USB OTG 外设的断开。注意如果需要重新挂载设备，需要至少等待 10ms。请参考 :doc:`USB Device Self Power <./usb_device_self_power>`
