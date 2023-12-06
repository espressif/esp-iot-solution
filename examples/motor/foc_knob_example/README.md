| Supported Targets |ESP32-S3 | ESP32-C3 |
| ----------------- |-------- |----------|

# ESP FOC Knob 
This example demonstrates the application of the ESP-32S3 microcontroller to control a motor, effectively transforming it into a knob-like interface.

## Modes supported

| Mode                              | Description                                                |
|-----------------------------------|------------------------------------------------------------|
| 0. Fine Value with Detents        | Motor can be unbounded with fine rotation and fine dents points.      |
| 1. Unbounded, No Detents          | Motor can be unbounded with fine rotation and no dent points.  |
| 2. Super Dial                     | Motor can be unbound with fine rotation and noticeable dent points.          |
| 3. Fine Values with Detents, Unbounded | Motor can be unbounded with fine rotation and noticeable dents points.  |
| 4. Bounded 0-13, No Detents      | Motor will be constrained within a specific range (180-360) with fine rotation and no detent points.  |
| 5. Coarse Values, Strong Detents  | Motor can be unbounded with coarser rotation and strong dent points.      |
| 6. Fine Values, No Detents        | Motor will be constrained within a specific range (256, 127), fine rotation, no dent points             |
| 7. On/Off, Strong Detents         | Motor will be constrained within a specific range presents a On/off functionality with noticeable stopping points.     |

However, users have the flexibility to adjust mode parameters to attain various responses. Within the following structure, users can modify these values to customize the functionality according to their specific requirements.

```
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