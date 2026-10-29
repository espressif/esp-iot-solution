# ESP LCD Touch CST9220 Controller

[![Component Registry](https://components.espressif.com/components/espressif/esp_lcd_touch_cst9220/badge.svg)](https://components.espressif.com/components/espressif/esp_lcd_touch_cst9220)

Implementation of the CST9220 touch controller with the
[esp_lcd_touch](https://github.com/espressif/esp-bsp/tree/master/components/lcd_touch/esp_lcd_touch) component.

| Touch controller | Communication interface | Component name          |
| :--------------: | :---------------------: | :---------------------: |
|     CST9220      |           I2C           | esp_lcd_touch_cst9220   |

The driver selects the report acknowledgement policy from the firmware project ID:

* current firmware (project `0x6854` and any unknown ID) stays on the `D101` page after identification, reads the full report from `0xD000` in one burst, and writes `0xAB` immediately;
* confirmed legacy firmware (project `0x542F`) returns to `D109` after identification, uses the same burst read, and does not send an end acknowledgement;
* both paths mark a release by clearing the status nibble while keeping the coordinate and the record count, so records are filtered by their touch status to keep a release from being reported as an active point.

The firmware check code is read from `D228` when the controller answers the `D11E`
command-mode handshake. Older images fall back to `D1FC`.

The component only implements runtime touch operation. Firmware upgrade is not included.

## Add to project

Packages from this repository are uploaded to [Espressif's component service](https://components.espressif.com/).
You can add the component to a project with:

```sh
idf.py add-dependency "espressif/esp_lcd_touch_cst9220"
```

## Example use

```c
esp_lcd_panel_io_i2c_config_t io_config = ESP_LCD_TOUCH_IO_I2C_CST9220_CONFIG();
esp_lcd_panel_io_handle_t io_handle = NULL;
ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus_handle, &io_config, &io_handle));

const esp_lcd_touch_config_t tp_config = {
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
ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_cst9220(io_handle, &tp_config, &tp));
```

Read touch data through the common API:

```c
esp_lcd_touch_point_data_t points[ESP_LCD_TOUCH_CST9220_MAX_POINTS];
uint8_t point_count = 0;

ESP_ERROR_CHECK(esp_lcd_touch_read_data(tp));
ESP_ERROR_CHECK(esp_lcd_touch_get_data(tp, points, &point_count,
                                      ESP_LCD_TOUCH_CST9220_MAX_POINTS));
```

Set `CONFIG_ESP_LCD_TOUCH_MAX_POINTS` to at least `2` to receive both supported touch points.

## Sleep mode

The standard touch sleep APIs are supported:

```c
ESP_ERROR_CHECK(esp_lcd_touch_enter_sleep(tp));
ESP_ERROR_CHECK(esp_lcd_touch_exit_sleep(tp));
```

The driver sends the standard CST9220 `D105` sleep command. Wakeup uses `D109`, or a hardware reset if that command is
unavailable and a reset GPIO is configured, then returns current firmware to the `D101` page used for reports. No
CST9220-specific sleep API is required.

## Report acknowledgement

After each successful report read, current firmware is acknowledged with `{0xD0, 0x00, 0xAB}` before the frame is
decoded. The write is sent for touch, release, empty, and malformed reports so the controller can publish the next
frame instead of waiting for its internal timeout. Confirmed legacy firmware does not use this acknowledgement.
