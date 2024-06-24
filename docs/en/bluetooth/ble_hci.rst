BLE HCI Components
==============================
:link_to_translation:`zh_CN:[中文]`

The BLE HCI component is used to directly operate the BLE Controller via the VHCI interface to achieve functionalities like broadcasting and scanning. Compared to initiating broadcasting and scanning through the Nimble or Bluedroid protocol stacks, using this component offers the following advantages:

- Lower memory usage
- Smaller firmware size
- Faster initialization process

How to Use BLE HCI
----------------------------

For Broadcasting Applications:
1.Initialize BLE HCI: Use :cpp:func:`ble_hci_init`  function to initialize.
2.Set Local Random Address (Optional): If you need to use a random address for broadcasting, set it using the :cpp:func:`ble_hci_set_random_address` function.
3.Configure Broadcast Parameters: Use :cpp:func:`ble_hci_set_adv_param` function to configure broadcast parameters.
4.Set Broadcast Data: Use :cpp:func:`ble_hci_set_adv_data` function to specify the data to be broadcast.
5.Start Broadcasting: Use :cpp:func:`ble_hci_set_adv_enable` function.

For Scanning Applications:
1.Initialize BLE HCI: Use :cpp:func:`ble_hci_init` function to initialize.
2.Configure Scanning Parameters: Use :cpp:func:`ble_hci_set_scan_param` function to configure scanning parameters.
3.Enable Meta Event: Use :cpp:func:`ble_hci_enable_meta_event` function to enable interrupt events.
4.Register Scanning Event Function: Use :cpp:func:`ble_hci_set_register_scan_callback` function to register the interrupt event.
5.Start Scanning: Use the :cpp:func:`ble_hci_set_scan_enable` function.


API Reference
-----------------

.. include-build-file:: inc/ble_hci.inc