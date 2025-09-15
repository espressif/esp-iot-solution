Developing with Native tinyusb
--------------------------------

:link_to_translation:`zh_CN:[中文]`

This guide contains the following content:

.. contents:: Table of Contents
    :local:
    :depth: 2

This section will introduce how to use tinyusb components for development.

Project Directory
~~~~~~~~~~~~~~~~~~~

First, create the following directory structure:

.. code:: C

    project_name
    |
    |-- main
        |-- CMakeLists.txt
        |-- idf_component.yml
        |-- main.c
        |-- tusb
            |-- tusb_config.h
            |-- usb_descriptors.c
            |-- usb_descriptors.h

Add component dependencies in the main component of the project, referencing :ref:`espressif/tinyusb <espressif/tinyusb>`.

The tusb folder primarily contains files to be provided to tinyusb. We place them in a separate folder to ensure simplicity in dependency relationships. Then, add the following statements in the ``main/CMakeLists.txt`` file (after idf_component_register):

.. code:: C

    # espressif__tinyusb should match the current tinyusb dependency name
    idf_component_get_property(tusb_lib espressif__tinyusb COMPONENT_LIB)

    target_include_directories(${tusb_lib} PUBLIC "${COMPONENT_DIR}/tusb")
    target_sources(${tusb_lib} PUBLIC "${COMPONENT_DIR}/tusb/usb_descriptors.c")

.. note::
    Regarding reverse dependency issues, since the project needs to depend on tinyusb and provide header files to tinyusb, it inevitably leads to reverse dependency problems. Currently, this can be resolved by directly compiling all key files of tinyusb into the main component.

tusb_config.h File
~~~~~~~~~~~~~~~~~~~

Most of tinyusb's functionalities are controlled through macros, so we need to declare the required functionalities in the ``tusb_config.h`` file. Here are some key macros:

**System Settings Macros**:

- **CFG_TUSB_RHPORT0_MODE**: Defines the connection method and speed to the USB Phy. The following method indicates a USB device with USB full speed.

    .. code:: C

        #define CFG_TUSB_RHPORT0_MODE    (OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED)

- **CFG_TUSB_RHPORT1_MODE**: Defines the connection method and speed to the USB Phy. The following method indicates a USB device with USB high speed.

    .. code:: C

        #define CFG_TUSB_RHPORT1_MODE    (OPT_MODE_DEVICE | OPT_MODE_HIGH_SPEED)

- **ESP_PLATFORM**: This macro needs to be enabled when using the esp-idf platform for compilation.

- **CFG_TUSB_OS**: Defines the operating system for tinyusb. If using FreeRTOS, this macro needs to be enabled. It can also be disabled if not using an OS.

    .. code:: C

        #define CFG_TUSB_OS           OPT_OS_FREERTOS

- **CFG_TUSB_OS_INC_PATH**: In ESP-IDF, it requires adding the "freertos/" prefix in the include path.

    .. code:: C

        #define CFG_TUSB_OS_INC_PATH   freertos/

- **CFG_TUSB_DEBUG**: Enables the LOG print level of tinyusb. There are three levels in total.

    .. code:: C

        #define CFG_TUSB_DEBUG         0

- **CFG_TUSB_DEBUG_PRINTF**: Defines the log printing function for tinyusb.

    .. code:: C

        #define CFG_TUSB_DEBUG_PRINTF      esp_rom_printf 

- **CFG_TUD_ENABLED**: Set to 1 to enable tinyusb device functionality.

- **CFG_TUSB_MEM_SECTION**: This macro can be enabled to allocate tinyusb memory to a specific memory section.

- **CFG_TUSB_MEM_ALIGN**: Defines the memory alignment method.

    .. code:: C

        #define CFG_TUSB_MEM_ALIGN      __attribute__ ((aligned(4)))

**USB Device Macros**:

- **CFG_TUD_ENDPOINT0_SIZE**: Defines the maximum packet size for endpoint 0.

**USB Class Macros**:

Here, using the UVC Class as an example, each USB Class has its own macros:

- **CFG_TUD_VIDEO**: Configures the number of video control interfaces.

- **CFG_TUD_VIDEO_STREAMING**: Configures the number of video streaming interfaces.

Refer to the following file examples:

- :example_file:`../components/usb/usb_device_uac/tusb/tusb_config.h`
- :example_file:`../components/usb/usb_device_uvc/tusb/tusb_config.h`
- :example_file:`/usb/device/usb_hid_device/hid_device/tusb_config.h`

