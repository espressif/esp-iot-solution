触摸屏驱动
===============

esp-iot-solution 中的触摸屏驱动支持如下屏幕：

 - XPT2046
 - FT5x06
 - NS2016

驱动同时支持 ESP32 和 ESP32-S2 芯片

驱动结构
----------




应用示例
------------

初始化一个触摸屏
*****************

.. code:: c

    extern touch_driver_fun_t xpt2046_driver_fun;
    static touch_driver_fun_t *g_touch_driver = &xpt2046_driver_fun;

    lcd_touch_config_t conf = {
    #ifdef CONFIG_TOUCH_DRIVER_INTERFACE_I2C
            .iface_i2c = {
                .pin_num_sda = CONFIG_TOUCH_I2C_SDA_PIN,
                .pin_num_scl = CONFIG_TOUCH_I2C_SCL_PIN,
                .clk_freq = 100000,
                .i2c_port = CONFIG_TOUCH_I2C_PORT_NUM,
                .i2c_addr = CONFIG_TOUCH_I2C_ADDRESS,
            },
    #endif
    #ifdef CONFIG_TOUCH_DRIVER_INTERFACE_SPI
            .iface_spi = {
    #ifdef CONFIG_TOUCH_SPI_USE_HOST
                .pin_num_miso = CONFIG_TOUCH_SPI_MISO_PIN,
                .pin_num_mosi = CONFIG_TOUCH_SPI_MOSI_PIN,
                .pin_num_clk = CONFIG_TOUCH_SPI_CLK_PIN,
                .clk_freq = CONFIG_TOUCH_SPI_CLOCK_FREQ,
                .spi_host = CONFIG_TOUCH_SPI_HOST,
                .init_spi_bus = true,
    #else
                .pin_num_miso = -1,
                .pin_num_mosi = -1,
                .pin_num_clk = -1,
                .clk_freq = 5000000,
                .spi_host = CONFIG_LCD_SPI_HOST,
                .init_spi_bus = false,
    #endif
                .pin_num_cs = CONFIG_TOUCH_SPI_CS_PIN,
                .dma_chan = 2,
                
            },
    #endif
            .pin_num_int = CONFIG_TOUCH_INT_PIN_NUM,
            .rotation = CONFIG_TOUCH_ROTATE,
            .width = CONFIG_TOUCH_SCREEN_WIDTH,
            .height = CONFIG_TOUCH_SCREEN_HEIGHT,
        };
        esp_err_t ret = g_touch_driver->init(&conf);


设置触摸屏的旋转
*****************

由于电阻屏在每个方向上的触摸数据并不相同，所以不建议使用电阻屏进行旋转，而是直接重新校准，用校准参数来保存旋转

.. code:: c

    g_touch_driver->set_rotate(TOUCH_DIR_LRTB);

获取触摸屏是否按下
********************

.. code:: c

    g_touch_driver->is_pressed();

获取触摸信息
****************

.. code:: c

    touch_info_t info;
    g_touch_driver->read_info(&info);


API Reference
-----------------
