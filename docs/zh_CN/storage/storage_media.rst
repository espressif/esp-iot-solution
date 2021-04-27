存储媒介
==========
:link_to_translation:`en:[English]`

已支持存储媒介列表：

+-----------------------------------------------------------------------------------------------------------------+--------------------------+--------------------------+------+----------+----------------------+-------------------------------------------------------------------------------------------------------------------------+------+
|                                                      名称                                                       |         关键特性         |         应用场景         | 容量 | 传输模式 |         速度         |                                                          驱动                                                           | 备注 |
+=================================================================================================================+==========================+==========================+======+==========+======================+=========================================================================================================================+======+
| `SPI Flash <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/spi_flash.html>`_ | 可与代码共用，无附加成本 | 参数存储、文本、图像存储 | MB   | SPI      | 40/80 MHz 4线        | `SPI Flash Driver <https://github.com/espressif/esp-idf/tree/master/components/spi_flash>`_                             |      |
+-----------------------------------------------------------------------------------------------------------------+--------------------------+--------------------------+------+----------+----------------------+-------------------------------------------------------------------------------------------------------------------------+------+
| `SD Card <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/sdmmc.html>`_       | 大容量、可插拔           | 声音、视频文件存储       | GB   | SDIO/SPI | 20/40 MHz 1线/4线    | `SD/SDIO/MMC Driver <https://github.com/espressif/esp-idf/blob/526f682/components/driver/include/driver/sdmmc_host.h>`__| \*1  |
+-----------------------------------------------------------------------------------------------------------------+--------------------------+--------------------------+------+----------+----------------------+-------------------------------------------------------------------------------------------------------------------------+------+
| `eMMC <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/sdmmc.html>`_          | 大容量、高速读写         | 声音、视频文件存储       | GB   | SDIO     | 20/40 MHz 1线/4线/8线| `SD/SDIO/MMC Driver <https://github.com/espressif/esp-idf/blob/526f682/components/driver/include/driver/sdmmc_host.h>`__| \*2  |
+-----------------------------------------------------------------------------------------------------------------+--------------------------+--------------------------+------+----------+----------------------+-------------------------------------------------------------------------------------------------------------------------+------+
| EEPROM_                                                                                                         | 可按字节寻址，低成本     | 参数存储                 | MB   | I2C      | 100 ~ 400 KHz        | :component:`eeprom <storage/eeprom>`                                                                                    |      |
+-----------------------------------------------------------------------------------------------------------------+--------------------------+--------------------------+------+----------+----------------------+-------------------------------------------------------------------------------------------------------------------------+------+

.. Note::

    * \*1. ESP32-S2 仅支持 SPI 模式
    * \*2. ESP32-S2 不支持

SPI Flash
-----------

ESP32/ESP32-S/ESP32-C 系列芯片默认使用 NOR Flash 存取用户代码和数据，Flash 可集成在模组或芯片中，容量一般为 4 MB、8 MB 或 16 MB。 对于 ESP-IDF v4.0 以上版本，SPI Flash 组件除了支持对主 Flash 进行读写操作以外，同时可支持外接第二块 Flash 芯片进行数据的存储。

用户可以使用 `分区表 <https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-guides/partition-tables.html>`_ 对 Flash 进行分区，根据分区表的功能划分，Flash 除了存放用户编译生成的二进制代码之外，也可以作为非易失性存储 (NVS) 空间存放应用程序参数，还可以将指定 Flash 空间挂载到文件系统（FatFS 等）存放文本、图像等文件。

Flash 芯片支持双线 (DOUT/DIO) 和四线 (QOUT/QIO) 操作模式，可通过配置工作在 40 MHz 或 80 MHz 模式。由于可以直接使用主 Flash 芯片进行数据存储，无需额外添加存储芯片，因此特别适合于容量需求较小 (MB)、集成度要求高、成本敏感的应用场景。

**参考文档：**

* `SPI Flash API <https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-reference/storage/spi_flash.html>`_


