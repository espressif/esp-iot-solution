File System
=============
:link_to_translation:`zh_CN:[中文]`

Supported file systems list:

.. list-table:: File System Feature Comparison
    :widths: 20 20 20 20 20
    :header-rows: 1

    * - Key Features
      - NVS Library
      - FAT File System
      - SPIFFS File System
      - LittleFS File System
    * - Characteristics
      - Key-value pair storage, secure interface
      - Operating system support, strong compatibility
      - Designed for embedded development, low resource usage
      - Low resource usage, fast read, write, erase speed
    * - Application Scenarios
      - Parameter storage
      - Audio/video, file storage
      - Audio/video, file storage
      - File storage
    * - Storage Media
      - SPI NOR Flash
      - SPI NOR Flash, SD/MMC card, USB flash drive
      - SPI NOR Flash
      - SPI NOR Flash, SD/MMC card
    * - Capacity
      - KB-MB
      - GB
      - ≤ 128 MB
      - < 128 MB
    * - Directory Support
      - ❌
      - ✅
      - ❌
      - ✅
    * - Wear Levelling
      - ✅
      - Optional
      - ✅
      - ✅
    * - Read/Write Efficiency
      - High
      - Medium
      - Medium
      - High
    * - Resource Usage
      - Low
      - Medium
      - Low
      - Very Low
    * - Power-down Protection
      - ✅
      - ❌
      - ❌
      - ✅
    * - Encryption
      - ✅
      - ✅
      - ❌
      - ✅


.. Note::

    * ✅: Supports this feature
    * ❌: Does not support this feature  
    * Read/write efficiency and resource usage are relative comparisons, actual performance depends on specific configuration and usage scenarios.


NVS Library
-----------------------

The Non-Volatile Storage (NVS) library is mainly used to read and write data stored in Flash NVS partitions. Data in NVS is saved in key-value pairs, where keys are ASCII strings and values can be integers, strings, or binary data (BLOB) types.

**Main Features:**

* **Key-value pair storage**: Supports multiple data types including integers, strings, BLOB
* **Power-down protection**: Atomic updates, ensuring data consistency
* **Encryption support**: Supports AES-XTS encryption
* **Wear levelling**: Built-in wear levelling mechanism

**Usage Limitations:**

* **Applicable scenarios**: Only suitable for configuration data, not suitable for frequent writes or large data
* **Partition size**: Recommended not to exceed 128 KB
* **Space requirements**: Requires sufficient space to store both old and new data simultaneously

If you need to store larger BLOBs or strings, please consider using the FAT file system based on the wear levelling library.

**Reference Documentation:**

- `Non-Volatile Storage Library <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html>`_.
- For mass production, you can use the `NVS Partition Generator Tool <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_partition_gen.html>`_.

**Example Programs:**

- Write a single integer value: `storage/nvs_rw_value <https://github.com/espressif/esp-idf/tree/release/v5.4/examples/storage/nvs_rw_value>`_.
- Write binary large object: `storage/nvs_rw_blob <https://github.com/espressif/esp-idf/tree/release/v5.4/examples/storage/nvs_rw_blob>`_.

FAT File System
--------------------------

ESP-IDF uses the FatFs library to implement support for the FAT file system. FatFs is a file system layer independent of platform and storage media, providing access to physical devices (such as Flash, SD card, USB flash drive) through a unified interface. Users can directly call FatFs interface operations, or use most of the FatFs library functionality through C standard library and POSIX API via VFS (Virtual File System).

**Main Features:**

* **Wide compatibility**: Compatible with PC and other platforms, supports standard FAT format
* **Multiple storage media**: Supports SPI Flash, SD/MMC cards, USB flash drives and other storage media
* **Directory support**: Supports multi-level directory structure, suitable for storing large numbers of files
* **Encryption support**: Supports partition encryption (`Flash Encryption <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/security/flash-encryption.html>`_)

**Usage Limitations:**

* **Power-down recovery**: Relatively weak power-down recovery capability
* **Partition size**: Minimum 8 sectors required when wear levelling is enabled

**Reference Documentation:**

- `Using FatFs with VFS <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/fatfs.html#fatfs-vfs>`_.
- `FATFS Image Generation and Parsing Tool <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/fatfsgen.html>`_.

**Example Programs:**

