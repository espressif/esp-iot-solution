i2c_bus
==========

:link_to_translation:`en:[English]`

i2c_bus 的使用方法：
---------------------

1. 创建总线：使用 :cpp:func:`i2c_bus_create` 创建一个总线实例。创建时需要指定 I2C 端口号，以及总线配置项 ``i2c_config_t``。配置项包括 SDA 和 SCL 引脚号、上下拉模式，因为这些配置项在系统设计时已经确定，一般不在运行时切换。总线配置项还包括总线默认的时钟频率，在设备不指定频率时使用。
2. 创建设备：使用 :cpp:func:`i2c_bus_device_create` 在已创建的总线实例之上创建设备，创建时需要指定总线句柄、设备的 I2C 地址、设备运行的时钟频率，I2C 传输时将根据设备的配置项动态切换频率。设备时钟速率可配置为 0，表示默认使用当前的总线频率。
3. 数据读取：使用 :cpp:func:`i2c_bus_read_byte`、:cpp:func:`i2c_bus_read_bytes` 可直接进行 ``Byte`` 的读取操作；使用 :cpp:func:`i2c_bus_read_bit`、:cpp:func:`i2c_bus_read_bits` 可直接进行 ``bit`` 的读取操作。只需要传入设备句柄、设备寄存器地址、用于存放读取数据的 buf 和读取长度的等。寄存器地址可设为 ``NULL_I2C_MEM_ADDR``，用于操作没有内部寄存器的设备。
4. 数据写入：使用 :cpp:func:`i2c_bus_write_byte`、:cpp:func:`i2c_bus_write_bytes` 可直接进行 ``Byte`` 的写入操作；使用 :cpp:func:`i2c_bus_write_bit`、:cpp:func:`i2c_bus_write_bits` 可直接进行 ``bit`` 的写入操作。只需要传入设备句柄、设备寄存器地址、将要写入的数据位置和写入长度等。寄存器地址可设为 ``NULL_I2C_MEM_ADDR``，用于操作没有内部寄存器的设备。
5. 删除设备和总线：如果所有的 i2c_bus 通信已经完成，可以通过删除设备和总线实例释放系统资源。可使用 :cpp:func:`i2c_bus_device_delete` 分别将已创建的设备删除，然后使用 :cpp:func:`i2c_bus_delete` 将总线资源删除。如果在设备未删除时删除总线，操作将不会被执行。

示例：

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

    对于某些特殊应用场景，例如:

    1. 当寄存器地址为 16 位时，可以使用 :cpp:func:`i2c_bus_read_reg16` 或 :cpp:func:`i2c_bus_write_reg16` 进行读写操作；
    2. 对于需要跳过地址阶段或者需要增加命令阶段的设备，可以使用 :cpp:func:`i2c_bus_cmd_begin` 结合 `I2C command link <https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-reference/peripherals/i2c.html?highlight=i2c#communication-as-master>`_ 进行操作。
    3. 对于 I2C 端口不足或者需要软件 I2C 调试的场景下，可以使用 ``menuconfig`` 在 ``(Top) → Component config → Bus Options → I2C Bus Options`` 中开启 ``Enable software I2C support``，并在 :cpp:func:`i2c_bus_create` 的 ``port`` 中传入 ``i2c_sw_port_t`` 类型的 ``port``，例如 ``I2C_NUM_SW_0``。

已适配 IDF 版本
-----------------

- ESP-IDF v4.0 及以上版本。

API 参考
-----------

.. include-build-file:: inc/i2c_bus.inc
