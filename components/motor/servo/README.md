# Servo Motor Component

This component provides an easy-to-use interface for controlling servo motors with the ESP-IDF framework. Servo motors are commonly used in robotics, automation, and various mechanical applications due to their precise control of angular position.

The library uses PWM (Pulse Width Modulation) to control servo motor rotation and supports customizable configurations for angle limits, pulse width, frequency, and more.

## Features

- Easy control of servo motors using PWM signals
- Support for custom angle range and pulse width
- Flexible configuration of PWM channels and timers
- Compatible with the ESP-IDF framework

## Getting Started

### Prerequisites

- ESP-IDF installed and configured on your development environment
- A servo motor compatible with PWM signals
- Proper connections between the ESP32 and the servo motor (signal, VCC, GND)

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

## Reference

[Documentation](https://docs.espressif.com/projects/esp-iot-solution/en/latest/motor/servo.html)
