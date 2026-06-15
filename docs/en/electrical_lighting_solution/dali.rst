Digital Addressable Lighting Interface (DALI) Bus Driver
=========================================================

:link_to_translation:`zh_CN:[中文]`

The DALI component provides an ESP-IDF based DALI (IEC 62386) master driver.
It uses the ESP RMT peripheral to generate :term:`forward frames <Forward Frame (FF)>`
and decode :term:`backward frames <Backward Frame (BF)>`, so applications can
control and query DALI control gear directly.

Features
--------

* Physical Layer — RMT-based Manchester TX/RX (Te = 416.67 µs, ±10 % tolerance).
* Addressing — Short address (0–63), group address (0–15), broadcast, and special commands.
* Part 102 (Control Gear) — DAPC dimming, indirect/configuration commands, and queries for lamps.
* Part 103 (Control Device) — Event reporting and device/instance-level commands for input devices.
* Part 303/304 (Sensors) — Occupancy (303) and light (304) sensor support.
* Part 209 (DT8 Color) — RGB, CCT (Tc), and XY chromaticity control.
* Commissioning — Automatic short address assignment for Part 102 and Part 103 devices.
* Send-twice — Built-in double transmission within 100 ms for configuration commands.

Supported Targets
-----------------

ESP32 series chips supported by the component metadata:

* ESP32
* ESP32-S2
* ESP32-S3
* ESP32-C3
* ESP32-C6
* ESP32-P4
* ESP32-H2

Quick Start
-----------

1. Include headers:

   .. code-block:: c

      #include "dali.h"
      #include "dali_command.h"

2. Initialize the driver:

   .. code-block:: c

      dali_master_handle_t dali;
      dali_master_config_t cfg = {
          .rx_gpio = GPIO_NUM_4,
          .tx_gpio = GPIO_NUM_5,
          .invert_tx = false,
          .invert_rx = false,
      };
      dali_master_rmt_config_t rmt_cfg = {
          .mem_block_symbols = 64,
      };
      ESP_ERROR_CHECK(dali_new_master_rmt(&cfg, &rmt_cfg, &dali));

3. Send a command (no backward frame expected):

   .. code-block:: c

      /* The driver automatically inserts the minimum inter-frame gap
         (> 22 Te) after every transaction — no explicit delay needed. */
      dali_master_transaction_config_t tx_cfg = {
          .addr_type = DALI_ADDR_SHORT,
          .addr = 0,
          .is_cmd = true,
          .command = DALI_CMD_RECALL_MAX_LEVEL,
          .send_twice = false,
          .tx_timeout_ms = DALI_TX_TIMEOUT_MS,
      };
      ESP_ERROR_CHECK(dali_master_do_transaction(dali, &tx_cfg, NULL));

4. Send a query (backward frame expected):

   .. code-block:: c

      int reply = DALI_RESULT_NO_REPLY;
      dali_master_transaction_config_t tx_cfg = {
          .addr_type = DALI_ADDR_SHORT,
          .addr = 0,
          .is_cmd = true,
          .command = DALI_CMD_QUERY_STATUS,
          .send_twice = false,
          .tx_timeout_ms = DALI_TX_TIMEOUT_MS,
      };
      ESP_ERROR_CHECK(dali_master_do_transaction(dali, &tx_cfg, &reply));
      if (DALI_RESULT_IS_VALID(reply)) {
          ESP_LOGI("dali", "QUERY_STATUS = 0x%02X", (unsigned)reply);
      }

5. Commissioning (auto-assign short addresses):

   .. code-block:: c

      /* Part 102 — Control Gear (lamps) */
      uint8_t count102 = 0;
      esp_err_t err = dali_commission(dali, DALI_COMMISSION_ALL, 0, 64, &count102,
                                      DALI_TX_TIMEOUT_MS);
      if (err == ESP_OK) {
          ESP_LOGI("dali", "Part 102 commissioned: %u gear", count102);
      }

      /* Part 103 — Control Devices (sensors) */
      uint8_t count103 = 0;
      err = dali_103_commission(dali, DALI_COMMISSION_ALL, 10, 64, &count103,
                                DALI_TX_TIMEOUT_MS);
      if (err == ESP_OK) {
          ESP_LOGI("dali", "Part 103 commissioned: %u devices", count103);
      }

