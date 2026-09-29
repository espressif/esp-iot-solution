ESP Display Present
===================
:link_to_translation:`zh_CN:[中文]`

``esp_display_present`` connects a renderer-produced pixel surface to an
``esp_lcd`` panel. It does not depend on scene, widget, or input semantics.
The pixel path is abstracted so LVGL, GSP, or a custom GUI can share the same
presenter.

This is not ``esp_lvgl_adapter``. For LVGL 9, use
`ESP LV Present <esp_lv_present.html>`__.

The renderer owns logical damage and pixel generation. The presenter owns
physical buffer coherence and panel submission.

Features
--------

- Validate the display target and select a presentation strategy
- Lease render surfaces or bounded draw buffers
- Submit full-frame or dirty-area updates
- Handle framebuffer ownership, TE synchronization, rotation, byte order,
  cache synchronization, and optional hardware copies
- Track ordered completion and shut down without releasing in-flight buffers

Add to Project
--------------

.. code:: bash

    idf.py add-dependency "espressif/esp_display_present"

Or in ``idf_component.yml``:

.. code:: yaml

    dependencies:
      espressif/esp_display_present: "*"

Public API
----------

Include ``esp_display_present.h``. Configuration and shared types are defined in
``esp_display_present_config.h`` and ``esp_display_present_types.h``.

The primary object is ``esp_display_presenter_t``. A renderer:

1. creates a presenter from an ``esp_display_present_target_config_t``
2. acquires a frame contract
3. renders the requested full surface or partition
4. submits logical coverage, or cancels the frame
5. stops and deletes the presenter after producer work has drained

The draw contract is one of:

===========  ============================================================
Contract     Renderer responsibility
===========  ============================================================
PARTITION    Render requested areas into a presenter-owned draw buffer
DIRECT       Render dirty areas into a coherent full logical surface
FULL         Redraw the complete logical surface
===========  ============================================================

Presentation mode should normally remain ``ESP_DISPLAY_PRESENT_MODE_AUTO``.
Accurate panel class, framebuffer, TE, rotation, and byte-order information in
the target configuration lets the component select the correct path.

When internal RAM allows, PARTIAL and TE modes use a second draw buffer so
tile copies overlap rendering. Set ``drawbuf.buffers = 1`` to keep a single
draw buffer on memory-constrained applications.

Before Display Off or panel teardown, stop frame production and call
``esp_display_presenter_quiesce()``. Keep the panel IO and its completion ISR
alive until quiesce succeeds, then turn the panel off and delete the presenter.

More Details
------------

See the component README:

- `English <https://github.com/espressif/esp-iot-solution/blob/master/components/display/tools/esp_display_present/README.md>`__
- `中文 <https://github.com/espressif/esp-iot-solution/blob/master/components/display/tools/esp_display_present/README_CN.md>`__

The LVGL benchmark example lives in
``examples/display/gui/lvgl_present_benchmark``. The GSP/LVGL ownership
handoff example lives in ``examples/display/gui/gsp_lvgl_present_handoff``.
The Unity test app lives in
``components/display/tools/esp_display_present/test_apps``.

API Reference
-------------

.. include-build-file:: inc/esp_display_present.inc
.. include-build-file:: inc/esp_display_present_config.inc
.. include-build-file:: inc/esp_display_present_types.inc
.. include-build-file:: inc/esp_display_present_geometry.inc
