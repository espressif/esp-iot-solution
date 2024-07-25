ESP LV SPNG
=============
:link_to_translation:`zh_CN:[中文]`

Allow the use of PNG images in LVGL. Besides that it also allows the use of a custom format, called Split PNG (SPNG), which can be decoded in more optimal way on embedded systems.

Referencing the implementation of `SJPG <https://docs.lvgl.io/8.4/libs/sjpg.html>`__.

Features
-----------------------

   - Supports both standard PNG and custom SPNG formats.

   - Decoding standard PNG requires RAM equivalent to the full uncompressed image size (recommended for devices with more RAM).

   - SPNG is a custom format based on standard PNG, specifically designed for LVGL.

   - SPNG is a 'split-png' format comprising small PNG fragments and an SPNG header.

   - SPNG images are decoded in segments, so zooming and rotating are not supported.

Converting PNG to SPNG
-----------------------

The `esp_mmap_assets <esp_mmap_assets.html>`__ component is required. It will automatically package and convert PNG images to SPNG format during compilation.

.. code:: c

   [12/1448] Move and Pack assets...
   --support_format: .jpg,.png
   --support_spng: ON
   --support_sjpg: ON
   --split_height: 16
   Input: temp_icon.png    RES: 90 x 90    splits: 6
   Completed, saved as: temp_icon.spng

Application Examples
---------------------

Register Decoder
^^^^^^^^^^^^^^^^^^^

Register the decoder function after LVGL starts.

.. code:: c

   esp_lv_split_png_init();


API Reference
-----------------

.. include-build-file:: inc/esp_lv_spng.inc