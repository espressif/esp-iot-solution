ESP MMAP ASSETS
================
:link_to_translation:`zh_CN:[中文]`

This module is mainly used to package resources (such as images, fonts, etc.) and map them directly for user access. At the same time, it also integrates rich image preprocessing functions.

Features
-----------

**Adding Import File Types**
   - Support for various file formats such as ``.bin``, ``.jpg``, ``.ttf``, etc.

**Multiple Access Modes**
   - **Partition Mode**: Access assets from ESP32 partition (default)
   - **Memory Mapping Mode**: Direct memory access for maximum performance
   - **File System Mode**: Access assets from file system for development and testing

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

CMake Options
------------------
The following options are supported. These options allow you to configure various aspects of image handling.

.. code:: cmake

   set(options
       FLASH_IN_PROJECT,
       FLASH_APPEND_APP,
       MMAP_SUPPORT_SJPG,
       MMAP_SUPPORT_SPNG,
       MMAP_SUPPORT_QOI,
       MMAP_SUPPORT_SQOI,
       MMAP_SUPPORT_RAW,
       MMAP_RAW_DITHER,
       MMAP_RAW_BGR_MODE)

   set(one_value_args
       MMAP_FILE_SUPPORT_FORMAT,
       MMAP_SPLIT_HEIGHT,
       MMAP_RAW_FILE_FORMAT,
       IMPORT_INC_PATH,
       COPY_PREBUILT_BIN)

Option Explanations
~~~~~~~~~~~~~~~~~~~~

General Options
^^^^^^^^^^^^^^^^^^^^

   - ``FLASH_IN_PROJECT``: Users can opt to have the image automatically flashed together with the app binaries, partition tables, etc. on ``idf.py flash``.
   
   - ``FLASH_APPEND_APP``: Enables appending binary data (``bin``) to the application binary (``app_bin``).

   - ``IMPORT_INC_PATH``: Target path for generated include files. Defaults to referencing component location.
   
   - ``COPY_PREBUILT_BIN``: Copies pre-generated binary files to target directory. This option allows you to use externally generated asset binaries instead of building them from source files.
   
   - ``MMAP_FILE_SUPPORT_FORMAT``: Specifies supported file formats (e.g., ``.png``, ``.jpg``, ``.ttf``).
   
   - ``MMAP_SPLIT_HEIGHT``: Defines height for image splitting to reduce memory usage. Depends on:

      - ``MMAP_SUPPORT_SJPG``
      - ``MMAP_SUPPORT_SPNG``
      - ``MMAP_SUPPORT_SQOI``

General demo
""""""""""""""""

.. code:: c

   spiffs_create_partition_assets(
      my_spiffs_partition
      my_folder
      FLASH_IN_PROJECT
      MMAP_FILE_SUPPORT_FORMAT ".jpg,.png,.ttf"
   )

Pre-built binary assets demo
"""""""""""""""""""""""""""""""

.. code:: c

   spiffs_create_partition_assets(
      my_spiffs_partition
      "${ASSETS_DIR}"
      FLASH_IN_PROJECT
      COPY_PREBUILT_BIN "${ASSETS_DIR}/prebuilt.bin"
   )

Supported Image Formats
^^^^^^^^^^^^^^^^^^^^^^^^^

   - ``MMAP_SUPPORT_SJPG``: Enables support for SJPG format.
   - ``MMAP_SUPPORT_SPNG``: Enables support for SPNG format.
   - ``MMAP_SUPPORT_QOI``: Enables support for QOI format.
   - ``MMAP_SUPPORT_SQOI``: Enables support for SQOI format. Depends on:

      - ``MMAP_SUPPORT_QOI``

Image Splitting Demo
"""""""""""""""""""""""

.. code:: c

   spiffs_create_partition_assets(
      my_spiffs_partition
      my_folder
      FLASH_IN_PROJECT
      MMAP_FILE_SUPPORT_FORMAT ".jpg"
      MMAP_SUPPORT_SJPG
      MMAP_SPLIT_HEIGHT 16
   )

