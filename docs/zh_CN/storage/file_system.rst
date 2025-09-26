文件系统
=============
:link_to_translation:`en:[English]`

已支持的文件系统列表：

.. list-table:: 文件系统特性比较
    :widths: 20 20 20 20 20
    :header-rows: 1

    * - 关键特性
      - NVS 库
      - FAT 文件系统
      - SPIFFS 文件系统
      - LittleFS 文件系统
    * - 特点
      - 键值对保存，接口安全
      - 操作系统支持，兼容性强
      - 针对嵌入式开发，资源占用低
      - 资源占用低，读、写、擦除速度快
    * - 应用场景
      - 参数保存
      - 音视频、文件保存
      - 音视频、文件保存
      - 文件保存
    * - 存储介质
      - SPI NOR Flash
      - SPI NOR Flash、SD/MMC 卡、USB 闪存盘
      - SPI NOR Flash
      - SPI NOR Flash、SD/MMC 卡
    * - 容量
      - KB-MB
      - GB
      - ≤ 128 MB
      - < 128 MB
    * - 目录支持
      - ❌
      - ✅
      - ❌
      - ✅
    * - 磨损均衡
      - ✅
      - 可选
      - ✅
      - ✅
    * - 读写效率
      - 高
      - 中
      - 中
      - 高
    * - 资源占用
      - 低
      - 中
      - 低
      - 极低
    * - 掉电保护
      - ✅
      - ❌
      - ❌
      - ✅
    * - 加密
      - ✅
      - ✅
      - ❌
      - ✅


.. Note::

    * ✅：支持该特性
    * ❌：不支持该特性  
    * 读写效率和资源占用为相对比较，实际性能取决于具体配置和使用场景。


NVS 库
----------------------

非易失性存储 (NVS) 库主要用于读写在 Flash NVS 分区中存储的数据。NVS 中的数据以键值对的方式保存，其中键是 ASCII 字符串，值可以是整数、字符串、二进制数据（BLOB）类型。

**主要特性：**

* **键值对存储**：支持整数、字符串、BLOB 等多种数据类型
* **断电保护**：原子更新，确保数据一致性
* **加密支持**：支持 AES-XTS 加密
* **磨损均衡**：内置磨损均衡机制

**使用限制：**

* **适用场景**：仅适合配置数据，不适合频繁写入或大数据
* **分区大小**：推荐不超过 128 KB
* **空间要求**：需要足够空间同时存储新旧数据

如需存储较大的 BLOB 或者字符串，请考虑使用基于磨损均衡库的 FAT 文件系统。

**参考文档：**

- `非易失性存储库 <https://docs.espressif.com/projects/esp-idf/zh_CN/release-v5.4/esp32s3/api-reference/storage/nvs_flash.html>`_。
- 批量生产时，可以使用 `NVS 分区生成工具 <https://docs.espressif.com/projects/esp-idf/zh_CN/release-v5.4/esp32s3/api-reference/storage/nvs_partition_gen.html>`_。

**示例程序：**

- 写入单个整数值：`storage/nvs_rw_value <https://github.com/espressif/esp-idf/tree/release/v5.4/examples/storage/nvs_rw_value>`_。
- 写入二进制大对象：`storage/nvs_rw_blob <https://github.com/espressif/esp-idf/tree/release/v5.4/examples/storage/nvs_rw_blob>`_。

FAT 文件系统
-------------------------

ESP-IDF 使用 FatFs 库实现了对 FAT 文件系统的支持，FatFs 是独立于平台和存储介质的文件系统层，通过统一接口实现对物理设备（如 Flash、SD 卡、USB 闪存盘）的访问。用户可以直接调用 FatFs 的接口操作，也可以借助 C 标准库和 Posix API 通过 VFS（虚拟文件系统）使用 FatFs 库的大多数功能。

**主要特性：**

* **广泛兼容性**：与 PC 和其他平台兼容，支持标准 FAT 格式
* **多存储介质**：支持 SPI Flash，SD/MMC 卡 和 USB 闪存盘等多种存储介质
* **目录支持**：支持多级目录结构，适合存储大量文件
* **加密支持**：支持分区加密 （`Flash Encryption <https://docs.espressif.com/projects/esp-idf/zh_CN/release-v5.4/esp32s3/security/flash-encryption.html>`_）

**使用限制：**

* **断电恢复**：断电恢复能力相对较弱
* **分区大小**：启用磨损均衡时最小需要 8 个扇区

**参考文档：**

- `FatFs 与 VFS 配合使用 <https://docs.espressif.com/projects/esp-idf/zh_CN/release-v5.4/esp32s3/api-reference/storage/fatfs.html#fatfs-vfs>`_。
- `生成和解析 FATFS 镜像工具 <https://docs.espressif.com/projects/esp-idf/zh_CN/release-v5.4/esp32s3/api-reference/storage/fatfsgen.html>`_。

**示例程序：**

