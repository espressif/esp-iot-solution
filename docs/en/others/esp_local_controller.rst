**ESP Local Controller**
========================

:link_to_translation:`zh_CN:[中文]`

``esp_local_controller`` is a **controller-side** client for ESP-IDF
`esp_local_ctrl <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/esp_local_ctrl.html>`_.

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Component
     - Role
   * - **esp_local_ctrl**
     - Device (**server**): exposes properties
   * - **protocomm_ext**
     - Controller **pipe** (transport + Security 0 / 1 / 2)
   * - **esp_local_controller**
     - Controller **business** (``esp_local_ctrl/control`` protobuf)

It encodes and decodes the local-control protobufs. Session transport and
security handshake stay in ``protocomm_ext``.

Features
--------

- HTTP, BLE (NimBLE Central), UART Console
- Runtime version JSON parsing (``local_ctrl.sec_ver`` / ``sec_patch_ver``)
- Property count / get / set by index
- Security 0 / 1 / 2 via ``protocomm_ext``

**Supported targets:** ESP32 / ESP32-S3 / ESP32-C2 / ESP32-C3 / ESP32-C5 / ESP32-C6 / ESP32-C61 / ESP32-H2 / ESP32-P4

Typical Flow
------------

.. code:: c

    #include "esp_local_controller.h"
    #include "protocomm_ext.h"
    #include "protocomm_ext_security.h"

    protocomm_ext_t *pc = protocomm_ext_new(&pe_cfg); /* transport + Sec0 placeholder */
    esp_local_controller_t *ctrl = esp_local_controller_create(pc);

    esp_local_controller_fetch_version(ctrl, /* BLE: &ble_addr, else NULL */ NULL);
    esp_local_controller_version_t ver = {0};
    esp_local_controller_get_version(ctrl, &ver);
    /* protocomm_ext_set_security(pc, ...) from ver.sec_ver + PoP / Sec2 creds */
    esp_local_controller_version_free(&ver);

    esp_local_controller_establish_security(ctrl);

    uint32_t count = 0;
    esp_local_controller_get_property_count(ctrl, &count);

    esp_local_controller_stop_session(ctrl);
    esp_local_controller_delete(ctrl);
    protocomm_ext_delete(pc);

If the peer scheme is already known, call ``protocomm_ext_set_security()`` first,
then ``esp_local_controller_start_session()``.

**Sessions:** one instance is one device. Create one per peer to keep several HTTP sessions open at the same time. One device still accepts only one HTTP client.

**Thread safety:** APIs on one instance are not thread-safe; serialize from a single task. Different instances may be used one after another from that task.

Add Dependency
--------------

.. code:: yaml

    dependencies:
      espressif/esp_local_controller: "^1.1.0"

Example
-------

See ``examples/esp_local_controller``. Pair with the official IDF example
``examples/protocols/esp_local_ctrl``.

API Reference
-------------

.. include-build-file:: inc/esp_local_controller.inc
