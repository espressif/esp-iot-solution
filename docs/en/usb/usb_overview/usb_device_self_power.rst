
Self-Powered USB Device Solutions
----------------------------------

:link_to_translation:`zh_CN:[中文]`

According to the USB protocol requirements, self-powered USB devices must detect the 5V VBUS voltage to determine if the device is unplugged, thereby enabling hot-plugging. For host-powered devices, since the device shuts down immediately when the host VBUS is disconnected, there is no need to implement this logic.

There are generally two methods for USB device VBUS detection: detection by USB PHY hardware, or \ **detection by software with the help of ADC/GPIO**\.

The internal USB PHY of ESP32S2/S3/P4 does not support hardware detection logic, this function needs to be implemented by software with the help of ADC/GPIO. Among them, using the GPIO detection method is the simplest. The implementation is as follows:

**For ESP-IDF 4.4 and earlier versions:**


#. On the hardware side, you need to allocate an additional I/O (except for special pins) connected to ESP32S2/S3 through a voltage divider with two resistors (e.g., two 100KΩ). (Note: ESP32S2/S3 I/O maximum input voltage is 3.3v.)
#. After the ``tinyusb_driver_install``, it is necessary to call the ``usbd_vbus_detect_gpio_enable`` function to enable VBUS detection. The implementation code for this function is as follows. Please copy it to the required location for invocation:

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

**For ESP-IDF 5.0 and later versions:**

#. Same as above, the hardware needs to occupy an additional IO (arbitrarily specified, except for special pins), which is connected to ESP32S2/S3 through two resistor voltage dividers (for example, two 100KΩ);
#. Initialize the IO used to detect VBUS to GPIO input mode;
#. Configure IO directly into ``tinyusb_config_t`` (\ `For details, please refer to <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/usb_device. html#self-powered-device>`_\ ):

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

If you are using native TinyUSB development, you need to configure this during the PHY initialization stage:

.. code-block:: C

    const usb_phy_otg_io_conf_t otg_io_conf = USB_PHY_SELF_POWERED_DEVICE(VBUS_MONITORING_GPIO_NUM);
    phy_conf.otg_io_conf = &otg_io_conf;
    usb_new_phy(&phy_conf, &s_phy_hdl);
