Communication Bus
===================

:link_to_translation:`zh_CN:[中文]`

The communication bus component (Bus) is a set of application-layer code built on top of the ESP-IDF peripheral driver code, including ``i2c_bus``, ``spi_bus`` and etc. It is mainly used for bus communication between ESP chips and external devices. From the point of application development, this component has the following features:

1. Simplified peripheral initialization processes
2. Thread-safe device operations
3. Simple and flexible RW operations

This component abstracts the following concepts:

1. Bus: the resource and configuration option shared between devices during communication
2. Device: device specific resource and configuration option during communication

Each physical peripheral bus can mount one or more devices if the electrical condition allows, with the SPI bus addressing devices based on CS pins and the I2C bus addressing devices based on their addresses, thus achieving software independence between different devices on the same bus.


.. figure:: ../../_static/i2c_bus.png
    :align: center
    :width: 75%

    i2c_bus Connection Diagram

.. figure:: ../../_static/spi_bus.png
    :align: center
    :width: 73%

    spi_bus Connection Diagram

How to Use i2c_bus 
---------------------------

1. Create a bus: create a bus object using :cpp:func:`i2c_bus_create`. During the process, you need to specify the I2C port number and the bus configuration option ``i2c_config_t``, which includes SDA and SCL pin numbers, pull-up and pull-down modes, as these are determined when the system is designed and normally will not be changed at runtime. The bus configuration option also includes the default clock frequency of the bus, which is used when the device does not specify a frequency.
2. Create a device: use :cpp:func:`i2c_bus_device_create` to create a device on the bus object created in the first step. During the process, you need to specify bus handle, the I2C address of the device, and the clock frequency when the device is running. The frequency will be dynamically changed during I2C transmission based on device configuration options. The device clock frequency can be configured as 0, indicating the current bus frequency is used by default.
3. Data reading: use :cpp:func:`i2c_bus_read_byte` or :cpp:func:`i2c_bus_read_bytes` to read ``Byte`` data; use :cpp:func:`i2c_bus_read_bit` or :cpp:func:`i2c_bus_read_bits` to read ``bit`` data. During the process, you only need to pass in the device handle, the device register address, a buffer to hold the read data, the read length, etc. The register address can be configured as ``NULL_I2C_MEM_ADDR`` for devices without internal registers.
4. Data writing: use :cpp:func:`i2c_bus_write_byte` or :cpp:func:`i2c_bus_write_bytes` to write ``Byte`` data; use :cpp:func:`i2c_bus_write_bit` or :cpp:func:`i2c_bus_write_bits` to write ``bit`` data. During the process, you only need to pass in the device handle, the device register address, the data location to be written, the write length, etc. The register address can be configured as ``NULL_I2C_MEM_ADDR`` for devices without internal registers.
5. Delete device and bus: if all i2c_bus communication has been completed, you can free your system resources by deleting devices and bus objects. Use :cpp:func:`i2c_bus_device_delete` to delete created devices respectively, then use :cpp:func:`i2c_bus_delete` to delete bus resources. If the bus is deleted with the device not being deleted yet, this operation will not take effect.

Example:

.. code:: c

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    }; // i2c_bus configurations

    uint8_t data_rd[2] = {0};
    uint8_t data_wr[2] = {0x01, 0x21};

    i2c_bus_handle_t i2c0_bus = i2c_bus_create(I2C_NUM_0, &conf); // create i2c_bus
    i2c_bus_device_handle_t i2c_device1 = i2c_bus_device_create(i2c0_bus, 0x28, 400000); // create device1, address: 0x28 , clk_speed: 400000
    i2c_bus_device_handle_t i2c_device2 = i2c_bus_device_create(i2c0_bus, 0x32, 0); // create device2, address: 0x32 , clk_speed: no-specified

    i2c_bus_read_bytes(i2c_device1, NULL_I2C_MEM_ADDR, 2, data_rd); // read bytes from device1 with no register address
    i2c_bus_write_bytes(i2c_device2, 0x10, 2, data_wr); // write bytes to device2 register 0x10

    i2c_bus_device_delete(&i2c_device1); //delete device1
    i2c_bus_device_delete(&i2c_device2); //delete device2
    i2c_bus_delete(&i2c0_bus);  //delete i2c_bus

.. note::

    For some special application scenarios:

    1. When the address of a register is 16-bit, you can use :cpp:func:`i2c_bus_read_reg16` or :cpp:func:`i2c_bus_write_reg16` to read or write its data;
    2. For devices that need to skip the address phase or need to add a command phase, you can operate using :cpp:func:`i2c_bus_cmd_begin` combined with `I2C command link <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/i2c.html?highlight=i2c#communication-as-master>`_.

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

Supported Chips
---------------------

-  ESP32
-  ESP32-S2

API Reference
----------------------

i2c_bus API Reference
++++++++++++++++++++++++++++

.. include:: /_build/inc/i2c_bus.inc

spi_bus API Reference
+++++++++++++++++++++++++++++++

.. include:: /_build/inc/spi_bus.inc