* `storage/sd_card <https://github.com/espressif/esp-idf/tree/release/v5.4/examples/storage/sd_card>`_: Access SD card using FAT file system.
* `storage/ext_flash_fatfs <https://github.com/espressif/esp-idf/tree/release/v5.4/examples/storage/ext_flash_fatfs>`_: Access external Flash chip using FAT file system.
* `peripherals/usb/host/msc <https://github.com/espressif/esp-idf/tree/release/v5.4/examples/peripherals/usb/host/msc>`_: Access USB flash drive using FAT file system (requires ESP32-S2/ESP32-S3).

SPIFFS File System
------------------------------

SPIFFS is an embedded file system dedicated to SPI NOR Flash, natively supporting wear levelling, file system consistency checks and other functions. Users can directly call the POSIX-style interfaces provided by SPIFFS, or operate most of SPIFFS functionality through VFS.

**Main Features:**

* **Embedded optimization**: Designed specifically for NOR Flash, low RAM usage
* **Static wear levelling**: Built-in wear levelling algorithm
* **Power-down recovery**: Certain degree of post-power-down repair functionality

**Usage Limitations:**

* **No directory support**: Only supports flat file structure
* **Capacity limitation**: Maximum support for 128 MB Flash
* **Performance degradation**: Significant performance degradation when usage exceeds 70%
* **Development stopped**: Maintenance has been discontinued

**Reference Documentation:**

* `SPIFFS File System <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/spiffs.html>`_.
* `SPIFFS Image Generation Tool <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/spiffs.html#id5>`_.

**Example Programs:**

* `storage/spiffs <https://github.com/espressif/esp-idf/tree/release/v5.4/examples/storage/spiffs>`_: SPIFFS usage example.
* `storage/spiffsgen <https://github.com/espressif/esp-idf/tree/release/v5.4/examples/storage/spiffsgen>`_: Demonstrates how to use the SPIFFS image generation tool to automatically create SPIFFS images from host folders during the build process.

LittleFS File System
-------------------------------

LittleFS is a block-based file system designed specifically for microcontrollers and embedded devices, natively supporting wear levelling, file system consistency checks, power-down protection and other functions.

**Main Features:**

* **Excellent power-down recovery**: Fault-safe features, strong power-down protection capability
* **Dynamic wear levelling**: Adaptive wear levelling algorithm
* **Extremely low RAM usage**: Fixed and extremely low RAM usage
* **Multiple storage media**: Supports SPI Flash and SD/MMC cards
* **Complete directory support**: Supports directory and subdirectory structure

**Usage Limitations:**

* **Platform compatibility**: Less compatible with other platforms than FAT (mainly for embedded use)
* **Capacity recommendation**: Recommended to be less than 128 MB for optimal performance
* **Third-party maintenance**: Needs to be obtained through ESP Component Registry
* **Documentation resources**: Fewer documentation resources compared to FAT file system

LittleFS is currently recommended for general application scenarios, especially applications with high power-down protection requirements.

**Reference Documentation:**

* `LittleFS File System Component Repository <https://github.com/joltwallet/esp_littlefs/tree/v1.14.5>`_ .
* `LittleFS File System Component Usage Guide <https://components.espressif.com/components/joltwallet/littlefs/versions/1.14.5>`_ .

**Example Programs:**

* `storage/littlefs <https://github.com/espressif/esp-idf/tree/release/v5.4/examples/storage/littlefs>`_: LittleFS usage example.

Virtual File System (VFS)
-------------------------------

The ESP-IDF Virtual File System (VFS) component can provide a unified interface for different file systems (FAT, SPIFFS), and can also provide file-like read/write operation interfaces for device drivers.

**Reference Documentation:**

* `Virtual File System Component <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/vfs.html>`_.

Storage Security
-------------------------------

When selecting and using file systems, please note the following security-related matters:

* **Data encryption**: NVS and FAT file systems support data encryption, LittleFS also supports encryption functionality, SPIFFS currently does not support encryption.
* **Power-down protection**: NVS and LittleFS have good power-down protection mechanisms, FAT and SPIFFS may have data corruption risks during power-down events.
* **Integrity checks**: It is recommended to regularly perform file system integrity checks, especially in production environments.

**Reference Documentation:**

* `Storage Security <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/storage-security.html>`_.

File System Design Recommendations
-----------------------------------

* Please refer to: `File handling design considerations <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/file-system-considerations.html#file-handling-design-considerations>`_.

Frequently Asked Questions (FAQ)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* Please refer to: `ESP-FAQ Storage Section <https://docs.espressif.com/projects/esp-faq/en/latest/software-framework/storage/index.html>`_