SD Card
-----------

ESP32 支持使用 SDIO 接口或 SPI 接口访问 SD 卡。SDIO 接口支持 1 线、4 线或 8 线模式，支持默认速率 20 MHz 和高速 40 MHz 两种速率。需要注意的是该接口至少占用 6 个 GPIO，并且只能使用 `固定的引脚 <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/sdmmc_host.html#sdmmc-host-driver>`_。SPI 接口支持通过 GPIO matrix 为 SD 卡指定任意的 IO，通过 CS 引脚可以支持对多个 SD 卡的访问，SPI 接口硬件设计上更加灵活，但是相比 SDIO 接口访问速率较低。

ESP-IDF 中的 ``SD/SDIO/MMC Driver`` 基于 SD 卡的两种访问模式进行了协议层的封装，提供了 SD 卡的初始化接口和协议层 API。SD 卡具有大容量、可插拔的特点，广泛适用于智能音响、电子相册等具有大容量存储需求的应用场景。

**参考文档：**

* `SD/SDIO/MMC 驱动 <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/sdmmc.html>`_：支持 SDIO 和 SPI 两种传输模式；
* `SDMMC 主机驱动 <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/sdmmc_host.html>`__：SDIO 模式；
* `SD SPI 主机驱动 <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/sdspi_host.html#sd-spi-host-driver>`_：SPI 模式；
* 使用 SPI 或 1-bit 模式，请注意 `引脚上拉需求 <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/sd_pullup_requirements.html>`_。

**示例程序：**

* `storage/sd_card <https://github.com/espressif/esp-idf/tree/526f682/examples/storage/sd_card>`_：访问使用 FAT 文件系统的 SD 卡。


eMMC
-----------

eMMC (embedded MMC) 内存芯片与 SD 卡具有相似的协议，可以使用与 SD 卡相同的驱动程序 `SD/SDIO/MMC 驱动 <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/sdmmc.html>`_。但是需要注意的是，eMMC 芯片仅能使用 SDIO 模式，不支持 SPI 模式。eMMC 目前在 8 线模式下支持默认速率 20 MHz 和高速 40 MHz，在 4 线模式下支持高速 40 MHz DDR 模式。

eMMC 一般以芯片的形式焊接到主板上，相比 SD 卡集成度更高，适用于可穿戴设备等，具有大容量存储需求同时对系统集成度有一定要求的场景。

**参考文档：**

* `SD/SDIO/MMC 驱动 <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/sdmmc.html>`__；
* `已支持的 eMMC 速度模式 <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/sdmmc_host.html#supported-speed-modes>`_。


EEPROM
---------

EEPROM（如 `AT24C0X 系列 <http://ww1.microchip.com/downloads/en/devicedoc/doc0180.pdf>`_）是 1024-16384 位的串行电可擦写存储器（通过控制引脚电平也可运行在只读模式），它的存储空间一般按照 ``word`` 进行分布，每个 ``word`` 包含 ``8-bit`` 空间。EEPROM 可按字节寻址，读写操作简单，特别适合于保存配置参数等，经过优化也可应用于对功耗和可靠性等有一定要求的工业和商业场景。

**已适配的 EEPROM 芯片:**

+------------+-----------------------+------+--------+----------------------------------------------------------------------------------------------------------------+----------------------------------------------+
|    名称    |         功能          | 总线 | 供应商 |                                                     规格书                                                     |                     驱动                     |
+============+=======================+======+========+================================================================================================================+==============================================+
| AT24C01/02 | 1024/2048 bits EEPROM | I2C  | Atmel  | `规格书    <http://ww1.microchip.com/downloads/en/devicedoc/atmel-8871f-seeprom-at24c01d-02d-datasheet.pdf>`__ | :component:`eeprom <storage/eeprom/at24c02>` |
+------------+-----------------------+------+--------+----------------------------------------------------------------------------------------------------------------+----------------------------------------------+
