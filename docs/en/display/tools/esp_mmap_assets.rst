ESP MMAP ASSETS
================
:link_to_translation:`zh_CN:[中文]`

This module is primarily used for packaging assets (such as images, fonts, etc.) and directly mapping them for user access.

Features
-----------

**Adding Import File Types**
   - Support for various file formats such as ``.bin``, ``.jpg``, ``.ttf``, etc.

**Enable Split JPG**
   - Need to use ``SJPG`` to parse. Refer to the LVGL `SJPG <https://docs.lvgl.io/8.4/libs/sjpg.html>`__.

**Enable Split PNG**
   - Need to use ``SPNG`` to parse. Refer to the component `esp_lv_spng <esp_lv_spng.html>`__.

**Set Split Height**
   - Set the split height, depends on ``MMAP_SUPPORT_SJPG`` or ``MMAP_SUPPORT_SPNG``.

CMake
---------
Optionally, users can opt to have the image automatically flashed together with the app binaries, partition tables, etc. on idf.py flash by specifying FLASH_IN_PROJECT. For example:

.. code:: c

   /* partitions.csv
    * --------------------------------------------------------
    * | Name               | Type | SubType | Offset | Size  | Flags     |
    * --------------------------------------------------------
    * | my_spiffs_partition | data | spiffs  |        | 6000K |           |
    * --------------------------------------------------------
    */
    spiffs_create_partition_assets(my_spiffs_partition my_folder FLASH_IN_PROJECT)

Application Examples
---------------------

Generate Header(assets_generate.h)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
This header file is automatically generated and includes essential definitions for memory-mapped assets.

.. code:: c

   #include "esp_mmap_assets.h"

   #define TOTAL_MMAP_FILES      2
   #define MMAP_CHECKSUM         0xB043

   enum MMAP_FILES {
      MMAP_JPG_JPG = 0,   /*!< jpg.jpg */
      MMAP_PNG_PNG = 1,   /*!< png.png */
   };

Create Assets Handle
^^^^^^^^^^^^^^^^^^^^^
The assets config ensures consistency with ``assets_generate.h``. It sets the ``max_files`` and ``checksum``, verifying the header and memory-mapped binary file.

.. code:: c

   mmap_assets_handle_t asset_handle;

   const mmap_assets_config_t config = {
      .partition_label = "my_spiffs_partition",
      .max_files = TOTAL_MMAP_FILES,
      .checksum = MMAP_CHECKSUM,
   };

   ESP_ERROR_CHECK(mmap_assets_new(&config, &asset_handle));

Assets Usage
^^^^^^^^^^^^^^^^^^^^^
You can use the enum defined in ``assets_generate.h`` to get asset information.

.. code:: c

    const char *name = mmap_assets_get_name(asset_handle, MMAP_JPG_JPG);
    const void *mem = mmap_assets_get_mem(asset_handle, MMAP_JPG_JPG);
    int size = mmap_assets_get_size(asset_handle, MMAP_JPG_JPG);
    int width = mmap_assets_get_width(asset_handle, MMAP_JPG_JPG);
    int height = mmap_assets_get_height(asset_handle, MMAP_JPG_JPG);

    ESP_LOGI(TAG, "Name:[%s], Mem:[%p], Size:[%d bytes], Width:[%d px], Height:[%d px]", name, mem, size, width, height);

API Reference
-----------------

.. include-build-file:: inc/esp_mmap_assets.inc
