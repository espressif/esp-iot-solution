USB Stream Component
=====================

:link_to_translation:`zh_CN:[中文]`

``usb_stream`` is an USB ``UVC`` + ``UAC`` host driver for ESP32-S2/ESP32-S3, which supports read/write/control multimedia streaming from usb device. For example, at most one UVC + one Microphone + one Speaker streaming can be supported at the same time.

Features:

1. Support video stream through UVC Stream interface, support both isochronous and bulk mode
2. Support microphone stream and speaker stream through the UAC Stream interface
3. Support volume, mute and other features control through the UAC Control interface
4. Supports automatic parse of device configuration descriptors
5. Support stream separately suspend and resume

USB Stream User Guide
----------------------

-  Development board

   1. Any ``ESP32-S2`` / ``ESP32-S3`` board with USB Host port can be used. Note that the port must be able to output voltage.

-  USB UVC function

   1. Camera must be compatible with ``USB1.1`` full-speed mode
   2. Camera must support ``MJPEG`` output
   3. Users can manually specify the camera interface, transmission mode, and image frame params through :cpp:func:`uvc_streaming_config`
   4. For isochronous mode camera, interface ``max packet size`` should no more than ``512`` bytes
   5. For isochronous mode camera, frame stream bandwidth should be less than ``4 Mbps`` (500 KB/s)
   6. For bulk mode camera, frame stream bandwidth should be less than ``8.8 Mbps`` (1100 KB/s)
   7. Please refer example readme for special camera requirements

-  USB UAC function

   1. Audio function must be compatible with ``UAC1.0`` protocol
   2. Users need to manually specify the spk/mic sampling rate, bit width params through :cpp:func:`uac_streaming_config` function

-  USB UVC + UAC

   1. The UVC and UAC functions can be enabled separately. For example, only the UAC be configured to drive a usb headset, or only the UVC be configured to drive a USB camera
   2. If you need to enable UVC and UAC at the same time, note that the driver currently only supports ``Composite devices`` with both camera and audio interfaces, rather than two separate devices.

USB Stream Config Reference
-----------------------------

1. Using :cpp:type:`uvc_config_t` to configure camera resolution and frame rate parameters. Using :cpp:type:`uac_config_t` to configure the audio sampling rate, bit width and other parameters. The parameters are described as follows:

.. code:: c

   uvc_config_t uvc_config = {
       .frame_width = 320, // mjpeg width pixel, for example 320
       .frame_height = 240, // mjpeg height pixel, for example 240
       .frame_interval = FPS2INTERVAL(15), // frame interval (100µs units), such as 15fps
       .xfer_buffer_size = 32 * 1024, // single frame image size, need to be determined according to actual testing, 320 * 240 generally less than 35KB
       .xfer_buffer_a = pointer_buffer_a, // the internal transfer buffer
       .xfer_buffer_b = pointer_buffer_b, // the internal transfer buffer
       .frame_buffer_size = 32 * 1024, // single frame image size, need to determine according to actual test
       .frame_buffer = pointer_frame_buffer, // the image frame buffer
       .frame_cb = &camera_frame_cb, //camera callback, can block in here
       .frame_cb_arg = NULL, // camera callback args
   }

   uac_config_t uac_config = {
       .mic_bit_resolution = 16, //microphone resolution, bits
       .mic_samples_frequence = 16000, //microphone frequency, Hz
       .spk_bit_resolution = 16, //speaker resolution, bits
       .spk_samples_frequence = 16000, //speaker frequency, Hz
       .spk_buf_size = 16000, //size of speaker send buffer, should be a multiple of spk_ep_mps
       .mic_buf_size = 0, //mic receive buffer size, 0 if not use, else should be a multiple of mic_min_bytes
       .mic_cb = &mic_frame_cb, //mic callback, can not block in here
       .mic_cb_arg = NULL, //mic callback args
   };

2. Use the :cpp:func:`uvc_streaming_config` to config the UVC driver, or use the :cpp:func:`uac_streaming_config` to config the UAC driver
3. Use the :cpp:func:`usb_streaming_start` to turn on the stream, then the driver will handle USB connection and negotiation.
4. The host will matches the descriptors of the connected devices according to the user parameters. If the device fails to meet the configuration requirements, the driver prompt warning message
5. If the device meets user configuration requirements, the Host will continue to receive the IN stream (UVC and UAC mic), and will call the user’s callbacks when new frames ready.

   1. The camera callback will be triggered after a new MJPEG image ready. The callback can block during processing, because which works in an independent task context.
   2. The mic callback will be triggered after :cpp:func:`mic_min_bytes` bytes data received. But the callback here must not block in any way, otherwise it will affect the reception of the next frame. If the block operations for mic is necessary, you can use the polling mode instead of the callback mode through :cpp:func:`uac_mic_streaming_read` api.

6. User can send speaker OUT stream using :cpp:func:`uac_spk_streaming_write` through a ringbuffer, the Host will fetch the data when USB is free to send.
7. Use the :cpp:func:`usb_streaming_control` to control the stream suspend/resume, uac volume/mute control can also be support if the USB device has such feature unit;
8. Use the :cpp:func:`usb_streaming_stop` to stop the usb stream, USB resource will be completely released.

Bug report
-----------

ESP32-S2 ECO0 Chip SPI screen jitter when work with usb camera
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

1. In earlier versions of the ESP32-S2 chip, USB transfers can cause SPI data contamination (esp32s2>=ECO1 and esp32s3 do not have this bug)
2. Software workaround

-  ``spi_ll.h`` add below function

.. code:: c

   //components/hal/esp32s2/include/hal/spi_ll.h
   static inline uint32_t spi_ll_tx_get_fifo_cnt(spi_dev_t *hw)
   {
       return hw->dma_out_status.out_fifo_cnt;
   }

-  modify ``spi_new_trans`` implement as below

.. code:: c

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

Examples
---------

1. :example:`usb/host/usb_camera_mic_spk`
2. :example:`usb/host/usb_camera_lcd_display`
3. :example:`usb/host/usb_audio_player`

API Reference
---------------------

.. include-build-file:: inc/usb_stream.inc
