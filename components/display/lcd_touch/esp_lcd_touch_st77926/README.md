# ESP LCD Touch ST77926 Controller

[![Component Registry](https://components.espressif.com/components/espressif/esp_lcd_touch_st77926/badge.svg)](https://components.espressif.com/components/espressif/esp_lcd_touch_st77926)

Implementation of the ST77926 touch controller with [esp_lcd_touch](https://github.com/espressif/esp-bsp/tree/master/components/lcd_touch/esp_lcd_touch) component.

| Touch controller | Communication interface | Component name        |
| :--------------: | :---------------------: | :-------------------: |
|     ST77926      |           I2C           | esp_lcd_touch_st77926 |

## Add to project

Packages from this repository are uploaded to [Espressif's component service](https://components.espressif.com/).
You can add them to your project via `idf.py add-dependency`, e.g.

```
    idf.py add-dependency "espressif/esp_lcd_touch_st77926"
```

## Example use

```c
    esp_lcd_panel_io_i2c_config_t io_config = ESP_LCD_TOUCH_IO_I2C_ST77926_CONFIG();
    esp_lcd_panel_io_handle_t io_handle = NULL;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus_handle, &io_config, &io_handle));

    esp_lcd_touch_config_t tp_cfg = {
        .x_max = EXAMPLE_LCD_H_RES,
        .y_max = EXAMPLE_LCD_V_RES,
        .rst_gpio_num = EXAMPLE_TOUCH_RST,
        .int_gpio_num = EXAMPLE_TOUCH_INT,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
    };

    esp_lcd_touch_handle_t tp = NULL;
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_st77926(io_handle, &tp_cfg, &tp));
```
