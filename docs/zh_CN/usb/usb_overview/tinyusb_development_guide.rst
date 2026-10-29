使用原生的 tinyusb 进行开发
-----------------------------

:link_to_translation:`en:[English]`

本指南包含以下内容：

.. contents:: 目录
    :local:
    :depth: 2

本篇将会介绍如何使用 tinyusb 的组件进行开发。

工程目录
~~~~~~~~~~~

首先需要建立以下目录结构：

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

在该工程的 main 组件中添加组件依赖，:ref:`espressif/tinyusb <espressif/tinyusb>`。

tusb 文件夹主要放置用于反向提供给 tinyusb 的文件，通过单独放置一个文件夹中，保证依赖关系的简单。然后需要在 ``main/CMakeLists.txt`` 文件中添加以下语句（放置于 idf_component_register 之后）

.. code:: C

    # espressif__tinyusb 应匹配当前依赖的 tinyusb 名称
    idf_component_get_property(tusb_lib espressif__tinyusb COMPONENT_LIB)

    target_include_directories(${tusb_lib} PUBLIC "${COMPONENT_DIR}/tusb")
    target_sources(${tusb_lib} PUBLIC "${COMPONENT_DIR}/tusb/usb_descriptors.c")

.. note::
    关于反向依赖问题，因为工程需要依赖 tinyusb，有需要向 tinyusb 提供头文件，所以不可避免的会导致反向依赖的问题。目前可以通过将 tinyusb 的全部关键文件作为源码直接编译到 main 组件中来解决。

tusb_config.h 文件
~~~~~~~~~~~~~~~~~~~~~~

tinyusb 大部分的功能的启用和关闭都是通过宏来控制，所以需要在 ``tusb_config.h`` 文件中声明所需要的功能。下面列出一些关键的宏：

**系统设置的宏**：

- **CFG_TUSB_RHPORT0_MODE**：用于定义连接到 USB Phy 的方式和速率，下面的方式表示是 USB device 设备，并且速率为 USB 全速。

    .. code:: C

        #define CFG_TUSB_RHPORT0_MODE    (OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED)

- **CFG_TUSB_RHPORT1_MODE**：用于定义连接到 USB Phy 的方式和速率，下面的方式表示是 USB device 设备，并且速率为 USB 高速。

    .. code:: C

        #define CFG_TUSB_RHPORT1_MODE    (OPT_MODE_DEVICE | OPT_MODE_HIGH_SPEED)

- **ESP_PLATFORM**：使用 esp-idf 平台进行编译，需启用该宏。

- **CFG_TUSB_OS**：用于定义 tinyusb 的操作系统，如果使用的是 FreeRTOS，需要启用该宏。也可以不启用操作系统

    .. code:: C

        #define CFG_TUSB_OS           OPT_OS_FREERTOS

- **CFG_TUSB_OS_INC_PATH**：在 ESP-IDF 中要求需要添加 "freertos/" 前缀在 include 路径中。

    .. code:: C

        #define CFG_TUSB_OS_INC_PATH   freertos/

- **CFG_TUSB_DEBUG**：用于启用 tinyusb 的 LOG 打印等级。总共三级

    .. code:: C

        #define CFG_TUSB_DEBUG         0

- **CFG_TUSB_DEBUG_PRINTF**: 用于定义 tinyusb 的 log 打印函数。

    .. code:: C

        #define CFG_TUSB_DEBUG_PRINTF      esp_rom_printf 

- **CFG_TUD_ENABLED**：设为 1 启用 tinyusb device 功能。

- **CFG_TUSB_MEM_SECTION**：通过启用该宏，可以将 tinyusb 的内存分配到特定的内存段中。

- **CFG_TUSB_MEM_ALIGN**：用于定义内存对齐方式。

    .. code:: C

        #define CFG_TUSB_MEM_ALIGN      __attribute__ ((aligned(4)))

**USB 设备的宏**：

- **CFG_TUD_ENDPOINT0_SIZE**：用于定义端点 0 的最大包大小。

**USB Class 的宏**：

这里以 UVC Class 举例，每一个 USB Class 都有单独的宏定义:

