# ESP LCD Touch FT6336U Controller

[![Component Registry](https://components.espressif.com/components/espressif/esp_lcd_touch_ft6336u/badge.svg)](https://components.espressif.com/components/espressif/esp_lcd_touch_ft6336u)

Implementation of the FT6336U touch controller with [esp_lcd_touch](https://github.com/espressif/esp-bsp/tree/master/components/lcd_touch/esp_lcd_touch) component.

| Touch controller | Communication interface | Component name         | Reference material |
| :--------------: | :---------------------: | :--------------------: | :----------------: |
|     FT6336U      |           I2C           | esp_lcd_touch_ft6336u  | FT6336U datasheet and register map |

Default I2C address is `0x38`. Use `ESP_LCD_TOUCH_IO_I2C_FT6336U_ADDRESS_ALT` for `0x48` modules.

## Add to project

Packages from this repository are uploaded to [Espressif's component service](https://components.espressif.com/).
You can add them to your project via `idf.py add-dependency`, e.g.

```
    idf.py add-dependency "espressif/esp_lcd_touch_ft6336u"
```

## Example use

```c
    esp_lcd_panel_io_i2c_config_t io_config = ESP_LCD_TOUCH_IO_I2C_FT6336U_CONFIG();
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
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_ft6336u(io_handle, &tp_cfg, &tp));
```

For `0x48` modules:

```c
    esp_lcd_panel_io_i2c_config_t io_config =
        ESP_LCD_TOUCH_IO_I2C_FT6336U_CONFIG_WITH_ADDR(ESP_LCD_TOUCH_IO_I2C_FT6336U_ADDRESS_ALT);
```
