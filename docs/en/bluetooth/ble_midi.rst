Bluetooth Low Energy MIDI Profile
===================================
:link_to_translation:`zh_CN:[中文]`

The BLE MIDI profile enables the transmission of MIDI (Musical Instrument Digital Interface)
messages over Bluetooth Low Energy. It is designed for low-latency, low-power communication
between MIDI devices such as musical instruments, controllers, and mobile applications.

BLE MIDI allows devices to exchange standard MIDI messages, including note events,
control changes, and system messages, while benefiting from the reduced power
consumption and flexible connection model provided by Bluetooth Low Energy.

The profile follows the MIDI over BLE specification defined by the MIDI Manufacturers
Association (MMA) and the Bluetooth SIG.

Examples
--------------

:example:`bluetooth/ble_profiles/ble_midi`.

API Reference
-----------------

.. include-build-file:: inc/esp_ble_midi.inc
