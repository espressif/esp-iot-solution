**Network Provisioner**
=======================

:link_to_translation:`zh_CN:[中文]`

``network_provisioner`` is a **controller-side** client for
`espressif/network_provisioning <https://components.espressif.com/components/espressif/network_provisioning>`_.

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Component
     - Role
   * - **network_provisioning**
     - Device (**server**): receives Wi-Fi / Thread credentials
   * - **protocomm_ext**
     - Controller **pipe** (transport + Security 0 / 1 / 2)
   * - **network_provisioner**
     - Controller **business** (``prov-config`` / ``prov-scan`` / ``prov-ctrl``)

It encodes and decodes the provisioning protobufs. Session transport and
security handshake stay in ``protocomm_ext``.

Features
--------

- SoftAP + HTTP: Wi-Fi provisioning
- BLE (NimBLE Central): Wi-Fi and Thread provisioning
- UART Console: debug path
- Runtime capability parsing from ``proto-ver`` (``prov.cap`` / ``prov.sec_ver``)
- High-level ``network_provisioner_provision_wifi()`` / ``provision_thread()``

**Supported targets:** ESP32 / ESP32-S3 / ESP32-C2 / ESP32-C3 / ESP32-C5 / ESP32-C6 / ESP32-C61 / ESP32-H2 / ESP32-P4

Typical Flow
------------

.. code:: c

    #include "network_provisioner.h"
    #include "protocomm_ext.h"

    protocomm_ext_t *pc = protocomm_ext_new(&pe_cfg);
    network_provisioner_t *np = network_provisioner_create(pc);

    network_provisioner_fetch_capabilities(np, /* BLE: connect cfg, else NULL */ NULL);
    network_provisioner_capabilities_t caps = {0};
    network_provisioner_get_capabilities(np, &caps);
    network_provisioner_security_t sec = {
        .sec_ver = caps.sec_ver,
        .pop = "abcd1234",
    };
    network_provisioner_set_security(np, &sec);
    network_provisioner_establish_security(np);

    network_provisioner_wifi_creds_t creds = {
        .ssid = (const uint8_t *)"myssid",
        .ssid_len = 6,
        .passphrase = (const uint8_t *)"mypass",
        .passphrase_len = 6,
    };
    network_provisioner_provision_wifi(np, &creds, NULL);

    network_provisioner_stop_session(np);
    network_provisioner_delete(np);
    protocomm_ext_delete(pc);

If the peer scheme is already known, ``network_provisioner_start_session()``
still covers open → proto-ver → handshake in one call.

A BLE controller can call ``network_provisioner_create_ble()``, then use
``network_provisioner_ble_scan()`` and ``network_provisioner_ble_connect_cfg()``.
Application endpoints use ``network_provisioner_add_endpoint()`` and
``network_provisioner_send()``.

**Thread safety:** APIs on one instance are not thread-safe; serialize from a single task.

Add Dependency
--------------

.. code:: yaml

    dependencies:
      espressif/network_provisioner: "^2.0.1"

Example
-------

See ``examples/network_provisioner``. Pair with the official device examples
``wifi_prov`` / ``thread_prov`` from ``network_provisioning``.

API Reference
-------------

.. include-build-file:: inc/network_provisioner.inc
