## [Unreleased]

## [0.1.0] - 2026-01-06

BLE MIDI (Bluetooth Low Energy MIDI) is a specification defined by the MIDI Association that allows MIDI messages to be transmitted over Bluetooth Low Energy.

It enables low-latency, wireless communication between MIDI devices such as controllers, synthesizers, and mobile or desktop applications.

BLE MIDI GATT Definition
- Service UUID (03B80E5A-EDE8-4B33-A751-6CE34EC4C700)
- Characteristic UUID (MIDI I/O) (7772E5DB-3868-4112-A1A9-F2669D106BF3)

The MIDI characteristic supports both Notify and Write Without Response, allowing bidirectional transmission of MIDI data encapsulated in BLE MIDI packets.

Features:
- BLE-MIDI: Bluetooth Low Energy MIDI Profile.
