ESP LV Present
==============
:link_to_translation:`zh_CN:[中文]`

``esp_lv_present`` is the LVGL 9 flush binding for
``esp_display_present``. The application owns the ``esp_lcd`` panel and the
presenter; this component only attaches an LVGL display to that presenter.

It is not a second display stack. ``display`` stays in
``esp_display_present``; this name marks the LVGL side of that presenter.

Requirements
------------

- ESP-IDF >= 6.0
- LVGL 9.x only (not LVGL 8)
- An ``esp_display_presenter_t`` created by the application

Relationship
------------

::

    LVGL 9 widgets
          │
    esp_lv_present           (this component, LVGL 9 only)
          │
    esp_display_present      (buffers, TE, rotation, panel submit)
          │
    esp_lcd panel

vs ``esp_lvgl_adapter``
-----------------------

``esp_lvgl_adapter`` is a full LVGL port (v8 and v9): display registration,
worker task, lock, touch/encoder, and optional FS / decoder / FreeType. Use it
when the UI does not go through ``esp_display_present``.

``esp_lv_present`` only installs the LVGL 9 flush path on an existing
presenter. The start-task must pump ``lv_timer_handler()``. Do not drive the
same panel with both components at once.

Pair with the presenter
-----------------------

1. Initialize the panel (for example with ``hw_init``).
2. Create a presenter (``ESP_DISPLAY_PRESENT_MODE_AUTO`` is usually enough).
3. Call ``esp_lv_present_start()`` from the task that will pump LVGL.
4. Before handing the presenter to another producer: quiesce, then
   ``esp_lv_present_stop()``. The presenter is not deleted.

.. code:: yaml

    dependencies:
      espressif/esp_lv_present: "^0.1.0"

.. code:: c

   #include "esp_lv_present.h"

   lv_display_t *display = NULL;
   ESP_ERROR_CHECK(esp_lv_present_start(presenter, &display));

More Details
------------

See the component README:

- `English <https://github.com/espressif/esp-iot-solution/blob/master/components/display/tools/esp_lv_present/README.md>`__
- `中文 <https://github.com/espressif/esp-iot-solution/blob/master/components/display/tools/esp_lv_present/README_CN.md>`__

Examples: ``examples/display/gui/lvgl_present_benchmark``,
``examples/display/gui/gsp_lvgl_present_handoff``.
Test app: ``components/display/tools/esp_lv_present/test_apps``.

See also: `ESP Display Present <esp_display_present.html>`__,
`ESP LVGL Adapter <esp_lvgl_adapter.html>`__.

API Reference
-------------

.. include-build-file:: inc/esp_lv_present.inc
