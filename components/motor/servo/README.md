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

## LEDC resource usage

Each servo handle owns one LEDC channel. A channel cannot be used by more than one servo handle at the same time.

Multiple servo handles can share the same LEDC timer only when they use the same speed mode, frequency, and duty resolution. Use different timers for servos that require different PWM frequencies.

## Reference

[Documentation](https://docs.espressif.com/projects/esp-iot-solution/en/latest/motor/servo.html)
