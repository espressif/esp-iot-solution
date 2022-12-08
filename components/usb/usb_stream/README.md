## USB Stream Component

`usb_stream` is an USB `UVC` + `UAC` host driver for ESP32-Sx, which supports read/write/control multimedia streaming from usb device. For example, at most one UVC + one Microphone + one Speaker streaming can be supported at the same time.

The driver support control UVC/UAC stream to `suspend` or `resume` via a simple API interface, and support control UAC `mute` and `volume` if the device has corresponding `feature unit`.

Stream frames can be handled by registering a frame callback, or using API like `_read` or `_write`.


### USB Stream User Guide

* Development board

  1. Any `ESP32-S2`,` ESP32-S3` board with USB Host port can be used.
  2. Please refer example readme for special hardware requirements

* USB UVC function

  1. Camera must be compatible with `USB1.1` full-speed mode
  2. Camera must support `MJPEG` compression
  3. Users need to manually specify the camera interface, transmission mode, and image frame params through `_config` function
  4. For isochronous mode camera, interface `max packet size` should be less than `512` bytes
  5. For isochronous mode camera, frame stream bandwidth should be less than `4 Mbps` (500 KB/s)
  6. For bulk mode camera, frame stream bandwidth should be less than `8.8 Mbps` (1100 KB/s)
  7. Please refer example readme for special camera requirements

* USB UAC function

  1. Audio function must be compatible with `UAC1.0` protocol
  2. Users need to manually specify the spk/mic interface, sampling rate, bit width params through `_config` function

* USB UVC + UAC

  1. The Driver only support `Composite Device` with camera and audio interface

### USB Stream API Reference

1. Users need to know device detailed descriptors in advance, the `uvc_config_t` `uac_config_t` should be filled based on the information, the parameters correspondence is as follows:

```c
uvc_config_t uvc_config = {
        .xfer_type = UVC_XFER_ISOC,  // camera transfer mode
        .format_index = 1, // mjpeg format index, for example 1
        .frame_width = 320, // mjpeg width pixel, for example 320
        .frame_height = 240, // mjpeg height pixel, for example 240
        .frame_index = 1, // frame resolution index, for example 1
        .frame_interval = 666666, // frame interval (100µs units), such as 15fps
        .interface = 1, // video stream interface number, generally 1
        .interface_alt = 1, // alt interface number, `max packet size` should be less than `512` bytes
        .ep_addr = 0x81, // alt interface endpoint address, for example 0x81
        .ep_mps = 512, // alt interface endpoint MPS, for example 512
        .xfer_buffer_size = 32 * 1024, // single frame image size, need to be determined according to actual testing, 320 * 240 generally less than 35KB
        .xfer_buffer_a = pointer_buffer_a, // the internal transfer buffer
        .xfer_buffer_b = pointer_buffer_b, // the internal transfer buffer
        .frame_buffer_size = 32 * 1024, // single frame image size, need to determine according to actual test
        .frame_buffer = pointer_frame_buffer, // the image frame buffer
        .frame_cb = &camera_frame_cb, //camera callback, can block in here
        .frame_cb_arg = NULL, // camera callback args
    }
```

