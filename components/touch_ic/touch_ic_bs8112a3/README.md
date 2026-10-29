# Touch IC BS8112A3 Driver

ESP-IDF driver for Holtek BS8112A3 capacitive touch controller IC.

## Features

- Multi-instance support
- I2C interface
- Interrupt mode with semaphore notification
- Configurable touch thresholds and wake-up settings
- Support for up to 12 keys (Key1-Key12)
- Low power saving mode (LSC) support

## Quick Start

### 1. Add Component

Please use the component manager command `add-dependency` to add the `touch_ic_bs8112a3` to your project's dependency, during the `CMake` step the component will be downloaded automatically

```
idf.py add-dependency "espressif/touch_ic_bs8112a3=*"
```

### 2. Initialize I2C Bus

```c
#include "driver/i2c_master.h"

i2c_master_bus_config_t bus_config = {
    .i2c_port = I2C_NUM_0,
    .sda_io_num = GPIO_NUM_21,
    .scl_io_num = GPIO_NUM_22,
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .glitch_ignore_cnt = 7,
    .flags.enable_internal_pullup = true,
};
i2c_master_bus_handle_t i2c_bus_handle;
i2c_new_master_bus(&bus_config, &i2c_bus_handle);
```

### 3. Create Touch IC Instance

```c
#include "touch_ic_bs8112a3.h"

// Use default chip configuration
bs8112a3_chip_config_t chip_cfg = TOUCH_IC_BS8112A3_DEFAULT_CHIP_CONFIG();

touch_ic_bs8112a3_config_t config = {
    .i2c_bus_handle = i2c_bus_handle,
    .device_address = BS8112A3_I2C_ADDR,  // 0x50
    .scl_speed_hz = 400000,
    .interrupt_pin = GPIO_NUM_NC,
    .chip_config = &chip_cfg,
};

touch_ic_bs8112a3_handle_t handle;
esp_err_t ret = touch_ic_bs8112a3_create(&config, &handle);
if (ret != ESP_OK) {
    ESP_LOGE("app", "Failed to create touch IC: %s", esp_err_to_name(ret));
}
```

### 4. Read Touch Status

```c
// Read all keys status
uint16_t status;
esp_err_t ret = touch_ic_bs8112a3_get_key_status(handle, &status);
if (ret == ESP_OK) {
    ESP_LOGI("app", "Touch status: 0x%04X", status);
}

// Read specific key (0-11 for Key0-Key11)
bool pressed;
ret = touch_ic_bs8112a3_get_key_value(handle, 0, &pressed);
if (ret == ESP_OK) {
    if (pressed) {
        ESP_LOGI("app", "Key 0 is pressed");
    }
}
```

### 5. Interrupt Mode (Optional)

```c
#include "freertos/event_groups.h"

// Configure with interrupt pin (event group is automatically created internally)
touch_ic_bs8112a3_config_t config = {
    .i2c_bus_handle = i2c_bus_handle,
    .device_address = BS8112A3_I2C_ADDR,
    .scl_speed_hz = 400000,
    .irq_pin = GPIO_NUM_5,  // Must be set if using interrupt
    .interrupt_callback = NULL,  // Optional: lightweight callback (runs in ISR, no I2C operations!)
    .interrupt_user_data = NULL,
    .chip_config = &chip_cfg,
};

touch_ic_bs8112a3_handle_t handle;
esp_err_t ret = touch_ic_bs8112a3_create(&config, &handle);

// Get event group handle (automatically created when interrupt is enabled)
EventGroupHandle_t event_group = touch_ic_bs8112a3_get_event_group(handle);

// In your task, wait for touch interrupt
while (1) {
    EventBits_t bits = xEventGroupWaitBits(event_group, BS8112A3_TOUCH_INTERRUPT_BIT,
                                            pdTRUE,  // Clear bits after reading
                                            pdFALSE, // Don't wait for all bits
                                            portMAX_DELAY);
    if (bits & BS8112A3_TOUCH_INTERRUPT_BIT) {
        // Read touch status (in task context, safe for I2C operations)
        uint16_t status;
        esp_err_t ret = touch_ic_bs8112a3_get_key_status(handle, &status);
        if (ret == ESP_OK) {
            ESP_LOGI("app", "Touch interrupt! Status: 0x%04X", status);
            // Process touch event
        }
    }
}
```

**Note:** The interrupt callback (if provided) runs in ISR context. Do NOT perform I2C operations or other blocking operations in the callback. Use the event group mechanism instead to notify your task, then read the touch status in task context.
