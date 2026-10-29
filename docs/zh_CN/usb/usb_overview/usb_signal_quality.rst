USB 信号质量测试
==================

:link_to_translation:`en:[English]`

USB 2.0 接口的信号质量测试是确保 USB 2.0 设备符合标准、获得 USB 2.0 认证标志的重要环节。信号质量测试内容主要包括：眼图测试、信号速率、结束域（EOP）位宽、交叉点电压范围、JK 对抖动、KJ 对抖动、连续抖动、上升时间和下降时间等。其中，眼图测试是串行数据应用中最基础且关键的测试项目之一。

测试说明
-----------

眼图模板
~~~~~~~~~~~

USB 2.0 High-Speed 信号的眼图测试结果需满足 USB-IF 协会制定的标准眼图模板要求。

.. figure:: ../../../_static/usb/usb20_signal_quality_measurement_plans.png
    :align: center
    :width: 70%

    USB 2.0 高速信号测量平面

USB 2.0 High-Speed 信号的眼图测试结果需符合 USB 2.0 规范的眼图模板：

.. figure:: ../../../_static/usb/usb20_signal_quality_tp2_tp3_no_captive_cable.png
    :align: center
    :width: 90%

    在 TP2 处测量的集线器和在 TP3 处测量的设备（无固定电缆）的传输波形要求

.. figure:: ../../../_static/usb/usb20_signal_quality_tp2_captive_cable.png
    :align: center
    :width: 90%

    在 TP2 处测量的设备（带有固定电缆）的传输波形要求

.. figure:: ../../../_static/usb/usb20_signal_quality_tp1_tp4.png
    :align: center
    :width: 90%

    在 TP1 处测量的集线器收发器和在 TP4 处测量的设备收发器的传输波形要求

必备材料
~~~~~~~~~~~

- 与待测 USB 模式和速率匹配的 USB 测试治具，可由仪器厂商提供或从 USB-IF 推荐的测试机构购买
- 示波器（> 2 GHz）、一致性测试软件、探头等

示波器眼图测试
~~~~~~~~~~~~~~~~~

USB 2.0 一致性分析软件通过对接口所发出的标准信号进行分析并生成眼图。各示波器厂商的一致性测试软件操作可能有所不同，请参考示波器厂商的操作说明进行眼图测试。

.. figure:: ../../../_static/usb/usb20_signal_quality_eye.png
    :align: center
    :width: 90%

    示波器眼图测试

在 USB 2.0 眼图测试中，眼图张开得越大，信号质量越好。测试结果应完全落在眼图模板的合格区域内，任何触及或压到模板边界的情况都表示信号质量不达标。

USB 2.0 High-Speed
^^^^^^^^^^^^^^^^^^^^

.. figure:: ../../../_static/usb/usb20_signal_quality_normal_eye.png
    :align: center
    :width: 40%

    可通过测试的眼图

对于未通过测试的眼图，可从以下几个方面进行分析：

- 检查探头是否校准
- 使用规范的测试治具
- 替换线缆/连接器，排除劣质配件
- 检查 PCB 设计是否考虑 USB 2.0 规范，若在 USB High-Speed D+/D- 上使用了带有较大寄生电容的 ESD，眼图可能会测试失败，建议选择寄生电容低于 1 pF 的 ESD 二极管

.. figure:: ../../../_static/usb/usb20_signal_quality_fail_eye.png
    :align: center
    :width: 40%

    未通过测试的眼图

USB 2.0 Full-Speed
^^^^^^^^^^^^^^^^^^^^

对于 Full-Speed 接口的眼图测试，请参考示波器厂商提供的 USB 一致性测试软件进行测试：

.. figure:: ../../../_static/usb/usb20_signal_quality_fs_fail_eye.png
    :align: center
    :width: 40%

    未通过测试的 USB 2.0 Full-Speed 眼图（过冲）

对于上述 Full-Speed 未通过测试的眼图，需在 D+/D- 串接电阻以解决过冲。实测结果表明，串接 22 Ω、33 Ω 或 44 Ω 电阻均能改善过冲，其中以 33 Ω 的效果最佳。

