Boards Component
====================

:link_to_translation:`zh_CN:[中文]`

.. toctree::
    :maxdepth: 1
    
    i2c_bus <i2c_bus.rst>
    spi_bus <spi_bus.rst>

The communication bus component (Bus) is a set of application-layer code built on top of the ESP-IDF peripheral driver code, including ``i2c_bus``, ``spi_bus`` and etc. It is mainly used for bus communication between ESP chips and external devices. From the point of application development, this component has the following features:

1. Simplified peripheral initialization processes
2. Thread-safe device operations
3. Simple and flexible RW operations

This component abstracts the following concepts:

1. Bus: the resource and configuration option shared between devices during communication
2. Device: device specific resource and configuration option during communication

Each physical peripheral bus can mount one or more devices if the electrical condition allows, with the SPI bus addressing devices based on CS pins and the I2C bus addressing devices based on their addresses, thus achieving software independence between different devices on the same bus.


.. figure:: ../../../_static/i2c_bus.png
    :align: center
    :width: 75%

    i2c_bus Connection Diagram

.. figure:: ../../../_static/spi_bus.png
    :align: center
    :width: 73%

    spi_bus Connection Diagram
