| Supported Targets | ESP32-C2 | ESP32-C3 |
| ----------------- | ----- | -------- | 

# Relinker example

## Overview

This example demonstrates how to use relinker by the [cmake_utilities](https://components.espressif.com/components/espressif/cmake_utilities) component.

## How to use the example

The example is a copy of [bleprph_wifi_coex](https://github.com/espressif/esp-idf/tree/master/examples/bluetooth/nimble/bleprph_wifi_coex). Please refer the [README.md](https://github.com/espressif/esp-idf/tree/master/examples/bluetooth/nimble/bleprph_wifi_coex/README.md) for setup details.

Before project configuration and build, be sure to set the correct chip target using:

```bash
idf.py set-target <chip_name>
```

### Configure the project

Open the project configuration menu:

```bash
idf.py menuconfig
```

In the `Example Configuration` menu:

* Enter SSID and password of known Wi-Fi AP with connectivity to internet.
* Enter desired ping IP Address. Default is set to `93.184.216.34` ( This is the IP address of https://example.com ).

* Enter other related parameters like count of ping and maximum numbers of retry.

## Testing

To test this demo, any BLE scanner app and a WiFi access point with internet connectivity can be used. Please refer the [README.md](https://github.com/espressif/esp-idf/tree/master/examples/bluetooth/nimble/bleprph_wifi_coex/README.md) for setup details.

### Build and Flash

Run `idf.py -p PORT flash monitor` to build, flash and monitor the project.

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://idf.espressif.com/) for full steps to configure and use ESP-IDF to build projects.

## Example Output

Compile log:
```
$ idf.py set-target <target>
-- Relinker is enabled.
-- Relinker configuration files: /path-to/esp-iot-solution/tools/cmake_utilities/scripts/relinker/examples/iram_strip/esp32c2/5.3
-- Configuring done (3.7s)
-- Generating done (0.3s)
-- Build files have been written to: /path-to/esp-iot-solution/examples/relinker/build

$ idf.py build
[989/993] Generating esp-idf/esp_system/ld/customer_sections.ld
paths: ['/home/infiniteyuan/espressif/espressif_gitlab/esp-idf/esp-idf-v5.3/components/bt/controller/lib_esp32c2/esp32c2-bt-lib/libble_app.a']
paths: ['/home/infiniteyuan/espressif/espressif_gitlab/esp-idf/esp-idf-v5.3/components/bt/controller/lib_esp32c2/esp32c2-bt-lib/libble_app.a']
paths: ['/home/infiniteyuan/espressif/espressif_gitlab/esp-idf/esp-idf-v5.3/components/bt/controller/lib_esp32c2/esp32c2-bt-lib/libble_app.a']
paths: ['/home/infiniteyuan/espressif/espressif_gitlab/esp-idf/esp-idf-v5.3/components/bt/controller/lib_esp32c2/esp32c2-bt-lib/libble_app.a']
paths: ['/home/infiniteyuan/espressif/espressif_gitlab/esp-idf/esp-idf-v5.3/components/bt/controller/lib_esp32c2/esp32c2-bt-lib/libble_app.a']
paths: ['/home/infiniteyuan/espressif/espressif_gitlab/esp-idf/esp-idf-v5.3/components/bt/controller/lib_esp32c2/esp32c2-bt-lib/libble_app.a']
paths: ['/home/infiniteyuan/espressif/espressif_gitlab/esp-idf/esp-idf-v5.3/components/bt/controller/lib_esp32c2/esp32c2-bt-lib/libble_app.a']
paths: ['/home/infiniteyuan/espressif/espressif_gitlab/esp-idf/esp-idf-v5.3/components/bt/controller/lib_esp32c2/esp32c2-bt-lib/libble_app.a']
paths: ['/home/infiniteyuan/espressif/espressif_gitlab/esp-idf/esp-idf-v5.3/components/bt/controller/lib_esp32c2/esp32c2-bt-lib/libble_app.a']
paths: ['/home/infiniteyuan/espressif/espressif_gitlab/esp-idf/esp-idf-v5.3/components/bt/controller/lib_esp32c2/esp32c2-bt-lib/libble_app.a']
paths: ['/home/infiniteyuan/espressif/espressif_gitlab/esp-idf/esp-idf-v5.3/components/bt/controller/lib_esp32c2/esp32c2-bt-lib/libble_app.a']
paths: ['/home/infiniteyuan/espressif/espressif_gitlab/esp-idf/esp-idf-v5.3/components/bt/controller/lib_esp32c2/esp32c2-bt-lib/libble_app.a']
paths: ['esp-idf/bootloader_support/CMakeFiles/__idf_bootloader_support.dir/bootloader_flash/src/bootloader_flash.c.obj']
paths: ['esp-idf/bootloader_support/CMakeFiles/__idf_bootloader_support.dir/src/flash_encrypt.c.obj']
paths: ['esp-idf/bt/CMakeFiles/__idf_bt.dir/porting/mem/bt_osi_mem.c.obj']
paths: ['esp-idf/bt/CMakeFiles/__idf_bt.dir/controller/esp32c2/bt.c.obj']
paths: ['esp-idf/bt/CMakeFiles/__idf_bt.dir/host/nimble/nimble/porting/nimble/src/nimble_port.c.obj']
skip os_callout_timer_cb(CONFIG_BT_ENABLED && !CONFIG_BT_NIMBLE_USE_ESP_TIMER)
paths: ['esp-idf/bt/CMakeFiles/__idf_bt.dir/porting/npl/freertos/src/npl_os_freertos.c.obj']
paths: ['esp-idf/esp_driver_gpio/CMakeFiles/__idf_esp_driver_gpio.dir/src/gpio.c.obj']
paths: ['esp-idf/esp_app_format/CMakeFiles/__idf_esp_app_format.dir/esp_app_desc.c.obj']
paths: ['esp-idf/esp_hw_support/CMakeFiles/__idf_esp_hw_support.dir/cpu.c.obj']
paths: ['esp-idf/esp_hw_support/CMakeFiles/__idf_esp_hw_support.dir/esp_clk.c.obj']
paths: ['esp-idf/esp_hw_support/CMakeFiles/__idf_esp_hw_support.dir/esp_memory_utils.c.obj']
paths: ['esp-idf/esp_hw_support/CMakeFiles/__idf_esp_hw_support.dir/hw_random.c.obj']
paths: ['esp-idf/esp_hw_support/CMakeFiles/__idf_esp_hw_support.dir/intr_alloc.c.obj']
paths: ['esp-idf/esp_hw_support/CMakeFiles/__idf_esp_hw_support.dir/periph_ctrl.c.obj']
paths: ['esp-idf/esp_hw_support/CMakeFiles/__idf_esp_hw_support.dir/regi2c_ctrl.c.obj']
paths: ['esp-idf/esp_phy/CMakeFiles/__idf_esp_phy.dir/src/phy_init.c.obj']
paths: ['esp-idf/esp_phy/CMakeFiles/__idf_esp_phy.dir/src/phy_override.c.obj']
paths: ['esp-idf/esp_pm/CMakeFiles/__idf_esp_pm.dir/pm_locks.c.obj']
paths: ['esp-idf/esp_pm/CMakeFiles/__idf_esp_pm.dir/pm_impl.c.obj']
skip vApplicationSleep(CONFIG_FREERTOS_USE_TICKLESS_IDLE)
paths: ['esp-idf/esp_ringbuf/CMakeFiles/__idf_esp_ringbuf.dir/ringbuf.c.obj']
paths: ['esp-idf/esp_system/CMakeFiles/__idf_esp_system.dir/port/brownout.c.obj']
paths: ['esp-idf/esp_system/CMakeFiles/__idf_esp_system.dir/port/soc/esp32c2/cache_err_int.c.obj']
paths: ['esp-idf/esp_system/CMakeFiles/__idf_esp_system.dir/port/cpu_start.c.obj']
paths: ['esp-idf/esp_system/CMakeFiles/__idf_esp_system.dir/crosscore_int.c.obj']
paths: ['esp-idf/esp_system/CMakeFiles/__idf_esp_system.dir/esp_system.c.obj']
paths: ['esp-idf/esp_system/CMakeFiles/__idf_esp_system.dir/port/esp_system_chip.c.obj']
paths: ['esp-idf/esp_system/CMakeFiles/__idf_esp_system.dir/port/soc/esp32c2/reset_reason.c.obj']
paths: ['esp-idf/esp_system/CMakeFiles/__idf_esp_system.dir/ubsan.c.obj']
paths: ['esp-idf/esp_timer/CMakeFiles/__idf_esp_timer.dir/src/esp_timer.c.obj']
skip timer_list_unlock(!CONFIG_ESP_TIMER_SUPPORTS_ISR_DISPATCH_METHOD)
skip timer_list_lock(!CONFIG_ESP_TIMER_SUPPORTS_ISR_DISPATCH_METHOD)
skip timer_insert(!CONFIG_ESP_TIMER_SUPPORTS_ISR_DISPATCH_METHOD)
skip esp_timer_impl_get_time(!CONFIG_ESP_TIMER_SUPPORTS_ISR_DISPATCH_METHOD)
skip esp_timer_impl_set_alarm_id(!CONFIG_ESP_TIMER_SUPPORTS_ISR_DISPATCH_METHOD)
paths: ['esp-idf/esp_timer/CMakeFiles/__idf_esp_timer.dir/src/esp_timer_impl_systimer.c.obj']
paths: ['esp-idf/esp_timer/CMakeFiles/__idf_esp_timer.dir/src/esp_timer_impl_common.c.obj']
paths: ['esp-idf/esp_timer/CMakeFiles/__idf_esp_timer.dir/src/ets_timer_legacy.c.obj']
paths: ['esp-idf/esp_timer/CMakeFiles/__idf_esp_timer.dir/src/system_time.c.obj']
paths: ['esp-idf/esp_wifi/CMakeFiles/__idf_esp_wifi.dir/esp32c2/esp_adapter.c.obj']
semphr_take_from_isr_wrapper failed to find section
env_is_chip_wrapper failed to find section
semphr_give_from_isr_wrapper failed to find section
timer_disarm_wrapper failed to find section
timer_arm_us_wrapper failed to find section
wifi_rtc_enable_iso_wrapper failed to find section
wifi_rtc_disable_iso_wrapper failed to find section
malloc_internal_wrapper failed to find section
skip uxListRemove(FALSE)
skip vListInitialise(CONFIG_FREERTOS_PLACE_FUNCTIONS_INTO_FLASH)
skip vListInitialiseItem(CONFIG_FREERTOS_PLACE_FUNCTIONS_INTO_FLASH)
skip vListInsert(CONFIG_FREERTOS_PLACE_FUNCTIONS_INTO_FLASH)
skip vListInsertEnd(FALSE)
skip vApplicationStackOverflowHook(FALSE)
skip vPortYieldOtherCore(FALSE)
paths: ['esp-idf/freertos/CMakeFiles/__idf_freertos.dir/FreeRTOS-Kernel/portable/riscv/port.c.obj']
paths: ['esp-idf/freertos/CMakeFiles/__idf_freertos.dir/port_common.c.obj']
xPortcheckValidStackMem failed to find section
esp_startup_start_app_common failed to find section
paths: ['esp-idf/freertos/CMakeFiles/__idf_freertos.dir/heap_idf.c.obj']
paths: ['esp-idf/freertos/CMakeFiles/__idf_freertos.dir/port_systick.c.obj']
skip xPortSysTickHandler(FALSE)
paths: ['esp-idf/freertos/CMakeFiles/__idf_freertos.dir/FreeRTOS-Kernel/queue.c.obj']
skip __getreent(CONFIG_FREERTOS_PLACE_FUNCTIONS_INTO_FLASH)
skip pcTaskGetName(CONFIG_FREERTOS_PLACE_FUNCTIONS_INTO_FLASH)
skip prvAddCurrentTaskToDelayedList(CONFIG_FREERTOS_PLACE_FUNCTIONS_INTO_FLASH)
skip prvDeleteTLS(CONFIG_FREERTOS_PLACE_FUNCTIONS_INTO_FLASH)
skip pvTaskIncrementMutexHeldCount(CONFIG_FREERTOS_PLACE_FUNCTIONS_INTO_FLASH)
skip taskSelectHighestPriorityTaskSMP(FALSE)
skip taskYIELD_OTHER_CORE(FALSE)
paths: ['esp-idf/freertos/CMakeFiles/__idf_freertos.dir/FreeRTOS-Kernel/tasks.c.obj']
skip vTaskInternalSetTimeOutState(CONFIG_FREERTOS_PLACE_FUNCTIONS_INTO_FLASH)
skip vTaskPlaceOnEventList(CONFIG_FREERTOS_PLACE_FUNCTIONS_INTO_FLASH)
skip vTaskPlaceOnEventListRestricted(CONFIG_FREERTOS_PLACE_FUNCTIONS_INTO_FLASH)
skip vTaskPlaceOnUnorderedEventList(CONFIG_FREERTOS_PLACE_FUNCTIONS_INTO_FLASH)
skip vTaskPriorityDisinheritAfterTimeout(CONFIG_FREERTOS_PLACE_FUNCTIONS_INTO_FLASH)
skip vTaskReleaseEventListLock(CONFIG_FREERTOS_PLACE_FUNCTIONS_INTO_FLASH)
skip vTaskTakeEventListLock(CONFIG_FREERTOS_PLACE_FUNCTIONS_INTO_FLASH)
skip xTaskCheckForTimeOut(CONFIG_FREERTOS_PLACE_FUNCTIONS_INTO_FLASH)
skip xTaskGetCurrentTaskHandle(CONFIG_FREERTOS_PLACE_FUNCTIONS_INTO_FLASH)
skip xTaskGetSchedulerState(CONFIG_FREERTOS_PLACE_FUNCTIONS_INTO_FLASH)
skip xTaskGetTickCount(CONFIG_FREERTOS_PLACE_FUNCTIONS_INTO_FLASH)
skip xTaskPriorityDisinherit(FALSE)
skip xTaskPriorityInherit(CONFIG_FREERTOS_PLACE_FUNCTIONS_INTO_FLASH)
skip prvGetExpectedIdleTime(FALSE)
skip vTaskStepTick(FALSE)
paths: ['esp-idf/hal/CMakeFiles/__idf_hal.dir/efuse_hal.c.obj', 'esp-idf/hal/CMakeFiles/__idf_hal.dir/esp32c2/efuse_hal.c.obj']
paths: ['esp-idf/heap/CMakeFiles/__idf_heap.dir/heap_caps_base.c.obj']
skip hex_to_str(CONFIG_HEAP_ABORT_WHEN_ALLOCATION_FAILS)
skip fmt_abort_str(CONFIG_HEAP_ABORT_WHEN_ALLOCATION_FAILS)
paths: ['esp-idf/heap/CMakeFiles/__idf_heap.dir/heap_caps.c.obj']
paths: ['./esp-idf/heap/CMakeFiles/__idf_heap.dir/multi_heap.c.obj']
paths: ['esp-idf/log/CMakeFiles/__idf_log.dir/log_freertos.c.obj']
paths: ['esp-idf/log/CMakeFiles/__idf_log.dir/log.c.obj']
paths: ['esp-idf/newlib/CMakeFiles/__idf_newlib.dir/assert.c.obj']
paths: ['esp-idf/newlib/CMakeFiles/__idf_newlib.dir/heap.c.obj']
paths: ['esp-idf/newlib/CMakeFiles/__idf_newlib.dir/locks.c.obj']
paths: ['esp-idf/newlib/CMakeFiles/__idf_newlib.dir/reent_init.c.obj']
paths: ['esp-idf/newlib/CMakeFiles/__idf_newlib.dir/time.c.obj']
paths: ['/home/infiniteyuan/espressif/espressif_gitlab/esp-idf/esp-idf-v5.3/components/esp_wifi/lib/esp32c2/libpp.a']
paths: ['esp-idf/pthread/CMakeFiles/__idf_pthread.dir/pthread.c.obj']
paths: ['esp-idf/riscv/CMakeFiles/__idf_riscv.dir/interrupt.c.obj']
intr_matrix_route failed to find section
skip spi_flash_needs_reset_check(FALSE)
skip spi_flash_set_erasing_flag(FALSE)
skip spi_flash_guard_set(CONFIG_SPI_FLASH_ROM_IMPL)
skip spi_flash_malloc_internal(CONFIG_SPI_FLASH_ROM_IMPL)
skip spi_flash_rom_impl_init(CONFIG_SPI_FLASH_ROM_IMPL)
skip esp_mspi_pin_init(CONFIG_SPI_FLASH_ROM_IMPL)
skip spi_flash_init_chip_state(CONFIG_SPI_FLASH_ROM_IMPL)
skip delay_us(CONFIG_SPI_FLASH_ROM_IMPL)
skip get_buffer_malloc(CONFIG_SPI_FLASH_ROM_IMPL)
skip release_buffer_malloc(CONFIG_SPI_FLASH_ROM_IMPL)
skip main_flash_region_protected(CONFIG_SPI_FLASH_ROM_IMPL)
skip main_flash_op_status(CONFIG_SPI_FLASH_ROM_IMPL)
skip spi1_flash_os_check_yield(CONFIG_SPI_FLASH_ROM_IMPL)
skip spi1_flash_os_yield(CONFIG_SPI_FLASH_ROM_IMPL)
skip start(CONFIG_SPI_FLASH_ROM_IMPL)
skip end(CONFIG_SPI_FLASH_ROM_IMPL)
skip delay_us(CONFIG_SPI_FLASH_ROM_IMPL)
Remove lib libble_app.a
Remove lib libbt.a
[992/993] Generating binary image from built executable
esptool.py v4.8.1
Creating esp32c2 image...
```

Device log:
```
I (558) coexist: coex firmware version: 6614b68
I (567) coexist: coexist rom version de8c800
I (575) main_task: Started on CPU0
I (575) main_task: Calling app_main()
I (585) wifi_prph_coex: ESP_WIFI_MODE_STA
I (595) pp: pp rom version: de8c800
I (595) net80211: net80211 rom version: de8c800
I (615) wifi:wifi driver task: 3fcc36f4, prio:23, stack:6144, core=0
I (615) wifi:wifi firmware version: b884461aeb
I (615) wifi:wifi certification version: v7.0
I (625) wifi:config NVS flash: enabled
I (635) wifi:config nano formatting: enabled
I (635) wifi:Init data frame dynamic rx buffer num: 32
I (645) wifi:Init static rx mgmt buffer num: 5
I (655) wifi:Init management short buffer num: 32
I (655) wifi:Init dynamic tx buffer num: 32
I (665) wifi:Init static tx FG buffer num: 2
I (675) wifi:Init static rx buffer size: 1600
I (675) wifi:Init static rx buffer num: 10
I (685) wifi:Init dynamic rx buffer num: 32
I (685) wifi_init: rx ba win: 6
I (695) wifi_init: accept mbox: 6
I (705) wifi_init: tcpip mbox: 32
I (705) wifi_init: udp mbox: 6
I (715) wifi_init: tcp mbox: 6
I (715) wifi_init: tcp tx win: 5760
I (725) wifi_init: tcp rx win: 5760
I (735) wifi_init: tcp mss: 1440
I (735) wifi_init: WiFi RX IRAM OP enabled
I (755) phy_init: phy_version 340,e255ce0,Jun  4 2024,16:44:11
W (755) phy_init: failed to load RF calibration data (0xffffffff), falling back to full calibration
I (795) phy_init: Saving new calibration data due to checksum failure or outdated calibration data, mode(2)
I (815) wifi:mode : sta (10:97:bd:f1:ab:f8)
I (815) wifi:enable tsf
I (815) wifi_prph_coex: wifi_init_sta finished.
I (1105) wifi:new:<1,0>, old:<1,0>, ap:<255,255>, sta:<1,0>, prof:1, snd_ch_cfg:0x0
I (1105) wifi:state: init -> auth (0xb0)
I (1105) wifi:state: auth -> assoc (0x0)
I (1175) wifi:state: assoc -> run (0x10)
I (1205) wifi:connected with myssid, aid = 1, channel 1, BW20, bssid = 60:55:f9:f9:d6:95
I (1205) wifi:security: WPA2-PSK, phy: bgn, rssi: -52
I (1225) wifi:pm start, type: 1

I (1225) wifi:dp: 1, bi: 102400, li: 3, scale listen interval from 307200 us to 307200 us
I (1225) wifi:set rx beacon pti, rx_bcn_pti: 14, bcn_timeout: 25000, mt_pti: 14, mt_time: 10000
I (1285) wifi:AP's beacon interval = 102400 us, DTIM period = 1
I (2245) esp_netif_handlers: sta ip: 192.168.4.2, mask: 255.255.255.0, gw: 192.168.4.1
I (2245) wifi_prph_coex: got ip:192.168.4.2
I (2245) wifi_prph_coex: connected to ap SSID:myssid password:mypassword
I (2255) BLE_INIT: Using main XTAL as clock source
I (2265) BLE_INIT: ble controller commit:[7b7ee44]
I (2275) BLE_INIT: ble rom commit:[3314f9d]
I (2285) BLE_INIT: Bluetooth MAC: 10:97:bd:f1:ab:fa
I (2285) phy: libbtbb version: b97859f, Jun  4 2024, 16:44:27
I (2305) wifi_prph_coex: BLE Host Task Started
I (2305) NimBLE: GAP procedure initiated: stop advertising.

I (2315) wifi_prph_coex: Device Address:10:97:bd:f1:ab:fa
I (2325) NimBLE: GAP procedure initiated: advertise; 
I (2335) NimBLE: disc_mode=2
I (2345) NimBLE:  adv_channel_map=0 own_addr_type=0 adv_filter_policy=0 adv_itvl_min=0 adv_itvl_max=0
I (2355) NimBLE: 

W (2365) wifi_prph_coex: Relinker Example End
W (2365) wifi:<ba-add>idx:0 (ifx:0, 60:55:f9:f9:d6:95), tid:0, ssn:0, winSize:64
I (2375) main_task: Returned from app_main()
```