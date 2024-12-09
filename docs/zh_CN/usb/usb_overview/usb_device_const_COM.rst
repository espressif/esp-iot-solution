
阻止 Windows 依据 USB 设备序列号递增 COM 编号
---------------------------------------------

:link_to_translation:`en:[English]`

由于任何连接到 Windows PC 的设备都通过其 VID、PID 和 Serial 号进行识别。如果这 3 个参数中的任何一个发生了变化，那么 PC 将检测到新的硬件，并与该设备关联一个不同的 COM 端口，详情请参考 `Windows USB device registry entries <https://learn.microsoft.com/en-us/windows-hardware/drivers/usbcon/usb-device-specific-registry-settings>`_\ 。

ESP ROM Code 中对 USB 描述符配置如下：

.. list-table::
   :header-rows: 1

   * -
     - ESP32S2
     - ESP32S3
     - ESP32C3
     - ESP32C5
     - ESP32C6
     - ESP32C61
     - ESP32H2
     - ESP32P4
   * - **Interface**
     - usb-otg
     - usb-serial-jtag
     - usb-serial-jtag
     - usb-serial-jtag
     - usb-serial-jtag
     - usb-serial-jtag
     - usb-serial-jtag
     - usb-serial-jtag
   * - **VID**
     - 0x303a
     - 0x303a
     - 0x303a
     - 0x303a
     - 0x303a
     - 0x303a
     - 0x303a
     - 0x303a
   * - **PID**
     - 0x0002
     - 0x1001
     - 0x1001
     - 0x1001
     - 0x1001
     - 0x1001
     - 0x1001
     - 0x1001
   * - **Revision (bcdDevice)**
     - N/A
     - v1.01
     - v1.01
     - v1.02
     - v1.02
     - v1.02
     - v1.02
     - v1.02
   * - **Serial**
     - 0
     - MAC 地址字符串
     - MAC 地址字符串
     - MAC 地址字符串
     - MAC 地址字符串
     - MAC 地址字符串
     - MAC 地址字符串
     - MAC 地址字符串

* ESP32S2（usb-otg）Serial 为常量 0，每个设备相同，COM 号一致
* ESP32S3（usb-serial-jtag）、或其他支持 usb-serial-jtag 接口的芯片，Serial 为设备 MAC 地址，每个设备均不同，COM 号默认递增

递增 COM 编号，为量产烧录带来额外工作。对于需要使用 USB 进行固件下载的客户，建议修改 Windows 的递增 COM 编号的规则，阻止依据 Serial 号递增编号。

解决方案
^^^^^^^^^

**管理员方式打开** ``Windows CMD`` , 执行以下指令。该指令将添加注册表项，阻止依据 Serial 号递增编号，\ **设置完成后请重启电脑使能修改**\ ：

.. code-block:: shell

    REG ADD HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\usbflags\303A10010101 /V IgnoreHWSerNum /t REG_BINARY /d 01
    REG ADD HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\usbflags\303A10010102 /V IgnoreHWSerNum /t REG_BINARY /d 01

用户也可下载 :download:`ignore_hwserial_usb_serial_jtag.bat <../../../_static/ignore_hwserial_usb_serial_jtag.bat>` 脚本，右键选择管理员方式运行。

.. Note::

    以上指令仅适用于 Windows 10/11，对于 Windows 7 或其它版本，请下载使用 :download:`ignore_hwserial_usb_serial_jtag_win7.bat <../../../_static/ignore_hwserial_usb_serial_jtag_win7.bat>` 脚本，右键选择管理员方式运行。
