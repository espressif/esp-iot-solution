# Power Measure Component

## Overview

The Power Measure component provides a unified, object-oriented interface for electrical power measurement on ESP32 series chips. Built with a factory pattern and driver interface architecture, it supports multiple power measurement chips and offers comprehensive electrical parameter monitoring including voltage, current, active power, apparent power, power factor, and energy consumption.

This component is designed for IoT applications requiring accurate power monitoring, energy management, and electrical safety features. The modular architecture allows for easy extension to support additional power measurement chips in the future.

## Architecture

The component follows a clean architecture pattern with the following key components:

### Factory Pattern

- **Device Creation**: Use factory functions to create device instances
- **Type Safety**: Separate configuration structures for each chip type
- **Resource Management**: Handle-based API for proper resource management
- **Chip-Specific Headers**: Each chip has its own header file with configuration types and factory functions

### Driver Interface

- **Abstract Interface**: Common interface for all power measurement drivers
- **Chip Abstraction**: Unified API regardless of underlying hardware
- **Extensibility**: Easy to add new chip support by implementing the driver interface

### File Structure

```
components/sensors/power_measure/
├── include/
│   ├── power_measure.h              # Main public API and common types
│   ├── power_measure_bl0937.h       # BL0937 specific configuration and factory function
│   ├── power_measure_ina236.h       # INA236 specific configuration and factory function
│   └── power_measure_interface.h    # Driver interface definition
├── src/
│   ├── power_measure_api.c          # Main API implementation
│   ├── power_measure_bl0937.c       # BL0937 driver implementation
│   └── power_measure_ina236.c       # INA236 driver implementation
└── drivers/
    ├── bl0937/
    │   ├── bl0937.h             # BL0937 chip driver
    │   └── bl0937.c
    └── ina236/
        ├── ina236.h                 # INA236 chip driver
        ├── ina236.c
        └── ina236_reg.h             # Register definitions
```

## Supported Chips

### BL0937 Power Measurement IC

- **Type**: Dedicated power measurement chip
- **Interface**: GPIO-based (CF, SEL, CF1 pins)
- **Features**:
  - Voltage measurement (AC)
  - Current measurement (AC)
  - Active power calculation
  - Apparent power calculation
  - Power factor calculation
  - Energy accumulation
  - Real-time calibration
  - Overcurrent/overvoltage/undervoltage protection

### INA236 Precision Power Monitor

- **Type**: I2C-based precision power monitor
- **Interface**: I2C communication
- **Features**:
  - High-precision voltage measurement
  - Current measurement via shunt resistor
  - Power calculation
  - Alert functionality
  - Low power consumption

## Key Features

### Core Measurements

- **Voltage**: AC voltage measurement with calibration support
- **Current**: AC current measurement with calibration support
- **Active Power**: Real-time power consumption monitoring
- **Apparent Power**: Total power including reactive components (BL0937 only)
- **Power Factor**: Power efficiency measurement (BL0937 only)
- **Energy**: Accumulated energy consumption with persistent storage

### Safety & Protection

- **Overcurrent Protection**: Configurable current threshold monitoring
- **Overvoltage Protection**: High voltage detection and alerting
- **Undervoltage Protection**: Low voltage detection and alerting
- **Event-driven Architecture**: Real-time safety event notifications

### Calibration & Accuracy

- **Factory Calibration**: Initial calibration with known reference values
- **Runtime Calibration**: Dynamic calibration during operation (BL0937 only)
- **Multi-point Calibration**: Separate calibration for voltage, current, and power

### Advanced Features

- **Multi-chip Support**: Unified API for different measurement chips
- **Configurable Thresholds**: Customizable protection levels
- **IRAM Optimization**: Optional performance optimization for BL0937

## Hardware Requirements

### BL0937 Configuration

For BL0937 power measurement chip, connect the following pins:

| Variable            | Chip Pin | Description              |
| ------------------- | -------- | ------------------------ |
| `BL0937_CF_GPIO`  | CF Pin   | Current frequency output |
| `BL0937_SEL_GPIO` | SEL Pin  | Selection pin            |
| `BL0937_CF1_GPIO` | CF1 Pin  | Power frequency output   |

