文件系统
============
:link_to_translation:`en:[English]`

已支持的文件系统列表：

+----------+----------------------+------------------------+----------------------------+
| 关键特性 |        NVS 库        |      FAT 文件系统      |      SPIFFS 文件系统       |
+==========+======================+========================+============================+
| 特点     | 键值对保存，接口安全 | 操作系统支持，兼容性强 | 针对嵌入式开发，资源占用低 |
+----------+----------------------+------------------------+----------------------------+
| 应用场景 | 参数保存             | 音视频、文件保存       | 音视频、文件保存           |
+----------+----------------------+------------------------+----------------------------+
| 容量     | KB-MB                | GB                     | < 128 MB                   |
+----------+----------------------+------------------------+----------------------------+
| 目录支持 | X                    | √                      | X                          |
+----------+----------------------+------------------------+----------------------------+
| 磨损均衡 | √                    | 可选                   | √                          |
+----------+----------------------+------------------------+----------------------------+
| 读写效率 | 0                    | 0                      | 0                          |
+----------+----------------------+------------------------+----------------------------+
| 资源占用 | 0                    | 0                      | \*1                        |
+----------+----------------------+------------------------+----------------------------+
| 掉电保护 | √                    | X                      | X                          |
+----------+----------------------+------------------------+----------------------------+
| 加密     | √                    | √                      | X                          |
+----------+----------------------+------------------------+----------------------------+


.. Note::

    * 0：暂无数据或不参与比较。
    * \*1：低 RAM 占用。


NVS 库
-----------

非易失性存储 (NVS) 库主要用于读写在 flash NVS 分区中存储的数据。NVS 中的数据以键值对的方式保存，其中键是 ASCII 字符串，值可以是整数、字符串、二进制数据 (BLOB) 类型。NVS 支持掉电保护和数据加密，适合存储一些较小的数据，如应用程序参数等。如需存储较大的 BLOB 或者字符串，请考虑使用基于磨损均衡库的 FAT 文件系统。

**参考文档：**

- `非易失性存储库 <https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-reference/storage/nvs_flash.html>`_。
- 批量生产时，可以使用 `NVS 分区生成工具 <https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-reference/storage/nvs_partition_gen.html>`_。

**示例程序：**

- 写入单个整数值：`storage/nvs_rw_value <https://github.com/espressif/esp-idf/tree/526f682/examples/storage/nvs_rw_value>`_。
- 写入二进制大对象：`storage/nvs_rw_blob <https://github.com/espressif/esp-idf/tree/526f682/examples/storage/nvs_rw_blob>`_。

FAT 文件系统
-------------

ESP-IDF 使用 FatFs 库实现了对 FAT 文件系统的支持，FatFs 是独立于平台和存储介质的文件系统层，通过统一接口实现对物理设备（如 flash、SD 卡）的访问。用户可以直接调用 FatFs 的接口操作，也可以借助 C 标准库和 Posix API 通过 VFS（虚拟文件系统）使用 FatFs 库的大多数功能。

FAT 文件系统操作系统兼容性强，广泛应用于 USB 存储盘或 SD 卡等移动存储设备上。ESP32 系列芯片通过支持 FAT 文件系统，可以实现对这些常见存储设备的访问。

**参考文档：**

- `FatFs 与 VFS 配合使用 <https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-reference/storage/fatfs.html#fatfs-vfs>`_。
- `FatFs 与 VFS 和 SD 卡配合使用 <https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-reference/storage/fatfs.html#fatfs-vfs-sd>`_。

**示例程序：**

* `storage/sd_card <https://github.com/espressif/esp-idf/tree/526f682/examples/storage/sd_card>`_：访问使用 FAT 文件系统的 SD 卡。
* `storage/ext_flash_fatfs <https://github.com/espressif/esp-idf/tree/master/examples/storage/ext_flash_fatfs>`_：访问使用 FAT 文件系统的外部 Flash 芯片。

SPIFFS 文件系统
----------------

SPIFFS 是一个专用于 SPI NOR flash 的嵌入式文件系统，原生支持磨损均衡、文件系统一致性检查等功能。用户可以直接调用 SPIFFS 提供的 Posix 样式接口，也可以通过 VFS 操作 SPIFFS 的大多数功能。

SPIFFS 作为嵌入式平台 SPI NOR Flash 的专用文件系统，相比 FAT 文件系统占用 RAM 资源更少，但是仅用于支持容量小于 128 MB 的 Flash 芯片。

**参考文档：**

* `SPIFFS 文件系统 <https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-reference/storage/spiffs.html>`_。
* `两种生成 SPIFFS 镜像的工具 <https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-reference/storage/spiffs.html#id6>`_。

**示例程序：**

* `storage/spiffs <https://github.com/espressif/esp-idf/tree/526f682/examples/storage/spiffs>`_：SPIFFS 使用示例。


虚拟文件系统 (VFS)
-------------------

ESP-IDF 虚拟文件系统 (VFS) 组件可以为不同文件系统 (FAT, SPIFFS) 提供统一的接口，也可以为设备驱动程序提供类似文件读写的操作接口。

**参考文档：**

* `虚拟文件系统组件 <https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-reference/storage/vfs.html>`_。
