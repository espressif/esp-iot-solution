ESP TinyUF2
============

:link_to_translation:`en:[English]`

``esp_tinyuf2`` 是一个扩展功能的 `TinyUF2 <https://github.com/adafruit/tinyuf2>`__，针对支持 USB-OTG 功能 ESP 芯片开发。支持以下功能:

- 支持通过虚拟 USB 驱动器（U 盘）进行 OTA 升级
- 支持将 NVS 键值输出到虚拟 USB 驱动器中的 ini 文件
- 支持通过修改 ini 文件修改 NVS

UF2 是 Microsoft 开发的文件格式 `PXT <https://github.com/Microsoft/pxt>`__, 特别适合通过 MSC（存储类）烧录微控制器。 有关更友好的解释，请查看 `博客 <https://makecode.com/blog/one-chip-to-flash-them-all>`__.

在您的项目中支持 UF2 OTA/NVS
-----------------------------------

1. 使用 ``idf.py add_dependency`` 命令将组件添加到项目中.

   .. code:: sh

      idf.py add-dependency "esp_tinyuf2"

2. 定义您的分区表。 像其他 OTA 解决方案一样，您需要至少保留两个 OTA 应用程序分区. 请参考 `Partition Tables <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/partition-tables.html>`__ 和示例 `usb_uf2_ota <https://github.com/espressif/esp-iot-solution/tree/master/examples/usb/device/usb_uf2_ota>`__.

   ::

      # Partition Table Example
      # Name,   Type, SubType, Offset,  Size, Flags
      nvs,      data, nvs,     ,        0x4000,
      otadata,  data, ota,     ,        0x2000,
      phy_init, data, phy,     ,        0x1000,
      ota_0,    app,  ota_0,   ,        1500K,
      ota_1,    app,  ota_1,   ,        1500K,

3. 使用 ``idf.py menuconfig`` 配置组件 ``(Top) → Component config → TinyUF2 Config``

-  ``USB Virtual Disk size(MB)``: 虚拟 USB 驱动器在文件资源管理器中显示的大小，默认情况下为 8MB
-  ``Max APP size(MB)``: 最大应用大小，默认情况下 4MB
-  ``Flash cache size(KB)``: 缓存大小，用于缓存即将烧录的 bin 片段，默认情况下为 32KB
-  ``USB Device VID``: Espressif VID (默认 0x303A) 
-  ``USB Device PID``: Espressif test PID (默认 0x8000), 如需申请 PID 请参考 `esp-usb-pid <https://github.com/espressif/usb-pids>`__.
-  ``USB Disk Name``: 虚拟 USB 驱动器在文件资源管理器中显示的名称, ``ESP32Sx-UF2`` 默认
-  ``USB Device Manufacture``: 制造商 ``Espressif`` 默认
-  ``Product Name``: 产品名称 ``ESP TinyUF2`` 默认
-  ``Product ID``: 产品 ID ``12345678`` 默认
-  ``Product URL``: ``index`` 文件将被添加到 U 盘, 用户可以单击进入网页, ``https://products.espressif.com/`` 默认
-  ``UF2 NVS ini file size``: ``ini`` 文件大小，用于保存 NVS 键值对

4. 如下所示，安装 ``tinyuf2`` 功能，有关更多详细信息，请参阅示例 `usb_uf2_nvs <https://github.com/espressif/esp-iot-solution/tree/master/examples/usb/device/usb_uf2_nvs>`__ 和 `usb_uf2_ota <https://github.com/espressif/esp-iot-solution/tree/master/examples/usb/device/usb_uf2_ota>`__

   .. code:: c

      /* install UF2 OTA */
      tinyuf2_ota_config_t ota_config = DEFAULT_TINYUF2_OTA_CONFIG();
      ota_config.complete_cb = uf2_update_complete_cb;
      /* disable auto restart, if false manual restart later */
      ota_config.if_restart = false;
      /* install UF2 NVS */
      tinyuf2_nvs_config_t nvs_config = DEFAULT_TINYUF2_NVS_CONFIG();
      nvs_config.part_name = "nvs";
      nvs_config.namespace_name = "myuf2";
      nvs_config.modified_cb = uf2_nvs_modified_cb;
      esp_tinyuf2_install(&ota_config, &nvs_config);

5. 运行 ``idf.py build flash`` 进行初始下载, 之后可以使用 ``idf.py uf2-ota`` 生成新的 ``uf2`` app bin

6. 将 UF2 格式 app bin 拖放到磁盘, 升级将自动进行

.. figure:: ../../../_static/usb/uf2_disk.png
   :alt: UF2 Disk

使用 UF2 USB Console
-----------------------

通过 menuconfig ``(Top) → Component config → TinyUF2 Config → Enable USB Console For log``, 该日志将输出到 USB 串行端口（默认向 UART 输出）.

编译 APP 为 UF2 格式
-----------------------

该组件添加了新的命令 ``idf.py uf2-ota``, 可以用于将 app 编译为 UF2 格式. 编译完成后, 新的 UF2 文件 (``${PROJECT_NAME}.uf2``) 将被产生到 ``project`` 文件夹.

.. code:: sh

   idf.py uf2-ota

将现有 bin 转换为 UF2 格式
----------------------------------

将您现有的应用 bin 转换为UF2格式, 可以使用 `uf2conv.py <https://github.com/espressif/esp-iot-solution/blob/master/components/usb/esp_tinyuf2/utils/uf2conv.py>`__, 您需要指定 family id 为 ``ESP32S2``, ``ESP32S3`` 或如下的 magic number. 并且还需使用 ``-b`` 指定地址, tinyuf2 将使用它作为偏移来写入 OTA 分区.

1. 转换指令

   using:

   .. code:: sh

      uf2conv.py your_firmware.bin -c -b 0x00 -f ESP32S3

   or:

   .. code:: sh

      uf2conv.py your_firmware.bin -c -b 0x00 -f 0xc47e5767

注意
-----

-  要连续使用 UF2 OTA 功能，必须在更新的应用中同样启用 tinyuf2.

API 参考
-------------

.. include-build-file:: inc/esp_tinyuf2.inc
