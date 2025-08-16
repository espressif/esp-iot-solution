USB 信号质量测试
==================

:link_to_translation:`en:[English]`

USB 2.0 接口的信号质量测试是确保 USB 2.0 设备符合标准、获得 USB 2.0 认证标志的重要环节。信号质量测试内容主要包括：眼图测试、信号速率、结束域（EOP）位宽、交叉点电压范围、JK 对抖动、KJ 对抖动、连续抖动、上升时间和下降时间等。其中，眼图测试是串行数据应用中最基础且关键的测试项目之一。

眼图测试
-----------

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

- Windows PC
- USB XHSETT： `USB-IF 提供的测试工具 <https://www.usb.org/compliancetools>`_，用于控制 USB 2.0 设备和主机以及 USB 3.1 主机和集线器进入合规测试模式
- USB 治具：由仪器厂商提供或从 USB-IF 推荐的测试机构购买
- 示波器（> 2 GHz）、一致性测试软件、探头等

.. figure:: ../../../_static/usb/usb20_signal_quality_test_diagram.png
    :align: center
    :width: 70%

    USB 2.0 信号质量测试系统

测试步骤
~~~~~~~~~~~

1. ESP32-S2/S3/P4 USB 测试固件
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

基于 `Launchpad <https://espressif.github.io/esp-launchpad/?flashConfigURL=https://dl.espressif.com/AE/esp-iot-solution/usb_eye_diagram/config.toml>`_ 平台，下载 ESP32-S2/S3/P4 USB 测试固件，其中 ESP32-P4 用于测试 High-Speed，ESP32-S2/S3 用于测试 Full-Speed。

2. 硬件连接
^^^^^^^^^^^^^^

连接 PC、示波器与 USB 测试治具：High-Speed 与 Full/Low-Speed 测试治具不同，请参考示波器一致性测试软件上的连接示意图。

.. figure:: ../../../_static/usb/usb20_signal_quality_connection.png
    :align: center
    :width: 60%

    High-Speed Device 信号质量测试连接示意图

3. XHSETT 安装与设置
^^^^^^^^^^^^^^^^^^^^^^^

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

4. 示波器眼图测试
^^^^^^^^^^^^^^^^^^^^^^^

USB 2.0 一致性分析软件通过对接口所发出的标准信号（XHSETT/HSETT）进行分析并生成眼图。各示波器厂商的一致性测试软件操作可能有所不同，请参考示波器厂商的操作说明进行眼图测试。

.. figure:: ../../../_static/usb/usb20_signal_quality_eye.png
    :align: center
    :width: 90%

    示波器眼图测试

在 USB 2.0 眼图测试中，眼图张开得越大，信号质量越好。测试结果应完全落在眼图模板的合格区域内，任何触及或压到模板边界的情况都表示信号质量不达标。

4.1 USB 2.0 High-Speed
""""""""""""""""""""""

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

4.2 USB 2.0 Full-Speed
""""""""""""""""""""""

对于 Full-Speed 设备的眼图测试，请参考示波器厂商提供的 USB 一致性测试软件进行测试：

.. figure:: ../../../_static/usb/usb20_signal_quality_fs_fail_eye.png
    :align: center
    :width: 40%
    
    未通过测试的 USB 2.0 Full-Speed 眼图（过冲）

对于上述 Full-Speed 未通过测试的眼图，需在 D+/D- 串接电阻以解决过冲。实测结果表明，串接 22 Ω、33 Ω 或 44 Ω 电阻均能改善过冲，其中以 33 Ω 的效果最佳。
