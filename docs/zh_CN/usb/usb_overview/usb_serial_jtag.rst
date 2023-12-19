USB-Serial-JTAG 外设介绍
------------------------

:link_to_translation:`en:[English]`

ESP32-S3/C3 等芯片内置 USB-Serial-JTAG 外设，它包含了 USB-to-serial 转换器和 USB-to-JTAG 转换器，支持通过 USB 线连接到 PC，实现固件下载、调试和打印系统 LOG 等功能。USB-Serial-JTAG 外设的内部结构可参考 `ESP32-C3 技术参考手册-USB Serial/JTAG Controller <https://www.espressif.com/sites/default/files/documentation/esp32-c3_technical_reference_manual_en.pdf>`_\ 。

USB-Serial-JTAG 外设驱动
^^^^^^^^^^^^^^^^^^^^^^^^^^^^


* Linux 和 MacOS 系统下，无需手动安装驱动
* Windows 10 及以上系统，联网将自动安装驱动
* Windows 7/8 系统，需要手动安装驱动，驱动下载地址：\ `esp32-usb-jtag-2021-07-15 <https://dl.espressif.com/dl/idf-driver/idf-driver-esp32-usb-jtag-2021-07-15.zip>`_\ 。用户也可以使用 `ESP-IDF Windows Installer <https://dl.espressif.com/dl/esp-idf/>`_\ ，勾选 USB-Serial-JTAG 驱动进行安装，

USB-Serial-JTAG 外设内置功能
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

USB-Serial-JTAG 外设接入 PC 后，设备管理器将新增两个设备：

Windows 如下图所示：


.. image:: ../../../_static/usb/device_manager_usb_serial_jtag_cn.png
   :target: ../../../_static/usb/device_manager_usb_serial_jtag_cn.png
   :alt: device_manager_usb_serial_jtag_cn


Linux 如下图所示：


.. image:: ../../../_static/usb/usb_serial_jtag_linux.png
   :target: ../../../_static/usb/usb_serial_jtag_linux.png
   :alt: device_manager_usb_serial_jtag_cn


使用 USB-Serial-JTAG 下载固件
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^


* 默认情况下，USB-Serial-JTAG 下载功能处于使能状态，可以直接使用 USB 线连接到 PC，然后使用 esptool 工具（或直接使用 idf.py flash）配置 USB-Serial-JTAG 设备对应的串口号（Windows 为 ``COM*``\ ，Linux 为 ``/dev/ttyACM*``\ , MacOS 为 ``/dev/cu*``\ ）下载固件。下载期间，esptool 通过 USB 控制协议自动将设备 Reset 并切换到下载模式。
* 如果在应用程序中将 USB-Serial-JTAG 对应的 USB 引脚用作了其它功能，例如用作普通 GPIO 或其它外设 IO，USB-Serial-JTAG 将无法与 USB 主机建立连接，因此无法通过 USB 将设备切换到下载模式，用户必须通过 Boot 控制引脚，将设备手动切换到下载模式，然后再使用 esptool 下载固件。
* 为了避免在应用程序中将 USB-Serial-JTAG 对应的 USB 引脚用作其它功能，导致无法通过 USB 自动进入下载模式，用户需要在硬件设计时，引出 Boot 控制引脚。
* 默认情况下，通过 USB 接口下载不同的芯片，COM 号将递增，可能对量产造成不便，用户可参考 :doc:`阻止 Windows 依据 USB 设备序列号递增 COM 编号 <./usb_device_const_COM>` 。

使用 USB-Serial-JTAG 调试代码
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

``USB-Serial-JTAG`` 支持通过 ``JTAG`` 接口调试代码，用户仅需使用 USB 线连接到 PC，然后使用 ``OpenOCD`` 工具即可调试代码。配置方法请参考 `配置 ESP32-C3 内置 JTAG 接口 <https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-guides/jtag-debugging/configure-builtin-jtag.html>`_\ 。

使用 USB-Serial-JTAG 打印系统 LOG
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^


* 用户可通过 ``menuconfig-> Component config → ESP System Settings → Channel for console secondary output`` 配置 USB-Serial-JTAG LOG 功能的使能状态。
* LOG 功能使能以后，可以直接使用 USB 线连接到 PC，然后使用 ``idf.py monitor`` 或其它串口工具打开 USB-Serial-JTAG 设备对应的串口号（Windows 为 ``COM*``\ ，Linux 为 ``/dev/ttyACM*``\ , MacOS 为 ``/dev/cu*``\ ），即可打印系统 LOG。
* ``USB-Serial-JTAG`` 仅在主机接入后才会打印 LOG，如果主机未接入，\ ``USB-Serial-JTAG`` 不会被初始化，也不会打印 LOG。
* ``USB-Serial-JTAG`` LOG 功能无法在睡眠模式下使用（包括 deep sleep 和 light sleep 模式），如果需要在睡眠模式下打印 LOG，可以使用 ``UART`` 接口。

使用 USB-Serial-JTAG 引脚作为普通 GPIO
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

如果用户需要在应用程序中将 USB-Serial-JTAG 对应的 USB 引脚用作其它功能，例如用作普通 GPIO。需要注意 USB D+ 接口默认上拉 USB 电阻，该电阻会导致 USB D+ 引脚常为高电平，因此需要在用作 GPIO 时将其禁用。


* ESP-IDF v4.4 及以后版本 GPIO 驱动默认会禁用 USB D+ 上拉电阻，用户使用 GPIO 驱动时无需额外配置。
* 用户也可以修改寄存器值 ``USB_SERIAL_JTAG.conf0.dp_pullup = 0;`` 将 USB D+ 上拉电阻禁用。

需要特别注意的是 USB D+ 引脚的上拉电阻在上电时刻即存在，用户在调用软件禁用上拉电阻之前，USB D+ 引脚已经被拉高，导致 D+ 引脚做为 GPIO 时，初始阶段为高电平，如果用户需要在上电后 USB D+ 引脚立即为低电平，需要在硬件设计时，将 USB D+ 引脚通过外部电路拉低。