6. Set DT8 color (Part 209):

   .. code-block:: c

      /* RGB mode — set address 4 to red */
      dali_color_val_t color = { .rgb = { .r = 254, .g = 0, .b = 0 } };
      ESP_ERROR_CHECK(dali_master_set_color(dali, DALI_ADDR_SHORT, 4,
                                             DALI_COLOR_RGB, color,
                                             DALI_TX_TIMEOUT_MS));

      /* CCT mode — set address 4 to 2700 K (370 Mirek) */
      dali_color_val_t cct = { .cct = { .mirek = 370 } };
      ESP_ERROR_CHECK(dali_master_set_color(dali, DALI_ADDR_SHORT, 4,
                                             DALI_COLOR_CCT, cct,
                                             DALI_TX_TIMEOUT_MS));

Configuration
-------------

All DALI driver configuration is done through the two configuration structures
at runtime — no Kconfig options are required:

* :cpp:type:`dali_master_config_t` — GPIO pin assignment (``rx_gpio``, ``tx_gpio``) and polarity inversion (``invert_tx``, ``invert_rx``).
* :cpp:type:`dali_master_rmt_config_t` — RMT-specific settings such as memory block size.

.. code-block:: c

   dali_master_config_t cfg = {
       .rx_gpio = GPIO_NUM_4,
       .tx_gpio = GPIO_NUM_5,
       .invert_tx = false,      /* By default, TX polarity is not inverted */
       .invert_rx = false,      /* By default, RX polarity is not inverted */
   };
   dali_master_rmt_config_t rmt_cfg = {
       .mem_block_symbols = 0, /* 0 = auto-detect per SOC capability */
   };
   ESP_ERROR_CHECK(dali_new_master_rmt(&cfg, &rmt_cfg, &dali));

Command Model
-------------

Use ``dali_master_do_transaction()`` as the single entry for all transaction types.
Pass a :cpp:type:`dali_master_transaction_config_t` to describe the transaction:

* DAPC value write: ``config.is_cmd = false``, ``config.command`` is arc power value.
* Normal command/query: ``config.is_cmd = true``, ``config.command`` from ``dali_command.h``.
* Commands requiring double transmission: set ``config.send_twice = true``.
* For queries, pass ``result != NULL`` and check ``DALI_RESULT_IS_VALID(*result)``.
  ``dali_master_do_transaction()`` automatically inserts the required inter-frame gap
  (> 22 :term:`Te`) after every transaction, so no manual delay is needed between
  consecutive calls.

Timing Notes
------------

* DALI defines strict frame spacing and backward-frame response windows.
* The driver automatically inserts the minimum inter-frame gap (> 22 Te) after
  every ``dali_master_do_transaction()`` call, satisfying the IEC 62386 requirement.
* ``dali_master_do_transaction()`` is blocking — do not call it from an ISR or a
  time-critical task.

Example and Test
----------------

* Example application: :example:`lighting/dali_basic`
* Component test app: ``components/dali/test_apps/main/dali_test.c``

The example demonstrates:

* Commissioning — Automatic short address assignment for Part 102 (lamps) and Part 103 (sensors)
* Dynamic Device Detection — Scan and identify DT6 (dimming) vs DT8 (color) gear by device type
* DAPC Dimming — Brightness control sequence on all discovered lamps
* Part 103 Sensor — Occupancy polling and event-triggered actions
* Simultaneous Blink — When occupied: DT6 lamps blink together, DT8 lamp alternates Red/Blue
* Query Commands — Status, actual level, and device type queries

API Reference
-------------

.. include-build-file:: inc/dali.inc

Glossary
--------

.. glossary::

   Te
      The DALI half-period unit. Nominal value: 416.67 µs (±10 % tolerance
      allowed by IEC 62386). All DALI timing is expressed as multiples of Te.

   Forward Frame (FF)
      A 16-bit frame transmitted by the DALI master to control gear. It
      consists of 1 start bit + 16 data bits + 2 stop bits = 38 Te total.
      The first byte encodes the address; the second byte carries the command
      or arc-power value.

   Backward Frame (BF)
      An 8-bit reply frame sent by a DALI slave in response to a query
      command. It consists of 1 start bit + 8 data bits + 2 stop bits =
      22 Te total. The slave must respond within 7 Te-22 Te after the
      forward frame ends.

   Short Address
      A unique address assigned to a single DALI control gear, in the range
      0-63. Encoded in the forward frame as ``0AAAAAAS`` (A = address bits,
      S = selector bit).
      Short addresses for Part 102 and Part 103 devices are independent;
      no address conflicts during commissioning.

   Group Address
      An address shared by up to 16 control gear units, in the range 0-15.
      Encoded as ``100AAAAS``. Allows simultaneous control of multiple
      fixtures without individual addressing.