LVGL Bin Support
^^^^^^^^^^^^^^^^^^^^

   - ``MMAP_SUPPORT_RAW``: Converts images to LVGL-supported **Binary** data.
      
      **References:**
         - LVGL v8: `Use detailed reference <https://github.com/W-Mai/lvgl_image_converter>`__
         - LVGL v9: `Use detailed reference <https://github.com/lvgl/lvgl/blob/master/scripts/LVGLImage.py>`__

   - ``MMAP_RAW_FILE_FORMAT``: Specifies file format for RAW images.

      - LVGL v8: ``{true_color, true_color_alpha, true_color_chroma, indexed_1, indexed_2, indexed_4, indexed_8, alpha_1, alpha_2, alpha_4, alpha_8, raw, raw_alpha, raw_chroma}``
      - LVGL v9: Not used.

   - ``MMAP_RAW_COLOR_FORMAT``: Specifies color format for RAW images.

      - LVGL v8: ``{RGB332, RGB565, RGB565SWAP, RGB888}``
      - LVGL v9: ``{L8, I1, I2, I4, I8, A1, A2, A4, A8, ARGB8888, XRGB8888, RGB565, RGB565A8, ARGB8565, RGB888, AUTO, RAW, RAW_ALPHA}``

   - ``MMAP_RAW_DITHER``: Enables **dithering** for RAW images.

      - LVGL v8: Requires dithering.
      - LVGL v9: Not used.

   - ``MMAP_RAW_BGR_MODE``: Enables **BGR mode** for RAW images.

      - LVGL v8: Not used.
      - LVGL v9: Not used.

LVGL v9 demo
""""""""""""""""

.. code:: c

   spiffs_create_partition_assets(
       .........
       MMAP_FILE_SUPPORT_FORMAT ".png"
       MMAP_SUPPORT_RAW
       MMAP_RAW_COLOR_FORMAT "ARGB8888"
   )

LVGL v8 demo
""""""""""""""""

.. code:: c

   spiffs_create_partition_assets(
       .........
       MMAP_FILE_SUPPORT_FORMAT ".png"
       MMAP_SUPPORT_RAW
       MMAP_RAW_FILE_FORMAT "true_color_alpha"
       MMAP_RAW_COLOR_FORMAT "RGB565SWAP"
   )

Application Examples
---------------------

Generate Header(mmap_generate_my_spiffs_partition.h)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
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
~~~~~~~~~~~~~~~~~~~~~~~
The assets config ensures consistency with ``mmap_generate_my_spiffs_partition.h``. It sets the ``max_files`` and ``checksum``, verifying the header and memory-mapped binary file.


Partition Mode (Default)
^^^^^^^^^^^^^^^^^^^^^^^^^

.. code:: c

   mmap_assets_handle_t asset_handle;

   const mmap_assets_config_t config = {
      .partition_label = "my_spiffs_partition",
      .max_files = MMAP_MY_FOLDER_FILES, //Get it from the compiled .h
      .checksum = MMAP_MY_FOLDER_CHECKSUM, //Get it from the compiled .h
      .flags = {
         .mmap_enable = false,  // Use partition mode
         .use_fs = false,       // Not using file system
         .app_bin_check = true,
      }
   };

   ESP_ERROR_CHECK(mmap_assets_new(&config, &asset_handle));

Memory Mapping Mode
^^^^^^^^^^^^^^^^^^^^

.. code:: c

   const mmap_assets_config_t config = {
      .partition_label = "my_spiffs_partition",
      .max_files = MMAP_MY_FOLDER_FILES,
      .checksum = MMAP_MY_FOLDER_CHECKSUM,
      .flags = {
         .mmap_enable = true,   // Enable memory mapping
         .use_fs = false,       // Not using file system
         .app_bin_check = true,
      }
   };

   ESP_ERROR_CHECK(mmap_assets_new(&config, &asset_handle));

File System Mode
^^^^^^^^^^^^^^^^^^^^

.. code:: c

   const mmap_assets_config_t config = {
      .partition_label = "/spiffs/assets.bin",  // File path instead of partition name
      .max_files = MMAP_MY_FOLDER_FILES,
      .checksum = MMAP_MY_FOLDER_CHECKSUM,
      .flags = {
         .mmap_enable = false,  // Disable memory mapping
         .use_fs = true,        // Use file system
         .app_bin_check = true,
      }
   };

   ESP_ERROR_CHECK(mmap_assets_new(&config, &asset_handle));

Assets Usage
~~~~~~~~~~~~~~
You can use the enum defined in ``mmap_generate_my_spiffs_partition.h`` to get asset information.

.. code:: c

    const char *name = mmap_assets_get_name(asset_handle, MMAP_JPG_JPG);
    const void *mem = mmap_assets_get_mem(asset_handle, MMAP_JPG_JPG);
    int size = mmap_assets_get_size(asset_handle, MMAP_JPG_JPG);
    int width = mmap_assets_get_width(asset_handle, MMAP_JPG_JPG);
    int height = mmap_assets_get_height(asset_handle, MMAP_JPG_JPG);

    ESP_LOGI(TAG, "Name:[%s], Mem:[%p], Size:[%d bytes], Width:[%d px], Height:[%d px]", name, mem, size, width, height);

API Reference
~~~~~~~~~~~~~~~~~

.. include-build-file:: inc/esp_mmap_assets.inc
