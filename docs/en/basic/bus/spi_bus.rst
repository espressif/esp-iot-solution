spi_bus
==========

:link_to_translation:`zh_CN:[中文]`

How to Use spi_bus
--------------------------

1. Create a bus: use :cpp:func:`spi_bus_create` to create a bus object. During the process, you need to specify the SPI port number (can choose between ``SPI2_HOST`` and ``SPI3_HOST``) and the bus configuration option ``spi_config_t``, which includes the pin numbers of ``MOSI``, ``MISO`` and ``SCLK``, as these are determined when the system is designed and normally will not be changed at runtime. The bus configuration option also includes ``max_transfer_sz`` to configure the maximum data size during a transmission. When ``max_transfer_sz`` is configured to 0, it means the maximum size will be the default value 4096.
2. Create a device: use :cpp:func:`spi_bus_device_create` to create a device on the bus object created in the first step. During the process, you need to specify the bus handle, the ``CS`` pin number of the device, device operation mode, the clock frequency when the device is running. The device mode and frequency will be dynamically changed during SPI transmissions based on device configuration options.
3. Data transmission: use :cpp:func:`spi_bus_transfer_byte`, :cpp:func:`spi_bus_transfer_bytes`, :cpp:func:`spi_bus_transfer_reg16` or :cpp:func:`spi_bus_transfer_reg32` to transfer data directly. Data send and receive can be operated at the same time since SPI communication is a full-duplex communication. During the process, you only need to pass in the device handle, data to be transmitted, a buffer to hold the read data, transmission length, etc.
4. Delete device and bus: if all spi_bus communication has been completed, you can free your system resources by deleting devices and bus objects. Use :cpp:func:`spi_bus_device_delete` to delete created devices respectively, then use :cpp:func:`spi_bus_delete` to delete bus resources. If the bus is deleted with the device not being deleted yet, this operation will not take effect.

Example:

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

    For some special application scenarios, you can operate using :cpp:func:`spi_bus_transmit_begin` combined with `spi_transaction_t <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/api-reference/peripherals/spi_master.html?highlight=spi_transaction_t#_CPPv417spi_transaction_t>`_ directly.


Adapted IDF Versions
---------------------------

- ESP-IDF v4.0 and later versions.

API Reference
----------------------

.. include-build-file:: inc/spi_bus.inc
