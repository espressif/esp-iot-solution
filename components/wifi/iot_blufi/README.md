# Component: iot_blufi

this components was designed for easily use of Blufi. User should enable bluetooth in menuconfig before using this component. The mainly calling sequence is shown below:

1. Initialize nvs and wifi outwards.
2. Call iot_blufi_start() with given arguments to start Blufi. The argument `release_classic_bt` means whether or not release classic bt memory before ble start; the argument `ticks_to_wait` means wait given ticks to start blufi. If ticks_to_wait is zero, the API will return immediately, user should call iot_blufi_get_status() to check the status of Blufi; else the API will block until wait ticks expired or start Blufi failed.
3. Call iot_blufi_stop() with given argument `release_ble` to stop Blufi when network configuration process succeed or timeout. The argument `release_ble` means whether or not release ble memory after ble stop.
