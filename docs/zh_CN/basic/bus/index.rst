通信总线组件 (Bus)
=====================

:link_to_translation:`en:[English]`

.. toctree::
    :maxdepth: 1
    
    i2c_bus <i2c_bus.rst>
    spi_bus <spi_bus.rst>

通信总线组件 (Bus) 是建立在 ESP-IDF 外设驱动代码之上的一套应用层代码，包括 ``i2c_bus``、``spi_bus`` 等，主要用于 ESP 芯片与外置设备之间的总线通信。该组件从应用开发的角度出发，实现了以下功能：

1. 简化外设初始化步骤
2. 线程安全的设备操作
3. 简单灵活的读写操作

该组件对以下概念进行了抽象：

1. 总线 (Bus)：通信时设备之间共同拥有的资源和配置项
2. 设备 (Device)：通信时设备特有的资源和配置项

每个物理外设总线 (Bus) 在电气条件允许的情况下，均可挂载一到多个设备（Device），其中 SPI 总线根据 CS 引脚对设备进行寻址，I2C 总线则根据设备地址进行寻址，进而实现相同总线不同设备之间软件上的独立。


.. figure:: ../../../_static/i2c_bus.png
    :align: center
    :width: 75%

    i2c_bus 连接框图

.. figure:: ../../../_static/spi_bus.png
    :align: center
    :width: 73%

    spi_bus 连接框图
