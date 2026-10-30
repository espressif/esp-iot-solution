**Protocomm Extension**
=======================

:link_to_translation:`zh_CN:[中文]`

``protocomm_ext`` is a **client-side** extension of ESP-IDF
`protocomm <https://docs.espressif.com/projects/esp-idf/en/latest/api-reference/provisioning/protocomm.html>`_.

====================  =====================================================
 Component             Role
====================  =====================================================
 **protocomm** (IDF)   Device (**server**): accepts provisioning / local-control sessions
 **protocomm_ext**     Controller (**client**): opens sessions, runs security handshake, exchanges payloads
====================  =====================================================

It is a **protocol pipe only**. It does **not** encode Wi-Fi provisioning or
``esp_local_ctrl`` business protobufs. Higher-level components should call
``protocomm_ext_send_data()`` with already marshalled endpoint payloads.

Features
--------

- **Security 0**: plaintext session
- **Security 1**: Curve25519 + AES-256-CTR (optional Proof-of-Possession)
- **Security 2**: SRP-6a (NG_3072 / SHA512) + AES-256-GCM (username / password)
- Transports: HTTP / HTTPS, BLE (NimBLE Central), UART Console
- Kconfig options to strip unused security versions

**Supported targets:** ESP32 / ESP32-S3 / ESP32-C2 / ESP32-C3 / ESP32-C5 / ESP32-C6 / ESP32-C61 / ESP32-P4

Typical Flow
------------

.. code:: c

    #include "protocomm_ext.h"
    #include "protocomm_ext_http.h"
    #include "protocomm_ext_security.h"

    esp_http_client_config_t http_cfg = {
        .url = "http://192.168.4.1:80",
        .timeout_ms = 5000,
    };

    protocomm_ext_security1_params_t pop = {
        .data = (const uint8_t *)"abcd1234",
        .len = 8,
    };

    protocomm_ext_config_data_t cfg = {
        .transport_method = PROTOCOMM_EXT_TRANSPORT_METHOD_HTTP,
        .security_method = PROTOCOMM_EXT_SECURITY_METHOD_SECURITY_1,
        .transport_data = &http_cfg,
        .security_data = &pop,
    };

    protocomm_ext_t *pc = protocomm_ext_new(&cfg);
    protocomm_ext_open_session(pc, NULL);
    protocomm_ext_establish_security(pc, "prov-session");

    uint8_t *resp = NULL;
    size_t resp_len = 0;
    protocomm_ext_send_data(pc, "prov-config", req, req_len, &resp, &resp_len);
    free(resp);

    protocomm_ext_close_session(pc);
    protocomm_ext_delete(pc);

**Sessions:** one instance is one peer. HTTP keeps that instance's connection, TCP keepalive, and cookie. Create one instance per device. BLE remains a single connection and needs the NimBLE central, observer, and GATT client roles.

Add Dependency
--------------

.. code:: yaml

    dependencies:
      espressif/protocomm_ext: "^1.1.0"

API Reference
-------------

.. include-build-file:: inc/protocomm_ext.inc

.. include-build-file:: inc/protocomm_ext_security.inc

.. include-build-file:: inc/protocomm_ext_http.inc

.. include-build-file:: inc/protocomm_ext_nimble.inc

.. include-build-file:: inc/protocomm_ext_console.inc
