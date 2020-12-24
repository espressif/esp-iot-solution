Bus 组件
=================

总线组件（bus）是建立在 ESP-IDF 外设驱动代码之上的一套应用层代码，包括
``i2c_bus``\ 、\ ``spi_bus`` 等，主要用于 ESP
芯片与外置设备之间的总线通信。该组件从应用开发的角度出发，实现了以下功能：

1. 简化了外设初始化步骤
2. 实现线程安全的设备操作
3. 实现简单灵活的读写操作

该组件对以下概念进行了抽象：

1. 总线（Bus）：通信时，设备之间共同拥有的资源和配置项
2. 设备（Device）：通信时，设备特有的资源和配置项

每个物理外设总线（bus）在电气条件允许的情况下，均可挂载一到多个设备（device），其中
SPI 总线根据 CS 引脚对设备进行寻址，I2C
总线则根据设备地址进行寻址，进而实现相同总线不同设备之间软件上的独立。

.. figure:: ../../_static/i2c_bus.png
    :align: center
    :width: 75%

    i2c_bus connection diagram

.. figure:: ../../_static/spi_bus.png
    :align: center
    :width: 73%

    spi_bus connection diagram

i2c\_bus 使用方法
-----------------

1. 创建总线：使用 ``i2c_bus_create`` 创建一个总线实例，创建时需要指定
   I2C 端口号，以及总线配置项 ``i2c_config_t``\ 。配置项包括 SDA 和 SCL
   引脚号、上下拉模式，因为这些配置项在系统设计时已经确定，一般不在运行时切换。总线配置项还包括总线默认的时钟频率，在设备不指定频率时使用。
2. 创建设备：使用 ``i2c_bus_device_create``
   在已创建的总线实例之上创建设备，创建时需要指定总线句柄、设备的 i2c
   地址、设备运行的时钟频率，i2c
   传输时将根据设备的配置项动态切换频率。设备时钟速率可配置为0
   ，将默认使用当前的总线频率。
3. 数据读取：使用 ``i2c_bus_read_byte`` ``i2c_bus_read_bytes``
   ``i2c_bus_read_bit`` ``i2c_bus_read_bits``
   可直接进行数据的读取操作，只需要传入设备句柄、设备寄存器地址、用于存放读取数据
   buf 和读取长度的等。寄存器地址可设为
   ``NULL_I2C_MEM_ADDR``\ ，用于操作没有内部寄存器的设备。
4. 数据写入：使用 ``i2c_bus_write_byte`` ``i2c_bus_write_bytes``
   ``i2c_bus_write_bit`` ``i2c_bus_write_bits``
   可直接进行数据的写入操作，只需要传入设备句柄、设备寄存器地址、将要写入的数据位置和写入长度等。寄存器地址可设为
   ``NULL_I2C_MEM_ADDR``\ ，用于操作没有内部寄存器的设备。
5. 删除设备和总线：如果所有的 i2c\_bus
   通信已经完成，可以通过删除设备和总线实例释放系统资源。可使用
   ``i2c_bus_device_delete`` 分别将已创建的设备删除，然后使用
   ``i2c_bus_delete``
   将总线资源删除。如果在设备未删除时删除总线，操作将不会被执行。

i2c\_bus 附加功能
-----------------

1. 总线扫描：可使用 ``i2c_bus_scan`` 扫描并保存总线上的有效地址；
2. 总线状态获取：可通过 ``i2c_bus_get_current_clk_speed``
   获取当前总线的有效频率。可通过\ ``i2c_bus_get_created_device_num``
   获取当前总线上已经创建的设备总数。

spi\_bus 使用方法
-----------------

1. 创建总线：使用 ``spi_bus_create`` 创建一个总线实例，创建时需要指定
   SPI 端口号 ( 可选 ``SPI2_HOST`` ``SPI3_HOST`` )，以及总线配置项
   ``spi_config_t``\ 。配置项包括 MOSI、MISO、SCLK
   引脚号，因为这些引脚在系统设计时已经确定，一般不在运行时切换。总线配置项还包括
   ``max_transfer_sz`` , 用于配置一次传输时的最大数据量，设置为 0
   将使用默认值 4096。
2. 创建设备：使用 ``spi_bus_device_create``
   在已创建的总线实例之上创建设备，创建时需要指定总线句柄、设备的 CS
   引脚号、设备运行模式、设备运行的时钟频率，spi
   传输时将根据设备的配置项动态切换模式和频率。
3. 数据传输：使用 ``spi_bus_transfer_byte`` ``spi_bus_transfer_bytes``
   ``spi_bus_transfer_reg16`` ``spi_bus_transfer_reg32``
   可直接进行数据的传输操作，由于 SPI
   是全双工通信，因此每次传输发送和接收可以同时进行，只需要传入设备句柄、将要发送的数据、用于存放读取数据的
   buf 和传输长度。
4. 删除设备和总线：如果所有的 spi\_bus
   通信已经完成，可以通过删除设备和总线实例释放系统资源。可使用
   ``spi_bus_device_delete`` 分别将已创建的设备删除，然后使用
   ``spi_bus_delete``
   将总线资源删除。如果在设备未删除时删除总线，操作将不会被执行。

组件依赖
--------

-  无

已适配 IDF 版本
---------------

-  ESP-IDF 4.0 及以上版本

已适配芯片
----------

-  ESP32
-  ESP32S2

API 参考
--------

.. include:: /_build/inc/i2c_bus.inc

.. include:: /_build/inc/spi_bus.inc
