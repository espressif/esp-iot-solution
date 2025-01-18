i2c_bus
==========

:link_to_translation:`zh_CN:[中文]`

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
    3. For scenarios where I2C ports are insufficient or software I2C debugging is required, you can enable ``Enable software I2C support`` in ``menuconfig`` under ``(Top) → Component config → Bus Options → I2C Bus Options``. Then, pass a ``port`` of type ``i2c_sw_port_t`` (e.g., ``I2C_NUM_SW_0``) to the ``port`` parameter in the :cpp:func:`i2c_bus_create` function.

Adapted IDF Versions
---------------------------

- ESP-IDF v4.0 and later versions.

API Reference
----------------------

.. include-build-file:: inc/i2c_bus.inc
