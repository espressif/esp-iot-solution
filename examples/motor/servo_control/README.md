# Example for the servo component

This example shows how to use the servo component.

## How to use

Initialize the sensor with the configuration:

```c
    servo_handle_t servo = NULL;
    servo_config_t servo_cfg = SERVO_CONFIG_DEFAULT(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, LEDC_CHANNEL_0, SERVO_GPIO);

    // Initialize the servo
    iot_servo_new(&servo_cfg, &servo);
```

Set the angle:

```c
uint16_t angle = 0;
iot_servo_write_angle(servo, angle);
```

Each servo handle owns one LEDC channel. Multiple servo handles can share the same LEDC timer only when they use the same speed mode, frequency, and duty resolution.