Device 模式测试
-----------------

必备材料
~~~~~~~~~~~

- Windows PC
- USB XHSETT： `USB-IF 提供的测试工具 <https://www.usb.org/compliancetools>`_，用于控制 USB 2.0 Device 进入合规测试模式

测试固件
~~~~~~~~~~~

请根据芯片型号和芯片版本下载对应的 Device 模式测试固件。

.. list-table:: Device 模式测试固件
   :header-rows: 1
   :widths: 20 20 20 40
   :align: center

   * - 芯片型号
     - 芯片版本
     - USB 速率
     - 测试固件
   * - ESP32-S2
     - 全部版本
     - Full-Speed
     - `下载 <https://dl.espressif.com/AE/esp-iot-solution/usb_eye_diagram/esp32s2-usbd-signal-quality.bin>`__
   * - ESP32-S3
     - 全部版本
     - Full-Speed
     - `下载 <https://dl.espressif.com/AE/esp-iot-solution/usb_eye_diagram/esp32s3-usbd-signal-quality.bin>`__
   * - ESP32-P4
     - < v3.0
     - High-Speed
     - `下载 <https://dl.espressif.com/AE/esp-iot-solution/usb_eye_diagram/esp32p4-usbd-signal-quality-lt-v3.0.bin>`__
   * - ESP32-P4
     - v3.0
     - High-Speed
     - `下载 <https://dl.espressif.com/AE/esp-iot-solution/usb_eye_diagram/esp32p4-usbd-signal-quality-v3.0.bin>`__
   * - ESP32-P4
     - >= v3.1
     - High-Speed
     - `下载 <https://dl.espressif.com/AE/esp-iot-solution/usb_eye_diagram/esp32p4-usbd-signal-quality-gte-v3.1.bin>`__
   * - ESP32-S31
     - 全部版本
     - High-Speed
     - `下载 <https://dl.espressif.com/AE/esp-iot-solution/usb_eye_diagram/esp32s31-usbd-signal-quality.bin>`__

.. note::

   对于 USB Serial/JTAG Port 的 USB 眼图测试，只需使芯片进入下载模式，并确保设备能够正常枚举即可。

硬件连接
~~~~~~~~~~~

连接 PC、示波器与 USB 测试治具。High-Speed 与 Full/Low-Speed 测试治具不同，请参考示波器一致性测试软件上的连接示意图。

.. figure:: ../../../_static/usb/usb20_signal_quality_test_diagram.png
    :align: center
    :width: 70%

    USB 2.0 Device 信号质量测试系统

.. figure:: ../../../_static/usb/usb20_signal_quality_connection.png
    :align: center
    :width: 60%

    High-Speed Device 信号质量测试连接示意图

XHSETT 安装与设置
~~~~~~~~~~~~~~~~~~~~

查询 PC 设备管理器中的 USB 端口信息：

- 如果 PC 为 USB 3.x 控制器，请使用 XHSETT 版本。xHCI 支持 USB 1.x/2.0/3.x
- 如果 PC 为 USB 2.0 控制器，请使用 HSETT 版本。HSETT 支持 USB 1.x/2.0

.. figure:: ../../../_static/usb/windows_universal_serial_bus_controller.png
    :align: center
    :width: 60%

    USB 3.x 控制器

.. note:: HSETT/XHSETT 软件安装并启动后，会占用当前 PC 上的所有 USB 端口，导致 USB 端口暂时无法正常使用。因此，在测试主机上运行 HSETT/XHSETT 软件并重启 PC 之前，请务必提前连接好 PS/2 键盘和鼠标，确保操作不中断。如果无法使用 PS/2 鼠标，也可以考虑通过远程桌面等远程控制工具进行操作。

在完成 HSETT/XHSETT 软件安装后，请按照如下步骤进行设置：

打开 XHSETT 软件，选择 Device 模式：