**Hardware Setup Requirements:**

- Voltage divider circuit for voltage measurement
- Current transformer or shunt resistor for current measurement
- Proper grounding and isolation
- Recommended ESP32-C3/C2 for testing

### INA236 Configuration

For INA236 precision power monitor:

| Variable              | Description |
| --------------------- | ----------- |
| `I2C_MASTER_SCL_IO` | I2C Clock   |
| `I2C_MASTER_SDA_IO` | I2C Data    |
| `I2C_MASTER_NUM`    | I2C Port    |

**Hardware Setup Requirements:**

- Shunt resistor for current measurement
- I2C pull-up resistors
- Proper power supply and grounding

## API Reference

### Device Creation (Factory Pattern)

```c
// Create BL0937 device
esp_err_t power_measure_new_bl0937_device(const power_measure_config_t *config,
                                          const power_measure_bl0937_config_t *bl0937_config,
                                          power_measure_handle_t *ret_handle);

// Create INA236 device
esp_err_t power_measure_new_ina236_device(const power_measure_config_t *config,
                                          const power_measure_ina236_config_t *ina236_config,
                                          power_measure_handle_t *ret_handle);

// Delete device
esp_err_t power_measure_delete(power_measure_handle_t handle);
```

### Measurement Functions

```c
esp_err_t power_measure_get_voltage(power_measure_handle_t handle, float* voltage);
esp_err_t power_measure_get_current(power_measure_handle_t handle, float* current);
esp_err_t power_measure_get_active_power(power_measure_handle_t handle, float* active_power);
esp_err_t power_measure_get_power_factor(power_measure_handle_t handle, float* power_factor);
esp_err_t power_measure_get_apparent_power(power_measure_handle_t handle, float* apparent_power);  // BL0937 only
```

### Energy Management

```c
esp_err_t power_measure_get_energy(power_measure_handle_t handle, float* energy);
esp_err_t power_measure_start_energy_calculation(power_measure_handle_t handle);
esp_err_t power_measure_stop_energy_calculation(power_measure_handle_t handle);
esp_err_t power_measure_reset_energy_calculation(power_measure_handle_t handle);
```

### Calibration

```c
esp_err_t power_measure_calibrate_voltage(power_measure_handle_t handle, float expected_voltage);  // BL0937 only
esp_err_t power_measure_calibrate_current(power_measure_handle_t handle, float expected_current);  // BL0937 only
esp_err_t power_measure_calibrate_power(power_measure_handle_t handle, float expected_power);      // BL0937 only
```

### Advanced Functions (BL0937 only)

```c
esp_err_t power_measure_get_voltage_multiplier(power_measure_handle_t handle, float* multiplier);
esp_err_t power_measure_get_current_multiplier(power_measure_handle_t handle, float* multiplier);
esp_err_t power_measure_get_power_multiplier(power_measure_handle_t handle, float* multiplier);
```

## Configuration

### Kconfig Options

#### Performance Optimization

- `BL0937_IRAM_OPTIMIZED`: Enable IRAM optimization for BL0937 (default: disabled)

#### BL0937 Hardware Configuration

- `BL0937_VREF_MV`: Internal voltage reference in millivolts (default: 1218)
- `BL0937_R_CURRENT_MOHM`: Current sampling resistor in milliohms (default: 1)
- `BL0937_R_VOLTAGE`: Voltage divider resistor in ohms (default: 2010)
- `BL0937_F_OSC`: Oscillator frequency in Hz (default: 2000000)
- `BL0937_PULSE_TIMEOUT_US`: Pulse timeout in microseconds (default: 1000000)

### Dependencies

- `esp_timer`: Timer functionality
- `driver`: GPIO and I2C drivers
- `i2c_bus`: I2C bus management

## Usage Examples

### BL0937 Example