- **CFG_TUD_VIDEO**：配置视频控制接口（video control interface）的数量

- **CFG_TUD_VIDEO_STREAMING**：配置视频流接口（video streaming interface）的数量

可以参考以下文件示例：

- :example_file:`../components/usb/usb_device_uac/tusb/tusb_config.h`
- :example_file:`../components/usb/usb_device_uvc/tusb/tusb_config.h`
- :example_file:`/usb/device/usb_hid_device/hid_device/tusb_config.h`

usb_descriptors.h 文件（可选）
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

该文件主要用来放置自定义的 USB 描述符。tinyusb 提供了很多描述符的模板，如果不满足需求，就需要自己定义一套 USB 描述符。需要注意的是尽量使用 tinyusb 中预定义好的一些描述符，这样可以很方便的进行描述符组装和计算长度。

可以参考以下文件示例：

- :example_file:`../components/usb/usb_device_uac/tusb_uac/uac_descriptors.h`
- :example_file:`../components/usb/usb_device_uvc/tusb/usb_descriptors.h`
- :example_file:`/usb/device/usb_hid_device/hid_device/usb_descriptors.h`

usb_descriptors.c 文件
~~~~~~~~~~~~~~~~~~~~~~~~

该文件主要实现了几个获取描述符的弱函数，分别是获取设备描述符，或者配置描述符和获取字符串描述符。

.. code:: C

    uint8_t const *tud_descriptor_device_cb(void);

    uint8_t const *tud_descriptor_configuration_cb(uint8_t index);

    uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid);

注意点：

- 配置描述符的长度一定要等于实际的长度
- 配置描述符使用的各个端点描述符的端点号要避免重复

可以参考以下文件示例：

- :example_file:`../components/usb/usb_device_uvc/tusb/usb_descriptors.c`
- :example_file:`../components/usb/usb_device_uac/tusb/usb_descriptors.c`
- :example_file:`/usb/device/usb_hid_device/hid_device/usb_descriptors.c`

初始化 USB Phy
~~~~~~~~~~~~~~~~~

初始化内部 USB Phy:

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

如果使用外部 USB Phy， 参考 :ref:`external_phy`

初始化 TinyUSB 协议栈
~~~~~~~~~~~~~~~~~~~~~~~~~

使用以下的代码

.. code:: c

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

实现设备层的弱函数
~~~~~~~~~~~~~~~~~~~

可以获取设备的插入，拔出，暂停，恢复等事件。

.. code:: C

    // Invoked when device is mounted
    void tud_mount_cb(void)
    {
    }

    // Invoked when device is unmounted
    void tud_umount_cb(void)
    {
    }

    // Invoked when device is suspended
    void tud_suspend_cb(bool remote_wakeup_en)
    {
    }

    // Invoked when usb bus is resumed
    void tud_resume_cb(void)
    {
    }

实现 USB Class 的特殊的回调函数。
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

USB Class 提供了一些弱函数来完成基本的功能，接下来会以 UVC 驱动为例。源码文件 `video device <https://github.com/hathach/tinyusb/blob/master/src/class/video/video_device.h>`_

通过观察 API 可以发现 UVC Class 提供了两个函数和一个回调函数，

.. code:: C

    bool tud_video_n_streaming(uint_fast8_t ctl_idx, uint_fast8_t stm_idx);

    bool tud_video_n_frame_xfer(uint_fast8_t ctl_idx, uint_fast8_t stm_idx, void *buffer, size_t bufsize);

    TU_ATTR_WEAK void tud_video_frame_xfer_complete_cb(uint_fast8_t ctl_idx, uint_fast8_t stm_idx);

通过调用 ``tud_video_n_frame_xfer`` 函数来传输一帧图像，并通过 ``tud_video_frame_xfer_complete_cb`` 来检查是否传输完成。

此外不同的 USB Class 还会有一些特殊的宏定义，用于定义软件 fifo 大小或者启用一些功能。比如 UVC Class 中的宏 ``CFG_TUD_VIDEO_STREAMING_EP_BUFSIZE`` 用于定义视频传输流（video streaming interface）端点的 buffer 的大小。
