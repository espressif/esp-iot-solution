# Power Measurement Example

## Overview

This example demonstrates how to use power measurement chips (**BL0937** and **INA236**) to detect electrical parameters such as voltage, current, active power, and energy consumption. It is implemented for **ESP32 series chips** using FreeRTOS, and shows how to configure and interface with different power measurement chips. The example initializes the power measurement system, fetches various parameters, and logs them at regular intervals.

This example supports two power measurement chips:

### BL0937 Chip

Capable of measuring:

1. **Voltage**
2. **Current**
3. **Active Power**
4. **Power Factor**
5. **Energy**

### INA236 Chip

Capable of measuring:

1. **Voltage**
2. **Current**
3. **Active Power**
4. **Power Factor** (fixed at 1.0 for DC circuits)

The primary goal is to demonstrate how to configure the hardware pins, initialize the power measurement system, and retrieve the data from different chip types.

## Configuration

### Chip Type Selection

Configure chip type in `idf.py menuconfig`:

```
Component config → power_measure → Power measurement chip type
```

Options:

- **BL0937** - Single-phase power measurement IC with GPIO interface
- **INA236** - Precision digital power monitor with I2C interface

### Hardware Configuration

After selecting chip type, relevant hardware configuration options will be displayed automatically:

#### BL0937 Configuration

- GPIO pin configuration (CF, SEL, CF1)
- Hardware parameter configuration (reference voltage, sampling resistor, etc.)
- Calibration factor configuration (voltage, current, power calibration)

#### INA236 Configuration

- I2C pin configuration (SCL, SDA)
- I2C address configuration

## Hardware Requirements

### BL0937 Configuration

The example uses the **BL0937** power measurement chip and ESP32-C3(C2) for test. To connect it, the following pins must be configured on the ESP32-C3(C2):

| Variable            | Default GPIO | Chip Pin | Description              |
| ------------------- | ------------ | -------- | ------------------------ |
| `BL0937_CF_GPIO`  | GPIO 3       | CF Pin   | Current frequency output |
| `BL0937_SEL_GPIO` | GPIO 4       | SEL Pin  | Selection pin            |
| `BL0937_CF1_GPIO` | GPIO 7       | CF1 Pin  | Power frequency output   |

**Note:** GPIO pins can be configured via `idf.py menuconfig` under Component config → power_measure → GPIO Pin Configuration.

Make sure that these GPIO pins are correctly connected to the respective pins on the **BL0937** chip in your hardware setup.

### INA236 Configuration

The example also supports the **INA236** power measurement chip via I2C interface. To connect it, the following pins must be configured:

| Variable              | Default GPIO | Chip Pin | Description    |
| --------------------- | ------------ | -------- | -------------- |
| `I2C_MASTER_SDA_IO` | GPIO 20      | SDA Pin  | I2C data line  |
| `I2C_MASTER_SCL_IO` | GPIO 13      | SCL Pin  | I2C clock line |

**INA236 I2C Configuration:**

- I2C Address: `0x41` (default)
- I2C Clock Speed: `100kHz`
- Alert pin: Disabled (can be enabled if needed)

**Note:** GPIO pins can be configured via `idf.py menuconfig` under Component config → power_measure → GPIO Pin Configuration.

Make sure that these GPIO pins are correctly connected to the respective pins on the **INA236** chip in your hardware setup.

## BL0937 Calibration Factor Usage Guide

### Calibration Factor Overview

BL0937 chip measurement accuracy can be adjusted through calibration factors:

- **KU (Voltage Calibration Factor)**: Used to adjust voltage measurement accuracy, unit: us/V
- **KI (Current Calibration Factor)**: Used to adjust current measurement accuracy, unit: us/A
- **KP (Power Calibration Factor)**: Used to adjust power measurement accuracy, unit: us/W

### How Calibration Factors Work

The BL0937 chip measures pulse frequency which is converted to physical quantities using calibration factors:

- Pulse frequency → calibration factor → actual electrical quantity
- The calibration factor is essentially the conversion coefficient from frequency to a physical value
- Calibration factors should be adjusted according to real hardware and accuracy needs

### Calibration Steps

#### 1. Voltage Calibration

1. **Prepare standard voltage source**: Use multimeter or standard voltage source to ensure accurate voltage value
2. **Read measured value**:
   ```c
   float measured_voltage;
   power_measure_get_voltage(power_handle, &measured_voltage);
   printf("Measured voltage: %.2f V\n", measured_voltage);
   ```
3. **Calculate calibration factor**:
   ```c
   float standard_voltage = 220.0f;  // Standard voltage value
   float calibration_factor = standard_voltage / measured_voltage;
   printf("Voltage calibration factor: %.4f\n", calibration_factor);
   ```
4. **Apply calibration**:
   Assign the computed `calibration_factor` to `DEFAULT_KU`.

#### 2. Current Calibration

1. **Prepare standard current source**: Use ammeter or standard current source
2. **Read measured value**:
   ```c
   float measured_current;
   power_measure_get_current(power_handle, &measured_current);
   printf("Measured current: %.3f A\n", measured_current);
   ```
3. **Calculate calibration factor**:
   ```c
   float standard_current = 15.0f;  // Standard current value
   float calibration_factor = standard_current / measured_current;
   ```