```c
#include "power_measure.h"
#include "power_measure_bl0937.h"

// Common configuration
power_measure_config_t common_config = {
    .overcurrent = 15,       // 15A threshold
    .overvoltage = 260,      // 260V threshold
    .undervoltage = 180,     // 180V threshold
    .enable_energy_detection = true
};

// BL0937 specific configuration
power_measure_bl0937_config_t bl0937_config = {
    .sel_gpio = GPIO_NUM_4,
    .cf1_gpio = GPIO_NUM_7,
    .cf_gpio = GPIO_NUM_3,
    .pin_mode = 0,
    .sampling_resistor = 0.001f,    // 1mΩ
    .divider_resistor = 2010.0f,    // 2010Ω
    .ki = 1.00f,                   // Current calibration
    .ku = 1.00f,                   // Voltage calibration  
    .kp = 1.00f                    // Power calibration
};

// Create device
power_measure_handle_t power_handle;
esp_err_t ret = power_measure_new_bl0937_device(&common_config, &bl0937_config, &power_handle);
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create BL0937 device");
    return;
}

// Read measurements
float voltage, current, power, energy;
power_measure_get_voltage(power_handle, &voltage);
power_measure_get_current(power_handle, &current);
power_measure_get_active_power(power_handle, &power);
power_measure_get_energy(power_handle, &energy);

ESP_LOGI(TAG, "V: %.2fV, I: %.3fA, P: %.2fW, E: %.3fkWh", 
         voltage, current, power, energy);

// Cleanup
power_measure_delete(power_handle);
```

### INA236 Example

```c
#include "power_measure.h"
#include "power_measure_ina236.h"
#include "i2c_bus.h"

// Initialize I2C bus
i2c_config_t i2c_conf = {
    .mode = I2C_MODE_MASTER,
    .sda_io_num = GPIO_NUM_20,
    .sda_pullup_en = GPIO_PULLUP_ENABLE,
    .scl_io_num = GPIO_NUM_13,
    .scl_pullup_en = GPIO_PULLUP_ENABLE,
    .master.clk_speed = 100000,
};

i2c_bus_handle_t i2c_bus = i2c_bus_create(I2C_NUM_0, &i2c_conf);

// Common configuration
power_measure_config_t common_config = {
    .overcurrent = 15,       // 15A threshold
    .overvoltage = 260,      // 260V threshold
    .undervoltage = 180,     // 180V threshold
    .enable_energy_detection = false  // INA236 doesn't support energy detection
};

// INA236 specific configuration
power_measure_ina236_config_t ina236_config = {
    .i2c_bus = i2c_bus,
    .i2c_addr = 0x41,
    .alert_en = false,
    .alert_pin = -1,
    .alert_cb = NULL,
};

// Create device
power_measure_handle_t power_handle;
esp_err_t ret = power_measure_new_ina236_device(&common_config, &ina236_config, &power_handle);
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create INA236 device");
    return;
}

// Read measurements
float voltage, current, power;
power_measure_get_voltage(power_handle, &voltage);
power_measure_get_current(power_handle, &current);
power_measure_get_active_power(power_handle, &power);

ESP_LOGI(TAG, "V: %.2fV, I: %.3fA, P: %.2fW", voltage, current, power);

// Cleanup
power_measure_delete(power_handle);
i2c_bus_delete(&i2c_bus);
```

## Performance Considerations

- **Sampling Rate**: BL0937 provides continuous measurement, INA236 requires periodic polling
- **Accuracy**: INA236 offers higher precision for low-power applications
- **Power Consumption**: BL0937 optimized for continuous monitoring, INA236 for periodic measurement
- **Calibration**: Runtime calibration available only for BL0937
- **Energy Persistence**: Both chips support persistent energy storage

## Troubleshooting

### Common Issues

1. **Inaccurate Readings**: Check calibration factors and hardware connections
2. **No Data**: Verify GPIO/I2C connections and power supply
3. **Calibration Failures**: Ensure stable reference values during calibration
4. **Energy Reset**: Use `power_measure_reset_energy_calculation()` to clear stored energy

### Debug Tips

- Enable debug logging for detailed operation information
- Verify hardware connections with multimeter
- Monitor function return values for error handling
