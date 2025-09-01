
自供电 USB 设备解决方案
-----------------------

:link_to_translation:`en:[English]`

按照 USB 协议要求，USB 自供电设备必须通过检测 5V VBUS 电压来判断设备是否拔出，进而实现热插拔。对于主机供电设备，由于主机 VBUS 断电之后，设备直接掉电关机，无需实现该逻辑。

USB 设备 VBUS 检测方法一般有两种方法：由 USB PHY 硬件检测，或\ **借助 ADC/GPIO 由软件检测**\ 。

由于 ESP32S2/S3/P4 内部 USB PHY 不支持硬件检测逻辑，该功能需要借助 ADC/GPIO 由软件实现，其中使用 GPIO 检测方法最为简便，实现方法如下：

**对于 ESP-IDF 4.4 及更早版本:**


#. 硬件上，需要额外占用一个 IO（任意指定，特殊引脚除外），通过两个电阻分压（例如两个 100KΩ ）与 ESP32S2/S3 相连 (ESP32S2/S3 IO 最大可输入电压为 3.3v)；
#. 在 ``tinyusb_driver_install`` 之后，需要调用 ``usbd_vbus_detect_gpio_enable`` 函数使能 VBUS 检测，该函数实现代码如下，请直接复制到需要调用的位置：

.. code-block:: C

   /**
    *
    * @brief For USB Self-power device, the VBUS voltage must be monitored to achieve hot-plug,
    *        The simplest solution is detecting GPIO level as voltage signal.
    *        A divider resistance Must be used due to ESP32S2/S3/P4 has no 5V tolerate pin.
    *
    *   5V VBUS ┌──────┐    ┌──────┐   GND
    *    ───────┤ 100K ├─┬──┤ 100K ├──────
    *           └──────┘ │  └──────┘
    *                    │           GPIOX
    *                    └───────────────
    *        The API Must be Called after tinyusb_driver_install to overwrite the default config.
    * @param gpio_num, The gpio number used for vbus detect
    * 
    */
   static void usbd_vbus_detect_gpio_enable(int gpio_num)
   {
       gpio_config_t io_conf = {
           .intr_type = GPIO_INTR_DISABLE,
           .pin_bit_mask = (1ULL<<gpio_num),
           //set as input mode
           .mode = GPIO_MODE_INPUT,
           .pull_up_en = 0,
           .pull_down_en = 0,
       };
       gpio_config(&io_conf);
       esp_rom_gpio_connect_in_signal(gpio_num, USB_OTG_VBUSVALID_IN_IDX, 0); 
       esp_rom_gpio_connect_in_signal(gpio_num, USB_SRP_BVALID_IN_IDX, 0); 
       esp_rom_gpio_connect_in_signal(gpio_num, USB_SRP_SESSEND_IN_IDX, 1); 
       return;
   }

**对于 ESP-IDF 5.0 及以上版本:**

#. 同上，硬件上需要额外占用一个 IO（任意指定，特殊引脚除外），通过两个电阻分压（例如两个 100KΩ ）与 ESP32S2/S3 相连；
#. 将用于检测 VBUS 的 IO 初始化为 GPIO 输入模式;
#. 直接将 IO 配置到 ``tinyusb_config_t`` 中（\ `详情可参考 <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/usb_device.html#self-powered-device>`_\ ）：

.. code-block:: C

       #define VBUS_MONITORING_GPIO_NUM GPIO_NUM_4
       const tinyusb_config_t tusb_cfg = {
           .device_descriptor = &descriptor_config,
           .string_descriptor = string_desc_arr,
           .string_descriptor_count = sizeof(string_desc_arr) / sizeof(string_desc_arr[0]),
           .external_phy = false,
           .configuration_descriptor = desc_configuration,
           .self_powered = true,
           .vbus_monitor_io = VBUS_MONITORING_GPIO_NUM,
       };
       ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

若使用原生 TinyUSB 开发，则需要在 Phy 初始化阶段进行配置：

.. code-block:: C

    const usb_phy_otg_io_conf_t otg_io_conf = USB_PHY_SELF_POWERED_DEVICE(VBUS_MONITORING_GPIO_NUM);
    phy_conf.otg_io_conf = &otg_io_conf;
    usb_new_phy(&phy_conf, &s_phy_hdl);
