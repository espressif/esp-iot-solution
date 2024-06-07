| Supported Targets | ESP32-P4 |
| ----------------- | -------- |

# ESP LCD USB_DISPLAY

[![Component Registry](https://components.espressif.com/components/espressif/esp_lcd_usb_display/badge.svg)](https://components.espressif.com/components/espressif/esp_lcd_usb_display)

Implement the USB virtual display functionality using the [usb_device_uvc](https://components.espressif.com/components/espressif/usb_device_uvc) and [esp_lcd](https://github.com/espressif/esp-idf/tree/master/components/esp_lcd) components, which supports compressing the generated images into a JPEG frame stream and transmitting them to the USB host.

## Add to project

Packages from this repository are uploaded to [Espressif's component service](https://components.espressif.com/).
You can add them to your project via `idf.py add-dependancy`, e.g.
```
    idf.py add-dependency "espressif/esp_lcd_usb_display==*"
```

Alternatively, you can create `idf_component.yml`. More is in [Espressif's documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html).

## Usage Example

```C
    esp_lcd_panel_handle_t display_panel;
    usb_display_vendor_config_t vendor_config = {
        .h_res = EXAMPLE_DISPL_H_RES,
        .v_res = EXAMPLE_DISPL_V_RES,
        .fb_rgb_nums = EXAMPLE_DISPLAY_BUF_NUMS,
        .fb_uvc_jpeg_size = EXAMPLE_DISP_UVC_MAX_FB_SIZE,
        .uvc_device_index = 0,
        .jpeg_encode_config = {
            .src_type = JPEG_ENCODE_IN_FORMAT_RGB888,
            .sub_sample = JPEG_DOWN_SAMPLING_YUV420,
            .quality = EXAMPLE_JPEG_ENC_QUALITY,
            .task_priority = EXAMPLE_JPEG_TASK_PRIORITY,
            .task_core_id = EXAMPLE_JPEG_TASK_CORE,
        },
        .user_ctx = display_panel,
    };
    esp_lcd_panel_dev_config_t panel_dev_config = {
        .bits_per_pixel = EXAMPLE_DISPL_BIT_PER_PIXEL,
        .vendor_config = &vendor_config,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_usb_display(&panel_dev_config, &display_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(display_panel));
```