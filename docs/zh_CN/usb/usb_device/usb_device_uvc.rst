USB Device UVC
====================

:link_to_translation:`en:[English]`

``usb_device_uvc`` 是用于 ESP32-S2/ESP32-S3/ESP32-P4 的 USB ``UVC`` 设备驱动程序，它支持将 JPEG 帧流传输到 USB 主机。用户可以通过回调函数将相机或任何设备包装成符合 UVC 标准的设备。

特性：

1. 支持通过 UVC 流接口进行视频流传输
2. 支持 ``Isochronous`` 和 ``Bulk`` 模式
3. 支持多种分辨率和帧率

将组件添加到项目
-------------------------------

请使用组件管理器命令 ``add-dependency`` 将 ``usb_device_uvc`` 添加到项目的依赖项中，在 ``CMake`` 步骤中会自动下载该组件

.. code:: sh

    idf.py add-dependency "espressif/usb_device_uvc=*"

用户参考
-------------------------------

该组件仅提供一个 API 用于配置 UVC 设备。由于驱动基于 ``TinyUSB`` 堆栈，因此未提供 ``deinit`` API。

.. code:: c

    #include "usb_device_uvc.h"

    static esp_err_t camera_start_cb(uvc_format_t format, int width, int height, int rate, void *cb_ctx)
    {
        // 用户可以在这里初始化相机
        // 相机应根据给定的格式、宽度、高度和帧率进行初始化
        return ESP_OK;
    }

    static void camera_stop_cb(void *cb_ctx)
    {
        // 用户代码
        return;
    }

    static uvc_fb_t* camera_fb_get_cb(void *cb_ctx)
    {
        // 用户代码以返回图像帧缓冲区
        // 相机应准备下一帧，并返回帧缓冲区
        return uvc_fb;
    }

    static void camera_fb_return_cb(uvc_fb_t *fb, void *cb_ctx)
    {
        // 在帧缓冲区被复制到传输缓冲区后返回
        // 用户代码以回收帧缓冲区
        return;
    }

    // 缓冲区用于存储要发送到主机的数据
    const size_t buff_size = 30 * 1024;
    uint8_t *uvc_buffer = (uint8_t *)heap_caps_malloc(buff_size, MALLOC_CAP_DEFAULT);
    assert(uvc_buffer != NULL);

    uvc_device_config_t config = {
        .uvc_buffer = uvc_buffer,
        .uvc_buffer_size = 40 * 1024,
        .start_cb = camera_start_cb,
        .fb_get_cb = camera_fb_get_cb,
        .fb_return_cb = camera_fb_return_cb,
        .stop_cb = camera_stop_cb,
        .cb_ctx = NULL,
    };

    ESP_ERROR_CHECK(uvc_device_config(0, &config));
    ESP_ERROR_CHECK(uvc_device_init());

示例
-------------------------------

:example:`usb/device/usb_webcam`
:example:`usb/device/usb_dual_uvc_device`

API 参考
-------------------------------

.. include-build-file:: inc/usb_device_uvc.inc