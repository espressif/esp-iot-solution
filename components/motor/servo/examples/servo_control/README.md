# Example for the servo component

This example shows how to use the servo component.

## How to use

Initialize the sensor with the configuration:

```c
    servo_config_t servo_cfg = {
        .max_angle = 180,
        .min_width_us = 500,
        .max_width_us = 2500,
        .freq = 50,
        .timer_number = LEDC_TIMER_0,
        .channels = {
            .servo_pin = {
                SERVO_GPIO,
            },
            .ch = {
                LEDC_CHANNEL_0,
            },
        },
        .channel_number = 1,
    };

    // Initialize the servo
    iot_servo_init(LEDC_LOW_SPEED_MODE, &servo_cfg);
```

Set the angle:

```c
uint16_t angle = 0;
iot_servo_write_angle(LEDC_LOW_SPEED_MODE, 0, angle);
```
