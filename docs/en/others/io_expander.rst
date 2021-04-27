
GPIO Expander
================
:link_to_translation:`zh_CN:[中文]`

With further expansions of the ESP32 chip family, more application scenarios with diverse demands are being introduced, including some that have more requirements on GPIO numbers. The subsequence release of ESP32-S2 and other products have included up to 43 GPIOs, which can greatly alleviate the problem of GPIO resource constraint. If this still could not meet your demand, you can also add GPIO expansion chips to ESP32 to have more GPIO resources, such as using the I2C-based GPIO expansion module MCP23017, which can expand 16 GPIO ports per module and mount up to 8 expansion modules simultaneously thus expanding additional 128 GPIO ports totally.

Adapted Products
---------------------

+------------+--------------------------------+-------+--------------+------------------------------------------------------------------------------------+---------------------------------------------------------+
| Name       | Function                       | Bus   | Vendor       | Datasheet                                                                          | Driver                                                  |
+============+================================+=======+==============+====================================================================================+=========================================================+
| MCP23017   | 16-bit I/O expander            | I2C   | Microchip    | `Datasheet <https://ww1.microchip.com/downloads/en/devicedoc/20001952c.pdf>`_      | :component:`mcp23017 <expander/io_expander/mcp23017>`   |
+------------+--------------------------------+-------+--------------+------------------------------------------------------------------------------------+---------------------------------------------------------+