```c
    uac_config_t uac_config = {
        .mic_interface = 4,  // microphone interface number, 0 if not use
        .mic_bit_resolution = 16, //microphone resolution, bits
        .mic_samples_frequence = 16000, //microphone frequence, Hz
        .mic_ep_addr = 0x82, //microphone interface endpoint address
        .mic_ep_mps = 32,  //microphone interface endpoint mps
        .spk_interface = 3, //speaker stream interface number, 0 if not use
        .spk_bit_resolution = 16, //speaker resolution, bits
        .spk_samples_frequence = 16000, //speaker frequence, Hz
        .spk_ep_addr = 0x02, //speaker interface endpoint address
        .spk_ep_mps = 32, //speaker interface endpoint mps
        .spk_buf_size = 16000, //size of speaker send buffer, should be a multiple of spk_ep_mps
        .mic_buf_size = 0, //mic receive buffer size, 0 if not use, else should be a multiple of mic_min_bytes
        .mic_min_bytes = 320, //min bytes to trigger mic callback, 0 if not using callback, else must be multiple (1~32) of mic_ep_mps
        .mic_cb = &mic_frame_cb, //mic callback, can not block in here
        .mic_cb_arg = NULL, //mic callback args
        .ac_interface = 2, //(optional) audio control interface number, set 0 if not use
        .spk_fu_id = 2, //(optional) speaker feature unit id, set 0 if not use
        .mic_fu_id = 0, //(optional) microphone feature unit id, set 0 if not use
    };
```

1. Use the `uvc_streaming_config` and above `uvc_config_t` parameters to config the UVC driver, and use the `uac_streaming_config` and above `uac_config_t` parameters to config the UAC driver if the device support;
2. Use the `usb_streaming_start` to turn on the stream, after USB connection and negotiation.
3. The Host will continue to receive the IN stream (UVC and UAC mic), and will call the user's callbacks when new frames ready. User can send Speaker OUT stream through a ringbuffer, the Host will fetch the data when USB is free to send.
   1. The camera callback will be triggered after a new MJPEG image ready. The callback can block during processing, because which works in an independent task context.
   2. The mic callback will be triggered after `mic_min_bytes` bytes data received. But the callback here must not block in any way, otherwise it will affect the reception of the next frame. If the block operations for mic is necessary, you can use the polling mode instead of the callback mode through `uac_mic_streaming_read` api.
4. Use the `usb_streaming_control` to control the stream suspend/resume, uac volume/mute can also be support if the USB device has such features ;
5. Use the `usb_streaming_stop` to stop the usb stream, USB resource will be completely released.

### Bug report

1. ESP32S2 ECO0 Chip SPI screen jitter when work with usb camera 
   1. In earlier versions of the ESP32S2 chip, USB transfers can cause SPI data contamination (esp32s2>=ECO1 and esp32s3 do not have this bug)
   2. Software workaround
      1. `spi_ll.h` add below function
    ```c
    //components/hal/esp32s2/include/hal/spi_ll.h
    static inline uint32_t spi_ll_tx_get_fifo_cnt(spi_dev_t *hw)
    {
        return hw->dma_out_status.out_fifo_cnt;
    }
    ```
      2. modify `spi_new_trans` implement as below
    ```c
    // The function is called to send a new transaction, in ISR or in the task.
    // Setup the transaction-specified registers and linked-list used by the DMA (or FIFO if DMA is not used)
    static void SPI_MASTER_ISR_ATTR spi_new_trans(spi_device_t *dev, spi_trans_priv_t *trans_buf)
    {
        //...................
        spi_hal_setup_trans(hal, hal_dev, &hal_trans);
        spi_hal_prepare_data(hal, hal_dev, &hal_trans);

        //Call pre-transmission callback, if any
        if (dev->cfg.pre_cb) dev->cfg.pre_cb(trans);
    #if 1
        //USB Bug workaround
        //while (!((spi_ll_tx_get_fifo_cnt(SPI_LL_GET_HW(host->id)) == 12) || (spi_ll_tx_get_fifo_cnt(SPI_LL_GET_HW(host->id)) == trans->length / 8))) {
        while (trans->length && spi_ll_tx_get_fifo_cnt(SPI_LL_GET_HW(host->id)) == 0) {
            __asm__ __volatile__("nop");
            __asm__ __volatile__("nop");
            __asm__ __volatile__("nop");
        }
    #endif
        //Kick off transfer
        spi_hal_user_start(hal);
    }
    ```

### Examples

[USB Camera + Audio stream](https://github.com/espressif/esp-iot-solution/tree/master/examples/usb/host/usb_camera_mic_spk)