4. **Apply calibration**:
   Assign the computed `calibration_factor` to `DEFAULT_KI`.

#### 3. Power Calibration

1. **Prepare standard power source**: Use power meter or known power load
2. **Read measured value**:
   ```c
   float measured_power;
   power_measure_get_active_power(power_handle, &measured_power);
   printf("Measured power: %.2f W\n", measured_power);
   ```
3. **Calculate calibration factor**:
   ```c
   float standard_power = 20.0f;  // Standard power value
   float calibration_factor = standard_power / measured_power;
   ```
4. **Apply calibration**:
   Assign the computed `calibration_factor` to `DEFAULT_KP`.

### Calibration Notes

1. **Calibration Environment**:

   - Ensure stable ambient temperature
   - Avoid electromagnetic interference
   - Use stable power supply
2. **Calibration Order**:

   - Recommend calibrating voltage first, then current, finally power
   - Wait for measurement to stabilize after each calibration before proceeding
3. **Calibration Factor Storage**:

   - Calibration factors are saved in the device and remain effective after restart
   - To reset calibration factors, reinitialize the device

## Chip Selection

To switch between different power measurement chips, use the menuconfig configuration:

```bash
idf.py menuconfig
# Navigate to: Component config → power_measure → Power measurement chip type
# Select: BL0937 or INA236
```

The chip selection is now handled automatically through the configuration system, eliminating the need to modify source code macros.

## Software Requirements

* ESP-IDF (Espressif IoT Development Framework)
* FreeRTOS for task management
* Power measurement driver (`power_measure.h`)

## GPIO Configuration

GPIO pins can be configured via menuconfig without modifying source code:

```bash
idf.py menuconfig
# Navigate to: Component config → power_measure → GPIO Pin Configuration
# Configure BL0937 and INA236 GPIO pins as needed
```

This allows you to easily adapt the example to different hardware configurations without code changes.

## Configuration

Adjust the following based on your hardware and application:

- **Chip selection**: Use the `DEMO_BL0937` and `DEMO_INA236` macros to switch between different chips.
- **GPIO/I2C pins**:
  - BL0937: `BL0937_CF_GPIO`, `BL0937_SEL_GPIO`, `BL0937_CF1_GPIO`;
  - INA236: `I2C_MASTER_SDA_IO`, `I2C_MASTER_SCL_IO`, `I2C_MASTER_NUM`, `I2C_MASTER_FREQ_HZ`;
- **I2C parameters (INA236)**: Address typically `0x41`, clock `100kHz` (adjustable);
- **Calibration (BL0937)**: `ki/ku/kp` calibration factors, can be dynamically adjusted through API functions or use hardware calibration results;
- **Thresholds**: Set overcurrent/overvoltage/undervoltage to meet safety requirements;
- **Energy detection**: Enable only on chips that support energy (disabled for INA236 in the example).

## Logging

The application uses `ESP_LOGI` for informational logging and `ESP_LOGE` for error logging. The log messages will show up in the serial monitor or logging console.

Example log output:

### BL0937 Output:

```
I (12345) PowerMeasureExample: Power Measure Example Starting...
I (12346) PowerMeasureExample: Selected chip: BL0937 (GPIO interface)
I (12347) PowerMeasureExample: GPIO pins - CF:3, SEL:4, CF1:7
I (12348) PowerMeasureExample: Power measurement initialized successfully
I (12349) PowerMeasureExample: Starting measurement loop...
I (12350) PowerMeasureExample: Voltage: 230.15 V
I (12351) PowerMeasureExample: Current: 0.45 A
I (12352) PowerMeasureExample: Power: 103.35 W
I (12353) PowerMeasureExample: Energy: 0.12 Kw/h
I (12354) PowerMeasureExample: ---
```

### BL0937 Calibration Output:

```
I (12345) PowerMeasureExample: Starting BL0937 calibration process...
I (12346) PowerMeasureExample: Voltage before calibration: 225.30 V
I (12347) PowerMeasureExample: Voltage after calibration: 220.00 V
I (12348) PowerMeasureExample: Current before calibration: 1.45 A
I (12349) PowerMeasureExample: Current after calibration: 1.50 A
I (12350) PowerMeasureExample: Power before calibration: 340.20 W
I (12351) PowerMeasureExample: Power after calibration: 330.00 W
I (12352) PowerMeasureExample: BL0937 calibration completed
```

### INA236 Output:

```
I (12345) PowerMeasureExample: Power Measure Example Starting...
I (12346) PowerMeasureExample: Selected chip: INA236 (I2C interface)
I (12347) PowerMeasureExample: I2C bus initialized successfully
I (12348) PowerMeasureExample: INA236 Configuration: I2C_ADDR=0x41, SCL=13, SDA=20
I (12349) PowerMeasureExample: Power measurement initialized successfully
I (12350) PowerMeasureExample: Starting measurement loop...
I (12351) PowerMeasureExample: Voltage: 3.30 V
I (12352) PowerMeasureExample: Current: 125.50 mA
I (12353) PowerMeasureExample: Power: 0.41 W (calculated)
I (12354) PowerMeasureExample: Energy measurement not available for INA236
I (12355) PowerMeasureExample: ---
```
