spi_bus
==========

:link_to_translation:`en:[English]`

spi_bus 的使用方法
----------------------

1. 创建总线：使用 :cpp:func:`spi_bus_create` 创建一个总线实例，创建时需要指定 SPI 端口号（可选 ``SPI2_HOST``、``SPI3_HOST``）以及总线配置项 ``spi_config_t``。配置项包括 ``MOSI``、``MISO``、``SCLK`` 引脚号，因为这些引脚在系统设计时已经确定，一般不在运行时切换。总线配置项还包括 ``max_transfer_sz``，用于配置一次传输时的最大数据量，设置为 0 将使用默认值 4096。
2. 创建设备：使用 :cpp:func:`spi_bus_device_create` 在已创建的总线实例之上创建设备，创建时需要指定总线句柄、设备的 ``CS`` 引脚号、设备运行模式、设备运行的时钟频率，SPI 传输时将根据设备的配置项动态切换模式和频率。
3. 数据传输：使用 :cpp:func:`spi_bus_transfer_byte`、:cpp:func:`spi_bus_transfer_bytes`、:cpp:func:`spi_bus_transfer_reg16` 以及 :cpp:func:`spi_bus_transfer_reg32` 可直接进行数据的传输操作。由于 SPI 是全双工通信，因此每次传输发送和接收可以同时进行，只需要传入设备句柄、待发送数据、存放读取数据的 buf 和传输长度。
4. 删除设备和总线：如果所有的 spi_bus 通信已经完成，可以通过删除设备和总线实例释放系统资源。可使用 :cpp:func:`spi_bus_device_delete` 分别将已创建的设备删除，然后使用 :cpp:func:`spi_bus_delete` 将总线资源删除。如果在设备未删除时删除总线，操作将不会被执行。

示例：

.. code:: c

    spi_bus_handle_t bus_handle = NULL;
    spi_bus_device_handle_t device_handle = NULL;
    uint8_t data8_in = 0;
    uint8_t data8_out = 0xff;
    uint16_t data16_in = 0;
    uint32_t data32_in = 0;

    spi_config_t bus_conf = {
        .miso_io_num = 19,
        .mosi_io_num = 23,
        .sclk_io_num = 18,
    }; // spi_bus configurations

    spi_device_config_t device_conf = {
        .cs_io_num = 19,
        .mode = 0,
        .clock_speed_hz = 20 * 1000 * 1000,
    }; // spi_device configurations

    bus_handle = spi_bus_create(SPI2_HOST, &bus_conf); // create spi bus
    device_handle = spi_bus_device_create(bus_handle, &device_conf); // create spi device

    spi_bus_transfer_bytes(device_handle, &data8_out, &data8_in, 1); // transfer 1 byte with spi device
    spi_bus_transfer_bytes(device_handle, NULL, &data8_in, 1); // only read 1 byte with spi device
    spi_bus_transfer_bytes(device_handle, &data8_out, NULL, 1); // only write 1 byte with spi device
    spi_bus_transfer_reg16(device_handle, 0x1020, &data16_in); // transfer 16-bit value with the device
    spi_bus_transfer_reg32(device_handle, 0x10203040, &data32_in); // transfer 32-bit value with the device

    spi_bus_device_delete(&device_handle);
    spi_bus_delete(&bus_handle);

.. note::

    对于某些特殊应用场景，可以直接使用 :cpp:func:`spi_bus_transmit_begin` 结合 `spi_transaction_t <https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s2/api-reference/peripherals/spi_master.html?highlight=spi_transaction_t#_CPPv417spi_transaction_t>`_ 进行操作。


已适配 IDF 版本
-----------------

- ESP-IDF v4.0 及以上版本。

API 参考
-----------

.. include-build-file:: inc/spi_bus.inc
