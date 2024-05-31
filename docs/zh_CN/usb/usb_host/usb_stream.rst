USB Stream 组件说明
======================

:link_to_translation:`en:[English]`

``usb_stream`` 是基于 ESP32-S2/ESP32-S3 的 USB UVC + UAC 主机驱动程序，支持从 USB 设备读取/写入/控制多媒体流。例如最多同时支持 1 路摄像头 + 1 路麦克风 + 1 路播放器数据流。

特性：

1. 支持通过 UVC Stream 接口获取视频流，支持批量和同步两种传输模式
2. 支持通过 UAC Stream 接口获取麦克风数据流，发送播放器数据流
3. 支持通过 UAC Control 接口控制麦克风音量、静音等特性
4. 支持自动解析设备配置描述符
5. 支持对数据流暂停和恢复

USB Stream 用户指南
---------------------

-  开发板

   1. 可以使用任何带有 USB 接口的 ESP32-S2/ESP32-S3 开发板，注意该 USB 接口需要能够向外供电

-  USB UVC 功能

   1. 摄像头必须兼容 ``USB1.1`` 全速（Fullspeed）模式
   2. 摄像头需要自带 ``MJPEG`` 压缩
   3. 用户可通过 :cpp:func:`uvc_streaming_config` 函数手动指定相机接口、传输模式和图像帧参数
   4. 如使用同步传输摄像头，需要支持设置接口 MPS（Max Packet Size） 为 512
   5. 同步传输模式，图像数据流 USB 传输总带宽应小于 4 Mbps （500 KB/s）
   6. 批量传输模式，图像数据流 USB 传输总带宽应小于 8.8 Mbps （1100 KB/s）
   7. 其它特殊相机要求，请参考示例程序 README.md

-  USB UAC 功能

   1. 音频功能必须兼容 ``UAC 1.0`` 协议
   2. 用户需要通过 :cpp:func:`uac_streaming_config` 函数手动指定 spk/mic 采样率，位宽参数

-  USB UVC + UAC 功能

   1. UVC 和 UAC 功能可以单独启用，例如仅配置 UAC 来驱动一个 USB 耳机，或者仅配置 UVC 来驱动一个 USB 摄像头
   2. 如需同时启用 UVC + UAC，该驱动程序目前仅支持具有摄像头和音频接口的复合设备（``Composite Device``），不支持同时连接两个单独的设备

USB Stream API 参考
----------------------

1. 用户可通过 :cpp:type:`uvc_config_t` 配置摄像头分辨率、帧率参数。通过 :cpp:type:`uac_config_t` 配置音频采样率、位宽等参数。参数说明如下:

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

2. 使用 :cpp:func:`uvc_streaming_config` 配置 UVC 驱动，如果设备同时支持音频，可使用 :cpp:func:`uac_streaming_config` 配置 UAC 驱动
3. 使用 :cpp:func:`usb_streaming_start` 打开数据流，之后驱动将响应设备连接和协议协商
4. 之后，主机将根据用户参数配置，匹配已连接设备的描述符，如果设备无法满足配置要求，驱动将以警告提示
5. 如果设备满足用户配置要求，主机将持续接收 IN 数据流（UVC 和 UAC 麦克风），并在新帧准备就绪后调用用户的回调

   1. 接收到新的 MJPEG 图像后，将触发 UVC 回调函数。用户可在回调函数中阻塞，因为它在独立的任务上下文中工作
   2. 接收到 ``mic_min_bytes`` 字节数据后，将触发 mic 回调。但是这里的回调一定不能以任何方式阻塞，否则会影响下一帧的接收。如果需要对 mic 进行阻塞操作，可以通过 ``uac_mic_streaming_read`` 轮询方式代替回调方式

6. 发送扬声器数据时，用户可通过 :cpp:func:`uac_spk_streaming_write` 将数据写入内部 ringbuffer，主机将在 USB 空闲时从中获取数据并发送 OUT 数据
7. 使用 :cpp:func:`usb_streaming_control` 控制流挂起/恢复。如果 UAC 支持特性单元，可通过其分别控制麦克风和播放器的音量和静音
8. 使用 :cpp:func:`usb_streaming_stop` 停止 USB 流，USB 资源将被完全释放。

Bug 报告
-----------

ESP32-S2 ECO0 芯片 SPI 屏幕和 USB 同时启用，可能导致屏幕抖动
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

1. 在最早版本的 ESP32-S2 ECO0 芯片上, USB 可能污染 SPI 数据 (ESP32-S2 新版本>=ECO1 和 ESP32-S3 均不存在该 Bug)
2. 软件解决方案

-  ``spi_ll.h`` 添加以下接口

.. code:: c

   //components/hal/esp32s2/include/hal/spi_ll.h
   static inline uint32_t spi_ll_tx_get_fifo_cnt(spi_dev_t *hw)
   {
       return hw->dma_out_status.out_fifo_cnt;
   }

-  如下在 ``spi_new_trans`` 中添加额外检查

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
----------

1. :example:`usb/host/usb_camera_mic_spk`
2. :example:`usb/host/usb_camera_lcd_display`
3. :example:`usb/host/usb_audio_player`

API Reference
--------------

.. include-build-file:: inc/usb_stream.inc
