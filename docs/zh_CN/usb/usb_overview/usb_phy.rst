
USB PHY/Transceiver 介绍
------------------------

:link_to_translation:`en:[English]`

USB PHY/Transceiver 的功能是将 USB 控制器的数字信号转换为 USB 总线信号电平，提供总线驱动能力，检测接收错误等。ESP32-S2/S3/P4 芯片已内置一个 USB Full-speed PHY，用户可直接使用芯片指定的 USB D+ D- 与外部 USB 系统通信。同时，ESP32-P4 还内置 USB High-Speed PHY。此外，ESP32-S2/S3 还保留了外部 PHY 的扩展接口，用户可在需要时连接外部 PHY。

使用内部 PHY
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

ESP32-S2/S3/C3 内部集成了 USB PHY，因此无需外接 PHY 芯片，可以直接与外部 USB 主机或设备通过 USB D+/D- 连接。但对于集成两个 USB 控制器的芯片，例如 ESP32-S3 内置 USB-OTG 和 USB-Serial-JTAG，两者共用一个内部 PHY，同一时间只能有一个工作。


.. image:: ../../../_static/usb/esp32s3_usb.png
   :target: ../../../_static/usb/esp32s3_usb.png
   :alt: esp32s3_usb


内部 USB-PHY 对应固定的 GPIO，如下表所示：

.. list-table::
   :header-rows: 1

   * -
     - D+
     - D-
   * - ESP32-S2
     - 20
     - 19
   * - ESP32-S3
     - 20
     - 19
   * - ESP32-C3
     - 19
     - 18
   * - ESP32-C6
     - 13
     - 12

对于 ESP32-P4 而言，内部集成了两个 USB Full-speed PHY 与一个 USB High-speed PHY。

.. image:: ../../../_static/usb/esp32p4_usb.png
   :target: ../../../_static/usb/esp32p4_usb.png
   :alt: esp32p4_usb

内部 USB-PHY 对应的 GPIO，如下表所示：

.. list-table::
   :header-rows: 1

   * -
     - D+
     - D-
   * - ESP32-P4 FS_PHY1
     - 25
     - 24
   * - ESP32-P4 FS_PHY2
     - 27
     - 26
   * - ESP32-P4 HS_PHY
     - pin 50
     - pin 49

默认情况下，FS_PHY1 连接到 USB 串行/JTAG 控制器，FS_PHY2 连接到 OTG_FS。用户可以通过 EFUSE_USB_PHY_SEL 更改连接关系。

.. _external_phy:

使用外部 PHY （ESP32-S2/S3）
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

通过增加一个外部 PHY，可以实现 USB-OTG 和 USB-Serial-JTAG 两个外设同时工作。

ESP32S2/S3 支持 SP5301 或同等功能的 USB PHY。外部 PHY 的典型电路图如下：


.. image:: ../../../_static/usb/usb_fs_phy_sp5301.png
   :target: ../../../_static/usb/usb_fs_phy_sp5301.png
   :alt: usb_fs_phy_sp5301


..

   使用外部 USB PHY，将占用至少 6 个 GPIO


USB PHY 默认配置
^^^^^^^^^^^^^^^^^^^^^^^^^^^^


#. 对于同时具有 USB-OTG 和 USB-Serial-JTAG 两个外设的芯片，默认情况下 USB-Serial-JTAG 与内部 USB-PHY 连接。用户可以直接通过 USB 接口进行下载或调试，无需额外配置。
#. 如需使用 USB Host Driver 或 TinyUSB 协议栈开发 USB-OTG 应用，在协议栈初始化时，USB-PHY 连接会自动切换为 USB-OTG，用户不必再进行配置。在 USB-OTG 模式下，如果用户需要使用 USB-Serial-JTAG 的下载功能，需要手动 Boot 到下载模式。

修改 USB PHY 默认配置
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

方法 1： 通过配置寄存器，将 USB-PHY 连接切换为 USB-OTG。


* USB Host Driver 或 TinyUSB 协议栈内部通过配置 USB PHY 寄存器，将内部 USB-PHY 连接切换为 USB-OTG，如需了解更多信息，请参考 `USB PHY 配置 API <https://github.com/espressif/esp-idf/blob/master/components/esp_hw_support/include/esp_private/usb_phy.h>`_\ 。

方法 2：通过烧写 efuse usb_phy_sel 位为 1，将 USB-PHY 默认连接切换为 USB-OTG：


* 仅当用户需要在 Boot 模式下使用 USB-OTG 功能时，才需要烧写此 efuse 位。烧写完成后，芯片进入 Boot 模式后，将使用 USB-OTG 提供的下载功能，例如 USB DFU。
* 注意，烧写 efuse 为 1 后不可恢复为 0。当 USB-PHY 默认连接切换为 USB-OTG，芯片进入 Boot 模式后，将使用 USB-OTG 功能，USB-Serial-JTAG 功能将无法使用。
* 注意：对于在 DateCode 2219 生产的 ESP32-S3 模组和开发板 (PW No. 早于 PW-2022-06-XXXX), 由于 EFUSE_DIS_USB_OTG_DOWNLOAD_MODE (BLK0 B19[7]) 已经被烧录为 1 （USB OTG 下载被禁用），用户如果再烧写 efuse_usb_phy_sel 位为 1，将导致芯片进入 Boot 模式后，USB-Serial-JTAG 和 USB-OTG 下载功能均无法使用。