* `storage/sd_card <https://github.com/espressif/esp-idf/tree/release/v5.4/examples/storage/sd_card>`_：访问使用 FAT 文件系统的 SD 卡。
* `storage/ext_flash_fatfs <https://github.com/espressif/esp-idf/tree/release/v5.4/examples/storage/ext_flash_fatfs>`_：访问使用 FAT 文件系统的外部 Flash 芯片。
* `peripherals/usb/host/msc <https://github.com/espressif/esp-idf/tree/release/v5.4/examples/peripherals/usb/host/msc>`_：访问使用 FAT 文件系统的 USB 闪存盘（需要 ESP32-S2/ESP32-S3）。

SPIFFS 文件系统
----------------------------

SPIFFS 是一个专用于 SPI NOR Flash 的嵌入式文件系统，原生支持磨损均衡、文件系统一致性检查等功能。用户可以直接调用 SPIFFS 提供的 Posix 样式接口，也可以通过 VFS 操作 SPIFFS 的大多数功能。

**主要特性：**

* **嵌入式优化**：专为 NOR Flash 设计，RAM 占用较少
* **静态磨损均衡**：内置磨损均衡算法
* **断电恢复**：一定程度的断电后修复功能

**使用限制：**

* **无目录支持**：只支持扁平文件结构
* **容量限制**：最大支持 128 MB Flash
* **性能下降**：使用率超过 70% 时性能明显下降
* **开发停止**：已停止维护

**参考文档：**

* `SPIFFS 文件系统 <https://docs.espressif.com/projects/esp-idf/zh_CN/release-v5.4/esp32s3/api-reference/storage/spiffs.html>`_。
* `生成 SPIFFS 镜像的工具 <https://docs.espressif.com/projects/esp-idf/zh_CN/release-v5.4/esp32s3/api-reference/storage/spiffs.html#id5>`_。

**示例程序：**

* `storage/spiffs <https://github.com/espressif/esp-idf/tree/release/v5.4/examples/storage/spiffs>`_：SPIFFS 使用示例。
* `storage/spiffsgen <https://github.com/espressif/esp-idf/tree/release/v5.4/examples/storage/spiffsgen>`_：演示如何使用 SPIFFS 镜像生成工具在构建过程中自动从主机文件夹创建 SPIFFS 镜像。

LittleFS 文件系统
------------------------------

LittleFS 是一个专为微控制器和嵌入式设备设计的基于块的文件系统，原生支持磨损均衡、文件系统一致性检查、断电保护等功能。

**主要特性：**

* **优异断电恢复**：故障安全特性，断电保护能力强
* **动态磨损均衡**：自适应磨损均衡算法
* **极低 RAM 占用**：固定且极低的 RAM 使用量
* **多存储介质**：支持 SPI Flash 和 SD/MMC 卡
* **完整目录支持**：支持目录和子目录结构

**使用限制：**

* **平台兼容性**：与其他平台兼容性不如 FAT（主要用于嵌入式）
* **容量建议**：建议小于 128 MB 以获得最佳性能
* **第三方维护**：需通过 ESP 组件注册表获取
* **文档资源**：相比 FAT 文件系统文档资源较少

LittleFS 目前推荐用于一般类型的应用场景，特别是对断电保护要求较高的应用。

**参考文档：**

* `LittleFS 文件系统组件仓库 <https://github.com/joltwallet/esp_littlefs/tree/v1.14.5>`_ 。
* `LittleFS 文件系统组件使用说明 <https://components.espressif.com/components/joltwallet/littlefs/versions/1.14.5>`_ 。

**示例程序：**

* `storage/littlefs <https://github.com/espressif/esp-idf/tree/release/v5.4/examples/storage/littlefs>`_：LittleFS 使用示例。

虚拟文件系统 (VFS)
------------------------------

ESP-IDF 虚拟文件系统 (VFS) 组件可以为不同文件系统 (FAT，SPIFFS) 提供统一的接口，也可以为设备驱动程序提供类似文件读写的操作接口。

**参考文档：**

* `虚拟文件系统组件 <https://docs.espressif.com/projects/esp-idf/zh_CN/release-v5.4/esp32s3/api-reference/storage/vfs.html>`_。

存储安全
------------------------------

在选择和使用文件系统时，请注意以下安全相关事项：

* **数据加密**：NVS 和 FAT 文件系统支持数据加密，LittleFS 也支持加密功能，SPIFFS 目前不支持加密。
* **掉电保护**：NVS 和 LittleFS 具有较好的掉电保护机制，FAT 和 SPIFFS 在掉电时可能存在数据损坏风险。
* **完整性检查**：建议定期进行文件系统完整性检查，特别是在生产环境中。

**参考文档：**

* `存储安全 <https://docs.espressif.com/projects/esp-idf/zh_CN/release-v5.4/esp32s3/api-reference/storage/storage-security.html>`_。

文件系统设计建议
------------------------------

* 请参考：`文件系统设计建议 <https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/api-guides/file-system-considerations.html#id7>`_。

常见问题 (FAQ)
~~~~~~~~~~~~~~~~~

* 请参考：`ESP-FAQ 存储部分 <https://docs.espressif.com/projects/esp-faq/zh_CN/latest/software-framework/storage/index.html>`_
