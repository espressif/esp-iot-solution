# ChangeLog

## v1.5.1 - 2025-7-15

### Bug Fixes:

* Fix compilation errors on esp-idf release v5.3 and above.

## v1.5.0 - 2024-12-10

### Improvements:

* Make header EOF(1 << 1) check can be disabled by `CONFIG_UVC_CHECK_HEADER_EOF`. Because some non-standard cameras set this bit in the header of every frame.

## v1.4.0 - 2024-05-21

### Improvements:

* UVC:
  * Add support for Bulk camera with `Multiple Transfers per Sample` (see UVC1.5 section 2.4.3.2.1)

## v1.3.2 - 2024-02-04

* Fix build error under latest ESP-IDF master (5.3), due to the changes in ``usb_dwc_ll.h``

## v1.3.1 - 2023-12-29

### Bug Fixes:

1. Fix link error under ESP-IDF 5.3

## v1.3.0 - 2023-12-14

* Support UVC frame type: frame based, which can be used to transmit H264 streams.

## v1.2.1 - 2023-11-27

### Bug Fixes:

* Fix `W (44) boot.esp32s3: PRO CPU has been reset by WDT.` after apply the IDF commit [2368837](https://github.com/espressif/esp-idf/commit/236883728d1d9ed835da15cfec4d37220ecec017)

## v1.2.0 - 2023-10-11

### Enhancements:

* USB:
  * Improve control transfer timeout handling
  * Add sysviwer debug support
* UVC:
  * Change sample process task core affinity option, support config -1 to not pin to core
* UAC:
  * Add Mic data save to SDCard test case

### Bug Fixes:

* UAC:
  * Volume/Mute: remove set volume/mute default value (to walk around some camera issue), user need to set after stream start
* UVC:
  * Fix bug when specified resolution not found.

## v1.1.2 - 2023-09-18

### Bug Fixes:

* UAC:
  * Mic：disable compensation by default, the frame data_bytes in `mic_cb` varies depending on host.
  * Mic：add separate `mic_frame_buf` to restore the frame data, fix the frame data may be overwritten in special case (MPS > sizeof(frame_ms)).

## v1.1.1 - 2023-08-31

### Bug Fixes:

* UAC:
  * Fix padding 0 length
  * Continuous padding 0 with buffer empty timeout (default 80ms) to avoid speaker noise

## v1.1.0 - 2023-08-02

### Bug Fixes:

* UVC:
  * Set UVC frame interval to default if user config not match

## v1.0.6 - 2023-07-21

### Bug Fixes:

* USB:
  * Fix transfer timeout cause USB refused to work (recover usb pipe in this case)

## v1.0.5 - 2023-07-21

### Enhancements:

* UVC:
  * Enable bulk packets reassembling depends on the probe results
  * Skip check EOH in payload header for compatibility (TinyUSB UVC device not set this bit)

## v1.0.4 - 2023-07-05

### Enhancements:

* USB:
  * Add retry mechanism if device enumeration failed

## v1.0.3 - 2023-06-01

### Bug Fixes:

* Fix Quick Start Mode usb stream can not stop

### Enhancements:

* UAC:
  * Support UAC work in Quick Start Mode (Device must support skip enum process)

## v1.0.2 - 2023-05-19

### Enhancements:

* UAC:
  * Add UAC audio player example, play 48 KHz 16bit 2ch audio to USB headset

## v1.0.1 - 2023-05-18

### Enhancements:

* USB:
  * Support resume/suspend stream in `state_callback_t` callback function.

* UAC:
  * Optimize speaker zero compensation logic (When speaker stream running and buffer empty for a long time, the host will padding n(ms) zero packets to avoid device noise)
  * If UAC has multiple alt interfaces, chose the first as default

### Bug Fixes:

Add additional delay to avoid malloc error if continuous suspend and resume

## v1.0.0 - 2023-05-10

### Release v1.0.0

### Enhancements:

* USB:
  * Refactor USB state machine, handle error and hot-plug more robustly.
  * Support user submit control transfer at any time (Better to support more control requests in future).
  * Support self-define initial state through `flags`, eg `FLAG_UAC_SPK_SUSPEND_AFTER_START` to make speaker suspend as startup state.
  * Support Stream Quick Start Mode (Skip the enum process to save at least 50 ms).

* UVC:
  * Support get camera resolution list and current resolution.
  * Support dynamically config UVC frame rate and resolution.

* UAC:
  * Support config UAC `*_ch_num`, `*_samples_frequence`, `*_bit_resolution` to `*_ANY` to adapter any available device.
  * Support get audio frame rate and resolution list.
  * Support dynamically config audio channel number, rate, and bits.

* Test:
  * Add hardware test for usb_stream component.

### Bug Fixes:
  * Fix some hot-plug issues
  * Fix UAC control interface can not be 0
  * Fix UAC stereo headset volume control failed

### Breaking Changes:

* UAC:
  * Remove the configuration `mic_min_bytes` from `uac_config_t`, use KConfig option `CONFIG_UAC_MIC_CB_MIN_MS_DEFAULT` instead. The default value is 20 ms, which means the mic callback will be called every 20 ms.
  * Add the configuration `flags` to `uac_config_t` and `uvc_config_t`, users can use it to define the initial state of the device. The default value is 0.
  * Add the configuration `mic_ch_num` and `spk_ch_num` to `uac_config_t`, users can specify the number of the microphone and speaker channels. The default value is 0, which means support any number of channels.

## v0.4.13 - 2023-04-13

### Enhancements

* test_apps:
  * Update test_apps project

## v0.4.12 - 2023-04-12

### Bug Fixes:

* Mic:
  * Fix mic_min_bytes may exceed the limits

## v0.4.11 - 2023-03-22

### Bug Fixes:

* Mic:
  * Fix adaptive endpoint occasional assert "rem_len<=transfer->num_bytes" to crash

### Enhancements:

* Mic:
  * Padding last frame data if packet size not as expected

## v0.4.10 - 2023-03-16

### Enhancements:

* Add example: usb_camera_lcd_display

## v0.4.9 - 2023-03-09

### Enhancements:

* Use cu_pkg_define_version to define the version of this component.

## v0.4.8 - 2023-03-02

### Enhancements:

* Add user guide for component manager

## v0.4.7 - 2023-02-20

### Bug Fixes:

* Camera:
  * Fix incorrect isoc buffer offset when transfer error occurs

## v0.4.6 - 2023-02-17

### Enhancements:

* Camera:
  * Add additional JPEG header check for bulk transfer

# ChangeLog

## v0.4.5 - 2023-02-14

### Bug Fixes:

* Remove unnecessary header files "hal/usbh_ll.h" "hal/usb_hal.h"

### Enhancements:

* Camera:
  * Support camera with length of 2, 6 bytes MJPEG UVC header
  * Support bulk camera with EOF bit set in each payload

## v0.4.4 - 2023-02-09

### Enhancements:

* Remove all assert, replace with error handler

### Bug Fixes:

* Fix error message during disconnect

## v0.4.3 - 2023-02-07

### Enhancements:

* Remove some unnecessary vTaskDelay
* Camera:
  * Support for bulk camera with suspend/resume control
  * Support force uvc format index if not found
  * Add support for bulk camera with MPS(Max Packet Size) equal to 512 bytes

### Bug Fixes:

* Speaker:
  * fix some speaker resume control not work (should set freq after resume)

## v0.4.2 - 2023-01-30

### Enhancements:

* Camera:
  * Disable BULK_BYTES_PER_URB_SAME_AS_FRAME by default, support reassemble bulk package segment (will save one image frame size RAM)
  * Add config UVC_DROP_NO_EOF_FRAME, if disable(default) the frame with no EOF will be reserved (MJPEG may broken)
  * Add config UVC_DROP_OVERFLOW_FRAME, if enable(default) the overflow frame will be dropped

### Bug Fixes:

* Camera
  * fix transfer size in GET_CUR Probe stage


## v0.4.1 - 2023-01-05

### Enhancements:

* Add IRAM_ATTR to packet parsing functions
* Speaker:
  * padding zero packet if speaker buffer empty


## v0.4.0 - 2022-12-22

### Enhancements:

* Support parsing descriptors to check the device functions and capabilities
* Self-adaption appropriate interfaces based on descriptors and user's configurations
* Camera:
  * Support UVC config to any resolution, the first frame index will be used

### API Changes:

* add `optional` label to some members in `uvc_config_t` and `uac_config_t`, users can optionally set a value. The self-adaption process will first use actual configs from descriptors, if no appropriate interface found, will use the params from user's configuration

### Bug Fixes:

* USB:
  * Fix STALL (appear on some devices) during get short configuration descriptor
  * Resize endpoint 0 MPS based on device descriptor

## v0.3.3 - 2022-12-22

### Enhancements:

* print version message

### Bug Fixes:

* Camera:
  * Fix not parse header only frames

## v0.3.2 - 2022-12-16

### Enhancements:

* Mic:
  * Padding last frame data when receive zero length packet

## v0.3.1 - 2022-12-8

### Enhancements:

* add development documentation https://docs.espressif.com/projects/esp-iot-solution/zh_CN/latest/usb/usb_host/usb_stream.html

## v0.3.0 - 2022-12-05

### Bug Fixes:

* USB:
  * Fix memory leak during hot-plug
