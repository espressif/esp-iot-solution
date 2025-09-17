Flash 加密
*****************

Flash 加密是 ESP32 系列芯片的重要安全特性，用于保护存储在外部 Flash 中固件和数据的机密性。启用 Flash 加密后，Flash 中的内容将被透明加密，防止通过物理方式读取获得明文固件和数据。

Flash 加密的核心特点：

- **透明加密解密**：通过 MMU 缓存访问时自动处理加密解密
- **硬件实现**：加密密钥存储在 eFuse 中，软件无法直接访问
- **XTS-AES 算法**：采用专为存储设备设计的安全加密算法
- **分区选择性加密**：可指定需要加密的分区类型

更多详细信息请参考：`ESP-IDF Flash 加密文档 <https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/security/flash-encryption.html>`_

Flash 加密与安全启动
~~~~~~~~~~~~~~~~~~~~~~~

Flash 加密与安全启动是互补的安全功能：

- **Flash 加密**：保护数据机密性，防止读取 Flash 内容
- **安全启动**：保证代码完整性，防止运行未授权固件

建议在生产环境中同时启用两项功能以获得最佳安全保护。详情参考：`Flash 加密与安全启动 <https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/security/flash-encryption.html#flash-encryption-and-secure-boot>`_

加密范围
~~~~~~~~~

Flash 加密会自动加密以下内容：

- 二级引导加载程序
- 所有 app 类型分区
- 分区表
- NVS 密钥分区
- 用户标记为 "encrypted" 的分区

**重要提示**：NVS 分区加密和 Flash 加密是两个独立的功能。需要使用 NVS 加密功能。详见：`NVS 加密文档 <https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/api-reference/storage/nvs_encryption.html>`_

加密范围详细说明：`加密分区 <https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/security/flash-encryption.html#encrypted-partitions>`_

Flash 加密功能支持列表
~~~~~~~~~~~~~~~~~~~~~~~

以下是 ESP32 系列芯片 Flash 加密功能对比表，包含加密算法、密钥长度/类型、硬件支持、密钥存储方式、密钥来源（精简描述）：

.. list-table:: ESP32 系列芯片 Flash 加密功能对比
    :header-rows: 1

    * - 芯片型号
      - 支持的加密算法
      - 密钥长度/类型
      - 硬件加速/支持
      - 密钥存储方式
      - 密钥来源
    * - ESP32
      - AES-256（自定义实现）
      - 256-bit
      - 有
      - eFuse
      - 硬件随机数生成
    * - ESP32-S2
      - XTS-AES-128、XTS-AES-256
      - 256-bit 或 512-bit
      - 有
      - eFuse（BLOCK_KEYN）
      - 硬件随机数或主机生成
    * - ESP32-S3
      - XTS-AES-128、XTS-AES-256
      - 256-bit 或 512-bit
      - 有
      - eFuse（BLOCK_KEYN）
      - 硬件随机数或主机生成
    * - ESP32-C2
      - XTS-AES-128
      - 256-bit
      - 有
      - eFuse（BLOCK_KEY0）
      - 硬件随机数或主机生成
    * - ESP32-C3
      - XTS-AES-128
      - 256-bit
      - 有
      - eFuse（BLOCK_KEYN）
      - 硬件随机数或主机生成
    * - ESP32-C5
      - XTS-AES-128
      - 256-bit
      - 有
      - eFuse（BLOCK_KEYN）或Key Manager
      - 硬件随机数、主机生成或Key Manager
    * - ESP32-C6
      - XTS-AES-128
      - 256-bit
      - 有
      - eFuse（BLOCK_KEYN）
      - 硬件随机数或主机生成
    * - ESP32-H2
      - XTS-AES-128
      - 256-bit
      - 有
      - eFuse（BLOCK_KEYN）
      - 硬件随机数或主机生成
    * - ESP32-P4
      - XTS-AES-128、XTS-AES-256
      - 256-bit 或 512-bit
      - 有
      - eFuse（BLOCK_KEYN）
      - 硬件随机数或主机生成

**说明：**
- ESP32-C5 支持 Key Manager 作为密钥来源和存储方式（参考：`ESP32-C5 SoC 功能宏定义 <https://docs.espressif.com/projects/esp-idf/en/latest/esp32c5/api-reference/system/soc_caps.html#macros>`_）。
- 其它芯片均为 eFuse 存储，密钥可由硬件随机数生成或主机生成后烧录（参考：`ESP32-S2 Flash 加密 <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/security/flash-encryption.html#key-points-about-flash-encryption>`_ 和 `ESP32-C3 Flash 加密 <https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/security/flash-encryption.html>`_）。

开发模式和发布模式
~~~~~~~~~~~~~~~~~~~~~

Flash 加密提供两种模式：

**开发模式 (Development Mode)**

- 允许多次烧录明文固件
- 引导加载程序自动处理加密
- 适用于开发和测试阶段
- 安全性相对较低

