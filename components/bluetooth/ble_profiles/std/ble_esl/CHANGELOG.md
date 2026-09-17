## v1.1.0 - 2026.10.21

Bug Fixes:
- AP: after PAST `set_info`, CONFIGURING/UPDATING becomes SYNCHRONIZED only when the Tag drops the ACL with HCI 0x13. A supervision timeout, 0x3e, or a local disconnect goes to UNSYNCHRONIZED instead of being reported as sync success.
- AP: Update Complete arms its pending mark before the GATT write, because a kept-sync Tag may send HCI 0x13 while `write_by_handle()` is still blocked (ESLP UPD/BV-02, no second PAST). If that write returns an error and the teardown has not already committed SYNCHRONIZED, the mark is cleared. A later 0x13 is not treated as sync success.
- AP: `configure_tag()` returns `ESP_ERR_INVALID_SIZE` when the ATT MTU cannot carry the 24-byte key material, instead of blocking in the key write until timeout.
- AP: PAwR `DATA_REQ` submits only subevents that have a queued command. Idle slots are not sent to HCI.
- AP OTP: OTS is not auto-discovered on `DISC_COMPLETE`. `transfer_image()` discovers it on the Tag connection when needed. Disconnect clears OTS state only for the connection that was discovered, so a phone/NUS drop does not wipe the Tag.
- Tag: Update Complete during Updating terminates any stale periodic sync while retaining ACL, re-arms PAST reception on `SYNC_LOST`, and drops ACL only after the fresh sync receives a PAwR report or its bounded confirmation wait expires. Other already-synchronized paths use the same PAwR-or-timeout ACL teardown guard.
- Tag/AP example: the Update Complete transaction retains its recovery mark through the PAwR confirmation window, re-arms PAST after transfer or sync loss, suppresses connectable advertising while settling, and retries PAST with a bounded timeout instead of leaving the Tag in Updating indefinitely.
- Tag: link loss while associated, or after config writes but before Update Complete, enters Unsynchronized and keeps the materials. Incomplete config goes Unassociated. `SYNC_LOST` clears the Synchronized bit and returns an associated Tag to Unsynchronized.
- Tag: `esp_ble_eslp_restore_association()` publishes flags-only advertising (no ESL UUID) before entering Unsynchronized, so a rebooted Tag is not advertised as Unassociated.

Features:
- AP: `esp_ble_eslp_ap_keeps_sync_without_past()` reports an ACL that was opened while the Tag was already Synchronized.
- AP: `esp_ble_eslp_ap_set_ap_sync_key()` and `esp_ble_eslp_ap_restore_persisted()` reinstall the sync key and a tracked Tag after reboot.
- Tag: `esp_ble_eslp_restore_association()` / `esp_ble_eslp_export_association()` save and reload the association.
- Tag: `esp_ble_eslp_image_snapshot()`, `esp_ble_eslp_image_restore()`, and `esp_ble_eslp_image_invalidate_all()` copy, reload, and clear OTP image slots.

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
