Storage Media
==================
:link_to_translation:`zh_CN:[中文]`

Supported storage media list:

.. list-table:: 
   :header-rows: 1
   :widths: 20 15 15 7 10 12 25 8

   * - Name
     - Key Features
     - Application Scenarios
     - Capacity
     - Transmission Mode
     - Speed
     - Driver
     - Notes
   * - `SPI Flash <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/spi_flash.html>`_
     - Can be shared with code, no additional cost
     - Parameter storage, text, image storage
     - MB
     - SPI
     - 40/80 MHz 4-line
     - `SPI Flash Driver <https://github.com/espressif/esp-idf/tree/master/components/spi_flash>`_
     - 
   * - `SD Card <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/sdmmc.html>`_
     - Large capacity, pluggable
     - Audio, video file storage
     - GB
     - SDIO/SPI
     - 20/40 MHz 1/4-line
     - `SD/SDIO/MMC Driver for SD <https://github.com/espressif/esp-idf/tree/master/components/sdmmc>`_
     - \*1
   * - `eMMC <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/sdmmc.html>`_
     - Large capacity, high-speed read/write
     - Audio, video file storage
     - GB
     - SDIO
     - 20/40 MHz 1/4/8-line
     - `SD/SDIO/MMC Driver for eMMC <https://github.com/espressif/esp-idf/tree/master/components/sdmmc>`_
     - \*2
   * - `USB Flash Drive <https://components.espressif.com/components/espressif/usb_host_msc>`_
     - Large capacity, pluggable
     - Audio, video file storage
     - GB
     - USB
     - 12 Mbps/480 Mbps
     - `USB Host Driver <https://github.com/espressif/esp-usb/tree/master/host/class/msc/usb_host_msc>`_
     - \*3
   * - `EEPROM <https://components.espressif.com/components/espressif/at24c02>`_
     - Byte-addressable, low cost
     - Parameter storage
     - MB
     - I2C
     - 100 ~ 400 kHz
     - `eeprom driver <storage/eeprom>`_
     - 

.. Note::

    * \*1. SD card SDIO mode is only available on ESP chips that support SDIO host peripherals (such as ESP32, ESP32-S3);
    * \*2. Only available on ESP chips that support SDIO host peripherals (such as ESP32, ESP32-S3);
    * \*3. Only available on ESP chips that support USB-OTG peripherals (such as ESP32-S2, ESP32-S3).

SPI Flash
------------

ESP series chips use NOR Flash by default to store user code and data. Flash can be integrated into modules or chips, with capacities typically of 4 MB, 8 MB, or 16 MB.

Users can use `partition tables <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/partition-tables.html>`_ to partition Flash. Based on the functional division of partition tables, the main Flash designated space can be used as non-volatile storage (NVS) space to store application parameters, or mounted to file systems (FatFS, etc.) to store text, images and other files.

Flash chips support dual-line (DOUT/DIO) and quad-line (QOUT/QIO) operation modes, and can be configured to work in 40 MHz or 80 MHz modes. Since the main Flash chip can be used directly for data storage without additional storage chips, it is particularly suitable for applications with small capacity requirements (MB), high integration requirements, and cost sensitivity.

.. Note::

    For ESP-IDF v4.0 and above, the SPI Flash component not only supports using the main Flash for user data storage, but also supports connecting to a second external Flash chip for data storage.

**Reference Documentation:**

* `SPI Flash API <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/spi_flash.html>`_
* `NVS Flash API <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html>`_
* `FatFS API <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/fatfs.html>`_

**Example Programs:**

* `storage/nvs <https://github.com/espressif/esp-idf/tree/master/examples/storage/nvs>`_: Use NVS for parameter storage;
* `storage/fatfs <https://github.com/espressif/esp-idf/tree/master/examples/storage/fatfs>`_: Use FatFS for file storage.

SD Card
------------

ESP series chips support using SDIO interface or SPI interface to access SD cards. The SDIO interface supports 1-line, 4-line, or 8-line modes, supporting default rate of 20 MHz or high-speed rate of 40 MHz. It should be noted that the SDIO interface generally can only use `fixed pins <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/sdmmc_host.html#sdmmc-host-driver>`_. The SPI interface supports specifying arbitrary IOs for SD cards through GPIO matrix, and can support access to multiple SD cards through CS pins. The SPI interface is more flexible in hardware design, but has lower access rates compared to the SDIO interface.

The ``SD/SDIO/MMC Driver`` in ESP-IDF provides protocol layer encapsulation based on the two access modes of SD cards, providing SD card initialization interfaces and protocol layer APIs. SD cards have the characteristics of large capacity and pluggability, and are widely used in application scenarios such as smart speakers and electronic photo albums that have large capacity storage requirements.

**Reference Documentation:**

* `SD/SDIO/MMC Driver <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/sdmmc.html>`_: Provides SD card initialization interfaces and protocol layer APIs;
* `SDMMC Host Driver <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/sdmmc_host.html>`__: Provides SDIO mode interface implementation;
* `SD SPI Host Driver <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/sdspi_host.html#sd-spi-host-driver>`_: Provides SPI mode interface implementation;
* When using SPI or 1-bit mode, please note the `pin pull-up requirements <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/sd_pullup_requirements.html>`_.

**Example Programs:**

* `storage/sd_card <https://github.com/espressif/esp-idf/tree/master/examples/storage/sd_card>`_: Access SD card using FAT file system.


eMMC
------------

eMMC (embedded MMC) memory chips have similar protocols to SD cards and can use the same driver `SD/SDIO/MMC Driver <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/sdmmc.html>`_. However, it should be noted that eMMC chips can only use SDIO mode and do not support SPI mode.

eMMC is generally soldered to the motherboard in chip form, with higher integration than SD cards, suitable for wearable devices and other scenarios that have large capacity storage requirements while having certain requirements for system integration.

**Reference Documentation:**

* `SD/SDIO/MMC Driver <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/sdmmc.html>`__;
* `Supported eMMC Speed Modes <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/sdmmc_host.html#supported-speed-modes>`_.

**Example Programs:**

* `storage/emmc <https://github.com/espressif/esp-idf/tree/master/examples/storage/emmc>`_: Access eMMC storage using FAT file system.


EEPROM
----------

EEPROM (such as AT24C0X series) is a 1024-16384 bit serial electrically erasable memory (can also run in read-only mode by controlling pin levels), its storage space is generally distributed by ``word``, with each ``word`` containing ``8-bit`` space. EEPROM is byte-addressable, with simple read/write operations, particularly suitable for saving configuration parameters. After optimization, it can also be applied to industrial and commercial scenarios with certain requirements for power consumption and reliability.

**Adapted EEPROM Chips:**

.. list-table::
   :header-rows: 1
   :widths: 15 25 10 15 40 30

   * - Name
     - Function
     - Bus
     - Vendor
     - Datasheet
     - Driver
   * - AT24C01/02
     - 1024/2048 bits EEPROM
     - I2C
     - Atmel
     - `Datasheet <https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-8871F-SEEPROM-AT24C01D-02D-Datasheet.pdf>`_
     - `AT24C02 driver <storage/eeprom/at24c02>`_


Frequently Asked Questions (FAQ)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* Please refer to: `ESP-FAQ Storage Section <https://docs.espressif.com/projects/esp-faq/en/latest/software-framework/storage/index.html>`_
