## v1.0.0 - 2026.07.28

Features:
- Tag role: GATT association, lifecycle ECP handling, optional PAST/PAwR synchronized state
- Tag role: Instantiate Device Information Service for vendor-specific opcode support
- Tag role: OTP Object Server for ESL image slots (`CONFIG_BLE_ESL_OTS_SUPPORT`)
- Tag role: Default Display / LED / Sensor ECP+PAwR handling with HW events and timed pending
- Tag role: Configuring link loss after config writes keeps materials and enters Unsynchronized
- Tag role: Service Reset optional hold callback via `esp_ble_eslp_register_service_needed_hold()`
- Tag role: `esp_ble_eslp_set_service_needed()`
- Shared ESL lifecycle state machine (`esp_eslp_state`) with Unsynchronized + 60-min timeouts
- AP role: Tag association helper, PAwR control, PAST set_info, subevent command queue
- AP role: OTP Object Client with `esp_ble_eslp_ap_transfer_image()` and Truncate write mode
- AP role: Remote Tag lifecycle tracking; Synchronized after decryptable PAwR response
- AP role: `esp_ble_eslp_ap_read_info()` for Display/Image/Sensor/LED/PnP
- AP role: Typed ECP/PAwR command helpers with 30 s ECP procedure timeout
- AP role: `esp_ble_eslp_ap_configure_tag()` requires an encrypted link
- EAD for Synchronized PAwR; AES-CCM Encrypted Data AD with AP Sync Key / Response Key
- Events on `BLE_ESLP_EVENTS` and `BLE_ESLP_AP_EVENTS`
- ESL Service: require `set_display_info` before `esp_ble_esl_init` when Display Info is enabled
- ESL Service: Absolute Time base and set timestamp protected by critical section
