# LVGL ESP Portation

[![Component Registry](https://components.espressif.com/components/espressif/esp_lvgl_port/badge.svg)](https://components.espressif.com/components/espressif/esp_lvgl_port)
![maintenance-status](https://img.shields.io/badge/maintenance-actively--developed-brightgreen.svg)

This component helps with using LVGL with Espressif's LCD and touch drivers. It can be used with any project with LCD display.

## Features
* Initialization of the LVGL
    * Create task and timer
    * Handle rotating
    * Power saving
* Add/remove display (using [`esp_lcd`](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/lcd.html))
* Add/remove touch input (using [`esp_lcd_touch`](https://github.com/espressif/esp-bsp/tree/master/components/lcd_touch))
* Add/remove navigation buttons input (using [`button`](https://github.com/espressif/esp-iot-solution/tree/master/components/button))
* Add/remove encoder input (using [`knob`](https://github.com/espressif/esp-iot-solution/tree/master/components/knob))
* Add/remove USB HID mouse/keyboard input (using [`usb_host_hid`](https://components.espressif.com/components/espressif/usb_host_hid))

## LVGL Version

This component supports **LVGL8** and **LVGL9**. By default, it selects the latest LVGL version. If you want to use a specific version (e.g. latest LVGL8), you can easily define this requirement in `idf_component.yml` in your project like this:

```
  lvgl/lvgl:
    version: "^8"
    public: true
```

### LVGL Version Compatibility

This component is fully compatible with LVGL version 9. All types and functions are used from LVGL9. Some LVGL9 types are not supported in LVGL8 and there are retyped in [`esp_lvgl_port_compatibility.h`](include/esp_lvgl_port_compatibility.h) header file. **Please, be aware, that some draw and object functions are not compatible between LVGL8 and LVGL9.**

## Usage

### Initialization
``` c
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    esp_err_t err = lvgl_port_init(&lvgl_cfg);
```

### Add screen

Add an LCD screen to the LVGL. It can be called multiple times for adding multiple LCD screens.

``` c
    static lv_disp_t * disp_handle;

    /* LCD IO */
	esp_lcd_panel_io_handle_t io_handle = NULL;
	ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t) 1, &io_config, &io_handle));

    /* LCD driver initialization */
    esp_lcd_panel_handle_t lcd_panel_handle;
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &lcd_panel_handle));

    /* Add LCD screen */
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = lcd_panel_handle,
        .buffer_size = DISP_WIDTH*DISP_HEIGHT,
        .double_buffer = true,
        .hres = DISP_WIDTH,
        .vres = DISP_HEIGHT,
        .monochrome = false,
        .mipi_dsi = false,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .rotation = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        },
        .flags = {
            .buff_dma = true,
            .swap_bytes = false,
        }
    };
    disp_handle = lvgl_port_add_disp(&disp_cfg);

    /* ... the rest of the initialization ... */

    /* If deinitializing LVGL port, remember to delete all displays: */
    lvgl_port_remove_disp(disp_handle);
```

> [!NOTE]
> 1. For adding RGB or MIPI-DSI screen, use functions `lvgl_port_add_disp_rgb` or `lvgl_port_add_disp_dsi`.
> 2. DMA buffer can be used only when you use color format `LV_COLOR_FORMAT_RGB565`.

### Add touch input

Add touch input to the LVGL. It can be called more times for adding more touch inputs.
``` c
    /* Touch driver initialization */
    ...
    esp_lcd_touch_handle_t tp;
    esp_err_t err = esp_lcd_touch_new_i2c_gt911(io_handle, &tp_cfg, &tp);

    /* Add touch input (for selected screen) */
    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp = disp_handle,
        .handle = tp,
    };
    lv_indev_t* touch_handle = lvgl_port_add_touch(&touch_cfg);

    /* ... the rest of the initialization ... */

    /* If deinitializing LVGL port, remember to delete all touches: */
    lvgl_port_remove_touch(touch_handle);
```

### Add buttons input

Add buttons input to the LVGL. It can be called more times for adding more buttons inputs for different displays. This feature is available only when the component `espressif/button` was added into the project.
``` c
    /* Buttons configuration structure */
    const button_config_t bsp_button_config[] = {
        {
            .type = BUTTON_TYPE_ADC,
            .adc_button_config.adc_channel = ADC_CHANNEL_0, // ADC1 channel 0 is GPIO1
            .adc_button_config.button_index = 0,
            .adc_button_config.min = 2310, // middle is 2410mV
            .adc_button_config.max = 2510
        },
        {
            .type = BUTTON_TYPE_ADC,
            .adc_button_config.adc_channel = ADC_CHANNEL_0, // ADC1 channel 0 is GPIO1
            .adc_button_config.button_index = 1,
            .adc_button_config.min = 1880, // middle is 1980mV
            .adc_button_config.max = 2080
        },
        {
            .type = BUTTON_TYPE_ADC,
            .adc_button_config.adc_channel = ADC_CHANNEL_0, // ADC1 channel 0 is GPIO1
            .adc_button_config.button_index = 2,
            .adc_button_config.min = 720, // middle is 820mV
            .adc_button_config.max = 920
        },
    };

    const lvgl_port_nav_btns_cfg_t btns = {
        .disp = disp_handle,
        .button_prev = &bsp_button_config[0],
        .button_next = &bsp_button_config[1],
        .button_enter = &bsp_button_config[2]
    };

    /* Add buttons input (for selected screen) */
    lv_indev_t* buttons_handle = lvgl_port_add_navigation_buttons(&btns);

    /* ... the rest of the initialization ... */

    /* If deinitializing LVGL port, remember to delete all buttons: */
    lvgl_port_remove_navigation_buttons(buttons_handle);
```
> [!NOTE]
> When you use navigation buttons for control LVGL objects, these objects must be added to LVGL groups. See [LVGL documentation](https://docs.lvgl.io/master/overview/indev.html?highlight=lv_indev_get_act#keypad-and-encoder) for more info.

### Add encoder input

Add encoder input to the LVGL. It can be called more times for adding more encoder inputs for different displays. This feature is available only when the component `espressif/knob` was added into the project.
``` c

    const button_config_t encoder_btn_config = {
        .type = BUTTON_TYPE_GPIO,
        .gpio_button_config.active_level = false,
        .gpio_button_config.gpio_num = GPIO_BTN_PRESS,
    };

    const knob_config_t encoder_a_b_config = {
        .default_direction = 0,
        .gpio_encoder_a = GPIO_ENCODER_A,
        .gpio_encoder_b = GPIO_ENCODER_B,
    };

    /* Encoder configuration structure */
    const lvgl_port_encoder_cfg_t encoder = {
        .disp = disp_handle,
        .encoder_a_b = &encoder_a_b_config,
        .encoder_enter = &encoder_btn_config
    };

    /* Add encoder input (for selected screen) */
    lv_indev_t* encoder_handle = lvgl_port_add_encoder(&encoder);

    /* ... the rest of the initialization ... */

    /* If deinitializing LVGL port, remember to delete all encoders: */
    lvgl_port_remove_encoder(encoder_handle);
```
> [!NOTE]
> When you use encoder for control LVGL objects, these objects must be added to LVGL groups. See [LVGL documentation](https://docs.lvgl.io/master/overview/indev.html?highlight=lv_indev_get_act#keypad-and-encoder) for more info.

### Add USB HID keyboard and mouse input

Add mouse and keyboard input to the LVGL. This feature is available only when the component [usb_host_hid](https://components.espressif.com/components/espressif/usb_host_hid) was added into the project.

``` c
    /* USB initialization */
    usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    ESP_ERROR_CHECK(usb_host_install(&host_config));

    ...

    /* Add mouse input device */
    const lvgl_port_hid_mouse_cfg_t mouse_cfg = {
        .disp = display,
        .sensitivity = 1, /* Sensitivity of the mouse moving */
    };
    lvgl_port_add_usb_hid_mouse_input(&mouse_cfg);

    /* Add keyboard input device */
    const lvgl_port_hid_keyboard_cfg_t kb_cfg = {
        .disp = display,
    };
    kb_indev = lvgl_port_add_usb_hid_keyboard_input(&kb_cfg);
```

Keyboard special behavior (when objects are in group):
- **TAB**: Select next object
- **SHIFT** + **TAB**: Select previous object
- **ENTER**: Control object (e.g. click to button)
- **ARROWS** or **HOME** or **END**: Move in text area
- **DEL** or **Backspace**: Remove character in textarea

> [!NOTE]
> When you use keyboard for control LVGL objects, these objects must be added to LVGL groups. See [LVGL documentation](https://docs.lvgl.io/master/overview/indev.html?highlight=lv_indev_get_act#keypad-and-encoder) for more info.

### LVGL API usage

Every LVGL calls must be protected with these lock/unlock commands:
``` c
	/* Wait for the other task done the screen operation */
    lvgl_port_lock(0);
    ...
    lv_obj_t * screen = lv_disp_get_scr_act(disp_handle);
    lv_obj_t * obj = lv_label_create(screen);
    ...
    /* Screen operation done -> release for the other task */
    lvgl_port_unlock();
```

### Rotating screen

LVGL port supports rotation of the display. You can select whether you'd like software rotation or hardware rotation.
Software rotation requires no additional logic in your `flush_cb` callback.

Rotation mode can be selected in the `lvgl_port_display_cfg_t` structure.
``` c
    const lvgl_port_display_cfg_t disp_cfg = {
        ...
        .flags = {
            ...
            .sw_rotate = true / false, // true: software; false: hardware
        }
    }
```
Display rotation can be changed at runtime.

``` c
    lv_disp_set_rotation(disp_handle, LV_DISP_ROT_90);
```

> [!NOTE]
> This feature consume more RAM.

> [!NOTE]
> During the hardware rotating, the component call [`esp_lcd`](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/lcd.html) API. When using software rotation, you cannot use neither `direct_mode` nor `full_refresh` in the driver. See [LVGL documentation](https://docs.lvgl.io/8.3/porting/display.html?highlight=sw_rotate) for more info.

### Using PSRAM canvas

If the SRAM is insufficient, you can use the PSRAM as a canvas and use a small trans_buffer to carry it, this makes drawing more efficient.
``` c
    const lvgl_port_display_cfg_t disp_cfg = {
        ...
        .buffer_size = DISP_WIDTH * DISP_HEIGHT, // in PSRAM, not DMA-capable
        .trans_size = size, // in SRAM, DMA-capable
        .flags = {
            .buff_spiram = true,
            .buff_dma = false,
            ...
        }
    }
```

### Generating images (C Array)

Images can be generated during build by adding these lines to end of the main CMakeLists.txt:
```
# Generate C array for each image
lvgl_port_create_c_image("images/logo.png" "images/" "ARGB8888" "NONE")
lvgl_port_create_c_image("images/image.png" "images/" "ARGB8888" "NONE")
# Add generated images to build
lvgl_port_add_images(${COMPONENT_LIB} "images/")
```

Usage of create C image function:
```
lvgl_port_create_c_image(input_image output_folder color_format compression)
```

Available color formats:
L8,I1,I2,I4,I8,A1,A2,A4,A8,ARGB8888,XRGB8888,RGB565,RGB565A8,RGB888,TRUECOLOR,TRUECOLOR_ALPHA,AUTO

Available compression:
NONE,RLE,LZ4

> [!NOTE]
> Parameters `color_format` and `compression` are used only in LVGL 9.

## Power Saving

The LVGL port can be optimized for power saving mode. There are two main features.

### LVGL task sleep

For optimization power saving, the LVGL task should sleep, when it does nothing. Set `task_max_sleep_ms` to big value, the LVGL task will wait for events only.

The LVGL task can sleep till these situations:
* LVGL display invalidate
* LVGL animation in process
* Touch interrupt
* Button interrupt
* Knob interrupt
* USB mouse/keyboard interrupt
* Timeout (`task_max_sleep_ms` in configuration structure)
* User wake (by function `lvgl_port_task_wake`)

> [!WARNING]
> This feature is available from LVGL 9.

> [!NOTE]
> Don't forget to set the interrupt pin in LCD touch when you set a big time for sleep in `task_max_sleep_ms`.

### Stopping the timer

Timers can still work during light-sleep mode. You can stop LVGL timer before use light-sleep by function:

```
lvgl_port_stop();
```

and resume LVGL timer after wake by function:

```
lvgl_port_resume();
```

## Performance

Key feature of every graphical application is performance. Recommended settings for improving LCD performance is described in a separate document [here](docs/performance.md).

### Performance monitor

For show performance monitor in LVGL9, please add these lines to sdkconfig.defaults and rebuild all.

```
CONFIG_LV_USE_OBSERVER=y
CONFIG_LV_USE_SYSMON=y
CONFIG_LV_USE_PERF_MONITOR=y
```