.. figure:: ../../../_static/usb/usb20_signal_quality_xhsett_select.png
    :align: center
    :width: 60%

    XHSETT 模式选择

点击 Enumerate Bus 按钮进行搜索，搜索到设备后选中待测设备并选择 Device Command：

- 对于 High-Speed 设备，选择 Device Command 中的 TEST_PACKET
- 对于 Full-Speed 设备，选择 Device Command 中的 (LOOP) DEVICE DESCRIPTOR

.. figure:: ../../../_static/usb/usb20_signal_quality_xhsett_device_test.png
    :align: center
    :width: 60%

    XHSETT 设备选择

完成上述设置后，按照 `示波器眼图测试`_ 中的说明执行测试并分析结果。

Host 模式测试
---------------

测试固件
~~~~~~~~~~~

Host 模式测试固件会在 USB Host 完成 High-Speed 设备枚举后，控制 USB Host 端口持续发送 USB 2.0 ``TEST_PACKET`` 标准测试包。请根据芯片型号和芯片版本下载对应的 Host 模式测试固件。

.. list-table:: Host 模式 High-Speed 测试固件
   :header-rows: 1
   :widths: 20 20 20 40
   :align: center

   * - 芯片型号
     - 芯片版本
     - USB 速率
     - 测试固件
   * - ESP32-P4
     - < v3.0
     - High-Speed
     - `下载 <https://dl.espressif.com/AE/esp-iot-solution/usb_eye_diagram/esp32p4-usb-signal-quality-lt-v3.0.bin>`__
   * - ESP32-P4
     - v3.0
     - High-Speed
     - `下载 <https://dl.espressif.com/AE/esp-iot-solution/usb_eye_diagram/esp32p4-usb-signal-quality-v3.0.bin>`__
   * - ESP32-P4
     - >= v3.1
     - High-Speed
     - `下载 <https://dl.espressif.com/AE/esp-iot-solution/usb_eye_diagram/esp32p4-usb-signal-quality-gte-v3.1.bin>`__
   * - ESP32-S31
     - 全部版本
     - High-Speed
     - `下载 <https://dl.espressif.com/AE/esp-iot-solution/usb_eye_diagram/esp32s31-usb-signal-quality.bin>`__

High-Speed 测试
~~~~~~~~~~~~~~~~~~

必备材料
^^^^^^^^^^^

- 一个支持 USB 2.0 High-Speed 的 U 盘，仅用于使 USB Host 完成 High-Speed 枚举并进入 ``TEST_PACKET`` 模式
- USB 2.0 High-Speed Host 测试治具

测试步骤
^^^^^^^^^^^

1. 将对应芯片的 Host 模式测试固件烧录至待测板，并通过串口查看运行日志。
#. 将 High-Speed U 盘连接至待测 USB Host 端口。固件识别到 U 盘后会完成设备枚举，并自动控制 USB Host 端口进入 ``TEST_PACKET`` 模式。
#. 确认日志中设备速率为 ``High speed``，且出现以下信息：

   .. code-block:: text

      TEST_PACKET operation successful
      Unplug the U-disk and connect the USB test fixture / oscilloscope
      The host continues sending USB 2.0 test packets

   如果日志显示设备不是 High-Speed，请更换 U 盘、USB 线缆或检查硬件连接，然后复位待测板重新测试。

#. 保持待测板供电且不要复位，拔出 U 盘，将 USB 2.0 High-Speed Host 测试治具连接至同一个待测 USB Host 端口。进入 ``TEST_PACKET`` 模式后，固件会持续发送标准测试包。
#. 将测试治具连接至示波器，按照示波器厂商的一致性测试软件说明选择 USB 2.0 High-Speed Host 测试项目，并按照 `示波器眼图测试`_ 中的说明完成眼图测试和结果分析。

.. note::

   Host 测试固件直接控制 USB Host 端口进入 ``TEST_PACKET`` 模式，无需使用 HSETT/XHSETT。如需退出测试模式或重新执行测试，请复位或重新上电待测板，并从连接 High-Speed U 盘开始重复上述步骤。
