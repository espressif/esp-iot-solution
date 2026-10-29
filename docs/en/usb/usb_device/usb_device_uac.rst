USB Device UAC
====================

:link_to_translation:`zh_CN:[中文]`

``esp_device_uac`` is a USB Audio Class driver library based on TinyUSB. It supports simulating an ESP chip as an audio device, with customizable audio sampling rates, microphone channels, and speaker channels.

Features:

1. Supports ISO FeedBack communication interface by default. It automatically syncs with the host based on the remaining size of the UAC FIFO memory. `Reference <https://github.com/hathach/tinyusb/pull/2328>`__.
2. Allows custom audio sampling rates, microphone channels, and speaker channels.
3. Buffers a segment of data before transmission when speaker data arrives, which helps reduce the frequency of audio data transmission interruptions.

USB Device UAC User Guide
--------------------------

- Development Board

    1. Any ESP32-S2/ESP32-S3/ESP32-P4 development board with a USB interface can be used.

- USB MIC Callback Function

    1. The ``uac_input_cb_t`` callback function is used to transfer audio data to the USB host. Users should transfer audio data according to the timeline or block the callback function while waiting for audio data.
    2. Set the length of the audio data read by the callback function by defining the ``CONFIG_UAC_MIC_INTERVAL_MS`` macro.
        - Setting ``CONFIG_UAC_MIC_INTERVAL_MS=10`` with a sampling rate of 48000Hz, 16-bit precision, and single channel results in a read data length of 10ms * 48000Hz / 1000 * 2bytes = 960bytes.
    3. Set the length of the first audio write by the callback function using the ``UAC_SPK_INTERVAL_MS`` macro. To prevent high audio data transmission interruption frequency, the default is 10ms. Subsequent audio writes will be transmitted in approximately 1ms data lengths.
    4. The ``UAC_SPK_NEW_PLAY_INTERVAL`` macro determines whether the incoming audio is new. If it is new, it will buffer a segment of data before transmission, which helps reduce the frequency of audio data transmission interruptions.
    5. The ``UAC_SUPPORT_MACOS`` macro supports MacOS. Note that enabling this macro may make the device unrecognizable by Windows systems.

USB Device UAC API Reference
------------------------------

1. Users can configure four callback functions for audio input/output, mute, and volume by calling the :cpp:type:`uac_device_config_t` function.

.. code:: c

    uac_device_config_t config = {
        .output_cb = uac_device_output_cb,          // Speaker output callback
        .input_cb = uac_device_input_cb,            // Microphone input callback
        .set_mute_cb = uac_device_set_mute_cb,      // Set mute callback
        .set_volume_cb = uac_device_set_volume_cb,  // Set volume callback
        .cb_ctx = NULL,
    };
    uac_device_init(&config);

Example
----------

- :example:`usb/device/usb_uac`

API Reference
----------------

.. include-build-file:: inc/usb_device_uac.inc