usb_descriptors.h File (Optional)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This file is mainly used to place custom USB descriptors. Tinyusb provides many descriptor templates, but if they do not meet your needs, you need to define your own set of USB descriptors. Note that it is best to use the predefined descriptors in tinyusb, as it makes descriptor assembly and length calculation more convenient.

Refer to the following file examples:

- :example_file:`../components/usb/usb_device_uac/tusb_uac/uac_descriptors.h`
- :example_file:`../components/usb/usb_device_uvc/tusb/usb_descriptors.h`
- :example_file:`/usb/device/usb_hid_device/hid_device/usb_descriptors.h`

usb_descriptors.c File
~~~~~~~~~~~~~~~~~~~~~~~

This file primarily implements several weak functions for obtaining descriptors, such as getting the device descriptor, configuration descriptor, and string descriptor.

.. code:: C

    uint8_t const *tud_descriptor_device_cb(void);

    uint8_t const *tud_descriptor_configuration_cb(uint8_t index);

    uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid);

Points to Note:

- The length of the configuration descriptor must equal the actual length.
- The endpoint numbers used in the configuration descriptor's endpoint descriptors must not overlap.

Refer to the following file examples:

- :example_file:`../components/usb/usb_device_uvc/tusb/usb_descriptors.c`
- :example_file:`../components/usb/usb_device_uac/tusb/usb_descriptors.c`
- :example_file:`/usb/device/usb_hid_device/hid_device/usb_descriptors.c`

Initializing USB Phy
~~~~~~~~~~~~~~~~~~~~

To initialize the internal USB Phy:

.. code:: C

    static void usb_phy_init(void)
    {
        // Configure USB PHY
        usb_phy_config_t phy_conf = {
            .controller = USB_PHY_CTRL_OTG,
            .otg_mode = USB_OTG_MODE_DEVICE,
            .target = USB_PHY_TARGET_INT,
        };
        usb_new_phy(&phy_conf, &s_uvc_device.phy_hdl);
    }

If using an external USB Phy, refer to :ref:`external_phy`.

Initializing the TinyUSB Stack
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Use the following code:

.. code:: C

    static void tusb_device_task(void *arg)
    {
        while (1) {
            tud_task();
        }
    }

    int main(void) {
        usb_phy_init();
        bool usb_init = tusb_init();
        if (!usb_init) {
            ESP_LOGE(TAG, "USB Device Stack Init Fail");
            return ESP_FAIL;
        }
        xTaskCreatePinnedToCore(tusb_device_task, "TinyUSB", 4096, NULL, 5, NULL, 0);
    }

Device-Level Weak Functions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

These functions allow you to handle events such as device insertion, removal, suspension, and resumption.

.. code:: C

    // Invoked when the device is mounted
    void tud_mount_cb(void)
    {
    }

    // Invoked when the device is unmounted
    void tud_umount_cb(void)
    {
    }

    // Invoked when the device is suspended
    void tud_suspend_cb(bool remote_wakeup_en)
    {
    }

    // Invoked when the USB bus is resumed
    void tud_resume_cb(void)
    {
    }

Implementing USB Class-Specific Callback Functions.
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

USB classes provide some weak functions to complete basic functions. Taking the UVC driver as an example, the source file is `video_device <https://github.com/hathach/tinyusb/blob/master/src/class/video/video_device.h>`.

By observing the API, it can be seen that the UVC Class provides two functions and one callback function:

.. code:: C

    bool tud_video_n_streaming(uint_fast8_t ctl_idx, uint_fast8_t stm_idx);

    bool tud_video_n_frame_xfer(uint_fast8_t ctl_idx, uint_fast8_t stm_idx, void *buffer, size_t bufsize);

    TU_ATTR_WEAK void tud_video_frame_xfer_complete_cb(uint_fast8_t ctl_idx, uint_fast8_t stm_idx);

The ``tud_video_n_frame_xfer`` function is used to transfer a frame of image, and the ``tud_video_frame_xfer_complete_cb`` callback is used to check if the transfer is complete.

Additionally, different USB classes have special macro definitions to define software FIFO sizes or enable certain features. For example, the macro ``CFG_TUD_VIDEO_STREAMING_EP_BUFSIZE`` in the UVC Class is used to define the buffer size of the video streaming interface endpoint.
