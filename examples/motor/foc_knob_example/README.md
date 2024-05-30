# ESP FOC Knob
This example demonstrates the application of the ESP32-S3 microcontroller to control a motor, effectively transforming it into a knob-like interface.

## Modes supported

|            Mode             |                                                    Description                                                     |
| --------------------------- | ------------------------------------------------------------------------------------------------------------------ |
| 0. unbound no detents       | Motor can be unbounded with fine rotation and no dent points.                                                      |
| 1. Unbounded Fine detents   | Motor can be unbounded with fine rotation and fine dent points.                                                    |
| 2. Unbounded Strong detents | Motor can be unbound with fine rotation and noticeable dent points.                                                |
| 3. Bounded No detents       | Motor will be constrained within a specific range (0-360) with fine rotation and no detent points.                 |
| 4. Bounded Fine detents     | Motor can be constrained with coarser rotation and fine dent points.                                               |
| 5. Bounded Strong detents   | Motor can be constrained with coarser rotation and strong dent points.                                             |
| 6. On/Off strong detents    | Motor will be constrained within a specific range presents a On/off functionality with noticeable stopping points. |

However, users have the flexibility to adjust mode parameters to attain various responses. Within the following structure, users can modify these values to customize the functionality according to their specific requirements.

```c
typedef struct {
    int32_t num_positions;        // Number of positions
    int32_t position;             // Current position
    float position_width_radians; // Width of each position in radians
    float detent_strength_unit;   // Strength of detent during normal rotation
    float endstop_strength_unit;  // Strength of detent when reaching the end
    float snap_point;             // Snap point for each position
    const char *descriptor;       // Description
} knob_param_t;

```

### Hardware Required
This example is specifically designed for ESP32-S3-Motor-LCDkit.

Other components
1. The BLDC Motor Model 5v 2804 is compatible and can be employed.
2. A Position Sensor (Hall-based) such as MT6701 or AS5600 is suitable for use.

### Configure the project

step1: chose your target chip.

````
idf.py set-target esp32s3
````

step2: build the project

```
idf.py build
```

step 3: Flash and monitor
Flash the program and launch IDF Monitor:

```bash
idf.py flash monitor
```

![FOC Knob example](https://dl.espressif.com/ae/esp-iot-solution/foc_knob.gif)