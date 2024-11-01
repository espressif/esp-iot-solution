ESP MMAP ASSETS
================
:link_to_translation:`zh_CN:[中文]`

This module is mainly used to package resources (such as images, fonts, etc.) and map them directly for user access. At the same time, it also integrates rich image preprocessing functions.

Features
-----------

**Adding Import File Types**
   - Support for various file formats such as ``.bin``, ``.jpg``, ``.ttf``, etc.

**Enable Split JPG**
   - Need to use ``SJPG`` to parse. Refer to the LVGL `SJPG <https://docs.lvgl.io/8.4/libs/sjpg.html>`__.

**Enable Split PNG**
   - Need to use ``SPNG`` to parse. See the component `esp_lv_decoder <esp_lv_decoder.html>`__.

**Set Split Height**
   - Set the split height, depends on ``MMAP_SUPPORT_SJPG``, ``MMAP_SUPPORT_SPNG`` or ``MMAP_SUPPORT_SQOI``.

**Support QOI**
   - Supports converting images from JPG and PNG formats to QOI.
   - Supports split qoi, which requires the use of ``esp_lv_decoder`` for parsing.

**Supporting multiple partitions**
   - The spiffs_create_partition_assets function allows you to mount multiple partitions, each with its own processing logic.

**Supporting LVGL Image Converter**
   - By enabling ``MMAP_SUPPORT_RAW`` and related configurations, JPG and PNG can be converted to ``Bin`` so that they can be used in ``LVGL``.

CMake options
------------------
Optionally, users can opt to have the image automatically flashed together with the app binaries, partition tables, etc. on idf.py flash by specifying FLASH_IN_PROJECT. For example:

.. code:: c

   /* partitions.csv
    * -----------------------------------------------------------------------
    * | Name                | Type | SubType | Offset | Size  | Flags     |
    * --------------------------------------------------------
    * | my_spiffs_partition | data | spiffs  |        | 6000K |           |
    * -----------------------------------------------------------------------
    */
    spiffs_create_partition_assets(
        my_spiffs_partition
        my_folder
        FLASH_IN_PROJECT
        MMAP_FILE_SUPPORT_FORMAT ".png")

The component also supports the following options, which allow you to enable various pre-processing of the image at compile time.

.. code:: c

   set(options FLASH_IN_PROJECT,           // Defines storage type (flash in project)
                MMAP_SUPPORT_SJPG,         // Enable support for SJPG format
                MMAP_SUPPORT_SPNG,         // Enable support for SPNG format
                MMAP_SUPPORT_QOI,          // Enable support for QOI format
                MMAP_SUPPORT_SQOI,         // Enable support for SQOI format
                MMAP_SUPPORT_RAW,          // Enable support for RAW format (LVGL conversion only)
                MMAP_RAW_DITHER,           // Enable dithering for RAW images (LVGL conversion only)
                MMAP_RAW_BGR_MODE)         // Enable BGR mode for RAW images (LVGL conversion only)

    set(one_value_args MMAP_FILE_SUPPORT_FORMAT,    // Specify supported file format (e.g., .png, .jpg)
                   MMAP_SPLIT_HEIGHT,               // Define the height for image splitting
                   MMAP_RAW_FILE_FORMAT)            // Specify the file format for RAW images (LVGL conversion only)

Application Examples
---------------------

Generate Header(mmap_generate_my_spiffs_partition.h)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
This header file is automatically generated and includes essential definitions for memory-mapped assets.

.. code:: c

   #include "mmap_generate_my_spiffs_partition.h"

   #define TOTAL_MMAP_FILES      2
   #define MMAP_CHECKSUM         0xB043

   enum MMAP_FILES {
      MMAP_JPG_JPG = 0,   /*!< jpg.jpg */
      MMAP_PNG_PNG = 1,   /*!< png.png */
   };

Create Assets Handle
^^^^^^^^^^^^^^^^^^^^^
The assets config ensures consistency with ``mmap_generate_my_spiffs_partition.h``. It sets the ``max_files`` and ``checksum``, verifying the header and memory-mapped binary file.

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
You can use the enum defined in ``mmap_generate_my_spiffs_partition.h`` to get asset information.

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
