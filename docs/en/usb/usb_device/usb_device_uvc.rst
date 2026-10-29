USB Device UVC
====================

:link_to_translation:`zh_CN:[中文]`

``usb_device_uvc`` is a USB ``UVC`` device driver for ESP32-S2/ESP32-S3/ESP32-P4, which supports streaming JPEG frames to the USB Host. User can wrapper the Camera or any devices as a UVC standard device through the callback functions.

Features:

1. Support video stream through the UVC Stream interface
2. Support both ``Isochronous`` and ``Bulk`` mode
3. Support multiple resolutions and frame rates

Add component to your project
-------------------------------

Please use the component manager command ``add-dependency`` to add the ``usb_device_uvc`` to your project's dependency, during the ``CMake`` step the component will be downloaded automatically

.. code:: sh

    idf.py add-dependency "espressif/usb_device_uvc=*"

User Reference
-------------------------------

The component provides only one API to configure the UVC device. As the driver based on the ``TinyUSB`` stack, the deinit API is not provided.

.. code:: c

    #include "usb_device_uvc.h"

    static esp_err_t camera_start_cb(uvc_format_t format, int width, int height, int rate, void *cb_ctx)
    {
        // user can initialize the camera here
        // camera should be initialized with the given format, width, height and frame rate
        return ESP_OK;
    }

    static void camera_stop_cb(void *cb_ctx)
    {
        //user code
        return;
    }

    static uvc_fb_t* camera_fb_get_cb(void *cb_ctx)
    {
        // user code to return a image frame buffer
        // camera should prepare next frame, and return the frame buffer
        return uvc_fb;
    }

    static void camera_fb_return_cb(uvc_fb_t *fb, void *cb_ctx)
    {
        // the frame buffer is returned after it is copied to the transfer buffer
        // user code to recycle the frame buffer
        return;
    }

    //the buffer is used to store the data to be sent to the host
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

Examples
-------------------------------

:example:`usb/device/usb_webcam`
:example:`usb/device/usb_dual_uvc_device`

API Reference
-------------------------------

.. include-build-file:: inc/usb_device_uvc.inc