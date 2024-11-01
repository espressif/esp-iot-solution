ESP LV DECODER
====================
:link_to_translation:`zh_CN:[中文]`

Allow the use of images in LVGL. Besides that it also allows the use of a custom format, called Split image, which can be decoded in more optimal way on embedded systems.

Referencing the implementation of `SJPG <https://docs.lvgl.io/8.4/libs/sjpg.html>`__.

Features
-----------------------

   - Supports both standard and custom split image formats, including JPG, PNG, and `QOI <https://github.com/phoboslab/qoi>`__.

   - Decoding standard image requires RAM equivalent to the full uncompressed image size (recommended for devices with more RAM).

   - Split image is a custom format based on standard image formats, specifically designed for LVGL.

   - File reads are implemented for both file storage and C-arrays.

   - Split images are decoded in segments, so zooming and rotating are not supported.

Usage
-----------------------

The `esp_mmap_assets <esp_mmap_assets.html>`__ component is required. It automatically packages and converts images to your desired format during compilation.

Converting JPG to SJPG
^^^^^^^^^^^^^^^^^^^^^^^^^
.. code:: c

   spiffs_create_partition_assets(
        my_spiffs_partition
        my_folder
        FLASH_IN_PROJECT
        MMAP_FILE_SUPPORT_FORMAT ".jpg"
        MMAP_SUPPORT_SJPG
        MMAP_SPLIT_HEIGHT 16)

Converting PNG to SPNG
^^^^^^^^^^^^^^^^^^^^^^^^^
.. code:: c

   spiffs_create_partition_assets(
        my_spiffs_partition
        my_folder
        FLASH_IN_PROJECT
        MMAP_FILE_SUPPORT_FORMAT ".png"
        MMAP_SUPPORT_SPNG
        MMAP_SPLIT_HEIGHT 16)

Converting PNG、JPG to QOI
^^^^^^^^^^^^^^^^^^^^^^^^^^^^
.. code:: c

   spiffs_create_partition_assets(
        my_spiffs_partition
        my_folder
        FLASH_IN_PROJECT
        MMAP_FILE_SUPPORT_FORMAT ".jpg,.png"
        MMAP_SUPPORT_QOI)

Converting PNG、JPG to SQOI
^^^^^^^^^^^^^^^^^^^^^^^^^^^^
.. code:: c

   spiffs_create_partition_assets(
        my_spiffs_partition
        my_folder
        FLASH_IN_PROJECT
        MMAP_FILE_SUPPORT_FORMAT ".jpg,.png"
        MMAP_SUPPORT_QOI
        MMAP_SUPPORT_SQOI
        MMAP_SPLIT_HEIGHT 16)

Application Examples
---------------------

Register Decoder
^^^^^^^^^^^^^^^^^^^

Register the decoder function after LVGL starts.

.. code:: c

   esp_lv_decoder_handle_t decoder_handle = NULL;
   esp_lv_decoder_init(&decoder_handle); //Initialize this after lvgl starts


API Reference
-----------------

.. include-build-file:: inc/esp_lv_decoder.inc