**发布模式 (Release Mode)**  

- 禁用明文固件烧录
- 只能通过 OTA 更新明文固件
- 适用于生产环境
- 提供最高安全性

**状态检查**：

- 使用 `esp_flash_encryption_enabled() <https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/api-reference/peripherals/spi_flash/index.html#_CPPv428esp_flash_encryption_enabledv>`__ 检查 Flash 加密状态
- 使用 `esp_get_flash_encryption_mode() <https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/api-reference/peripherals/spi_flash/index.html#_CPPv429esp_get_flash_encryption_modev>`__ 获取加密模式（开发模式或发布模式）

配置方法详见：`Flash 加密设置 <https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/security/flash-encryption.html#id4>`_

加密过程
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Flash 加密的基本过程：

1. **首次启动**：检测 eFuse 状态，启动加密流程
2. **密钥生成**：生成随机加密密钥并存储到 eFuse (可跳过，支持烧录自定义密钥)
3. **就地加密**：加密 Flash 中的指定分区内容
4. **设置标志**：标记 Flash 加密已启用
5. **重启系统**：开始正常的加密模式运行

完整的加密过程说明：`Flash 的加密过程 <https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/security/flash-encryption.html#id3>`_

应用程序访问加密分区
~~~~~~~~~~~~~~~~~~~~~~~

应用程序可以透明地访问加密的 Flash 内容：

**读取加密 Flash**：

- 使用 `esp_partition_read() <https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/api-reference/storage/partition.html#_CPPv418esp_partition_readPK15esp_partition_t6size_tPv6size_t>`__ 或 `esp_flash_read_encrypted() <https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/api-reference/peripherals/spi_flash/index.html#_CPPv424esp_flash_read_encryptedP11esp_flash_t8uint32_tPv8uint32_t>`__ 读取明文内容（自动解密）
- 使用 `esp_flash_read() <https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/api-reference/peripherals/spi_flash/index.html#_CPPv414esp_flash_readP11esp_flash_tPv8uint32_t8uint32_t>`__ 读取原始加密数据（不解密）

**写入加密 Flash**：

- 使用 `esp_partition_write() <https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/api-reference/storage/partition.html#_CPPv419esp_partition_writePK15esp_partition_t6size_tPKv6size_t>`__ 写入明文内容（自动加密）
- 使用 `esp_flash_write_encrypted() <https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/api-reference/peripherals/spi_flash/index.html#_CPPv425esp_flash_write_encryptedP11esp_flash_t8uint32_tPKv8uint32_t>`__ 写入原始加密内容（跳过自动加密）


更多 API 详情：`在加密的 Flash 中读写数据 <https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/security/flash-encryption.html#reading-writing-content>`_

重新烧录 Flash
~~~~~~~~~~~~~~~

**开发模式下**：

- 通过 ``idf.py encrypted-app-flash`` 烧录新的应用程序明文，烧录时会自动加密
- 通过 ``idf.py encrypted-flash`` 烧录所有分区明文，烧录时会自动加密

**发布模式下**：

- 只能通过 OTA 更新明文固件
- 只能手动烧录密文固件（仅当 UART ROM Downloads 启用时支持）

详细说明：`重新烧录更新后的分区 <https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/security/flash-encryption.html#encrypt-partitions>`_

取消加密
~~~~~~~~~~~

**仅开发模式支持**：在加密状态下通过烧录 eFuse ``SPI_BOOT_CRYPT_CNT`` 以禁用 Flash 加密（发布模式无法取消）。

**警告**：每个芯片只有有限次数的开关加密次数，通常为 3 次（关闭->开启->关闭->开启），请谨慎操作。

详细步骤：`关闭 Flash 加密 <https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/security/flash-encryption.html#disabling-flash-encryption>`_

示例代码
~~~~~~~~~~~

完整的 Flash 加密使用示例请参考：

- `ESP-IDF Flash 加密示例 <https://github.com/espressif/esp-idf/tree/master/examples/security/flash_encryption>`_
- `安全功能综合示例 <https://github.com/espressif/esp-idf/tree/master/examples/security/security_features_app>`_

这些示例展示了：

- Flash 加密状态检查
- 加密分区读写操作  
- NVS 和 FATFS 在加密环境下的使用
- 开发模式和发布模式的配置方法

最佳实践
~~~~~~~~~~~

1. **生产环境使用发布模式**
2. **每个设备使用唯一密钥**
3. **结合安全启动使用**
4. **合理规划分区加密策略**
5. **测试 OTA 更新流程**

更多最佳实践：`Flash 加密最佳实践 <https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/security/flash-encryption.html#flash-encrypt-best-practices>`_

常见问题 (FAQ)
~~~~~~~~~~~~~~~~~

* 请参考：`ESP-FAQ 安全部分 <https://docs.espressif.com/projects/esp-faq/zh_CN/latest/software-framework/security.html>`_
