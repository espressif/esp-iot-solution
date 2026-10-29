BTHome Component
==============================
:link_to_translation:`zh_CN:[中文]`

The BTHome component implements the BTHome V2 protocol, supporting sensor data reporting, binary sensor data reporting, and event data reporting.
This component supports both encrypted and non-encrypted modes, and can be seamlessly integrated with smart home platforms like Home Assistant.

BTHome Usage
-----------------
1. Initialize BTHome:

   - Use :cpp:func:`bthome_create` to create a BTHome instance
   - Use :cpp:func:`bthome_register_callbacks` to register callback functions
   - Use :cpp:func:`bthome_set_encrypt_key` to set encryption key (optional)
   - Use :cpp:func:`bthome_set_peer_mac_addr` to set peer MAC address

2. Configuration Storage:

   - Use :cpp:func:`settings_store` to store configuration
   - Use :cpp:func:`settings_load` to load configuration

3. Parse Advertising Data:

   - Use :cpp:func:`bthome_parse_adv_data` to parse advertising data
   - Use :cpp:func:`bthome_free_reports` to free report data

Example Code
-----------------
.. code-block:: c

    // Create BTHome instance
    bthome_handle_t bthome_recv;
    ESP_ERROR_CHECK(bthome_create(&bthome_recv));

    // Register callback functions
    bthome_callbacks_t callbacks = {
        .store = settings_store,
        .load = settings_load,
    };
    ESP_ERROR_CHECK(bthome_register_callbacks(bthome_recv, &callbacks));

    // Set encryption key
    static const uint8_t encrypt_key[] = {0x23, 0x1d, 0x39, 0xc1, 0xd7, 0xcc, 0x1a, 0xb1, 0xae, 0xe2, 0x24, 0xcd, 0x09, 0x6d, 0xb9, 0x32};
    ESP_ERROR_CHECK(bthome_set_encrypt_key(bthome_recv, encrypt_key));

    // Set peer MAC address
    static const uint8_t peer_mac[] = {0x54, 0x48, 0xE6, 0x8F, 0x80, 0xA5};
    ESP_ERROR_CHECK(bthome_set_peer_mac_addr(bthome_recv, peer_mac));

    // Parse advertising data
    bthome_reports_t *reports = bthome_parse_adv_data(bthome_recv, result.ble_adv, result.adv_data_len);
    if (reports != NULL) {
        // Process report data
        for (int i = 0; i < reports->num_reports; i++) {
            // Process each report
        }
        bthome_free_reports(reports);
    }

API Reference
---------------------------------------------
.. include-build-file:: inc/bthome_v2.inc 