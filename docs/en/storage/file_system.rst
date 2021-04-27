File System
=============
:link_to_translation:`zh_CN:[中文]`

Supported file systems:

+--------------------------+---------------------------------------------------+--------------------------------------------------+--------------------------------------------------------+
| Key Features             | NVS Library                                       | FAT File System                                  | SPIFFS File System                                     |
+==========================+===================================================+==================================================+========================================================+
| Features                 | Operates on key-value pairs, with safe interfaces | Operation system supported, strong compatibility | Developed for embedded systems, low resource occupancy |
+--------------------------+---------------------------------------------------+--------------------------------------------------+--------------------------------------------------------+
| Application Scenarios    | Stores parameters                                 | Stores audio, video and other files              | Stores audio, video and other files                    |
+--------------------------+---------------------------------------------------+--------------------------------------------------+--------------------------------------------------------+
| Size                     | KB-MB                                             | GB                                               | < 128 MB                                               |
+--------------------------+---------------------------------------------------+--------------------------------------------------+--------------------------------------------------------+
| Directory Support        | X                                                 | √                                                | X                                                      |
+--------------------------+---------------------------------------------------+--------------------------------------------------+--------------------------------------------------------+
| Wear Levelling           | √                                                 | Optional                                         | √                                                      |
+--------------------------+---------------------------------------------------+--------------------------------------------------+--------------------------------------------------------+
| R/W Efficiency           | 0                                                 | 0                                                | 0                                                      |
+--------------------------+---------------------------------------------------+--------------------------------------------------+--------------------------------------------------------+
| Resources Occupancy      | 0                                                 | 0                                                | 1                                                      |
+--------------------------+---------------------------------------------------+--------------------------------------------------+--------------------------------------------------------+
| Power Failure Protection | √                                                 | X                                                | X                                                      |
+--------------------------+---------------------------------------------------+--------------------------------------------------+--------------------------------------------------------+
| Encryption               | √                                                 | √                                                | X                                                      |
+--------------------------+---------------------------------------------------+--------------------------------------------------+--------------------------------------------------------+


.. Note::

    * 0: data not available or not for comparison.
    * 1: low RAM occupancy.


NVS Library
---------------

Non-volatile storage (NVS) is used to read and write data stored in the flash NVS partition. NVS operated on key-value pairs. Keys are ASCII strings; values can be integers, strings and variable binary large object (BLOB). NVS supports power loss protection and data encryption, and works best for storing many small values, such as application parameters. If you need to store large blobs or strings, please consider using the facilities provided by the FAT file system on top of the wear levelling library.

**Related documents:**

- `Non-volatile storage library <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html>`_.
- For mass production, you can use the `NVS Partition Generator Utility <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_partition_gen.html>`_.

**Examples:**

- Write a single integer value: `storage/nvs_rw_value <https://github.com/espressif/esp-idf/tree/526f682/examples/storage/nvs_rw_value>`_.
- Write a blob: `storage/nvs_rw_blob <https://github.com/espressif/esp-idf/tree/526f682/examples/storage/nvs_rw_blob>`_.

FAT File System
---------------------

ESP-IDF uses the FatFs library to work with FAT file system. FatFs is a file system layer independent to platform and storage media that can realize access to physical devices (e.g., flash, SD card) via a unified interface. Although the library can be used directly, many of its features can be accessed via VFS, using the C standard library and POSIX API functions.

The operating system of FAT is compatible with a wide range of mobile storage devices such as USB memory disc or SD cards. And ESP32 series chips can access these common storage devices by supporting the FAT file system.

**Related documents:**

- `Using FatFs with VFS <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/fatfs.html#fatfs-vfs>`_.
- `Using FatFs with VFS and SD cards <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/fatfs.html#fatfs-vfs-sd>`_.

**Examples:**

* `storage/sd_card <https://github.com/espressif/esp-idf/tree/526f682/examples/storage/sd_card>`_: access the SD card which uses the FAT file system.
* `storage/ext_flash_fatfs <https://github.com/espressif/esp-idf/tree/master/examples/storage/ext_flash_fatfs>`_: access the external flash chip which uses the FAT file system.

SPIFFS File System
----------------------

SPIFFS is a file system intended for SPI NOR flash devices on embedded targets. It supports wear levelling, file system consistency checks, and more. Users can directly use the Posix interfaces provided by SPIFFS, or use many of its features via VFS.

As a dedicated file system for SPI NOR flash devices on embedded targets, the SPIFFS occupies less RAM resources than FAT and is only used to support flash chips with capacities less than 128 MB.

**Related documents:**

* `SPIFFS Filesystem <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/spiffs.html>`_.
* `Two Tools to Generate SPIFFS Images <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/spiffs.html#id6>`_.

**Examples:**

* `storage/spiffs <https://github.com/espressif/esp-idf/tree/526f682/examples/storage/spiffs>`_: SPIFFS examples.


Virtual File System (VFS)
------------------------------

The Virtual File System (VFS) component from ESP-IDF provides a unified interface for different file systems (FAT, SPIFFS), and also provides a file-like interface for device drivers.

**Related documents:**

* `Virtual Filesystem Component <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/vfs.html>`_.
