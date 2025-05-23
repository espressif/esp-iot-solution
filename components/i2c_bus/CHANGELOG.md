# ChangeLog

## v1.4.0 - 2025-3-13

### Enhancements:

- Support removing the restriction on ``NULL_I2C_MEM_ADDR``, allowing users to refer to all eligible register addresses.

## v1.3.0 - 2025-2-13

### Enhancements:

- ``i2c_bus_v2`` supports initialization using the bus_handle provided by ``esp_driver_i2c``, and also supports returning the internal bus_handle of ``esp_driver_i2c``.

## v1.2.0 - 2025-1-14

### Enhancements:

- Support enabling software I2C to extend the number of I2C ports.

## v1.1.0 - 2024-11-22

### Enhancements:

- Support manual selection of ``driver/i2c`` or ``esp_driver_i2c`` in idf v5.3 and above.

## v1.0.0 - 2024-9-19

### Enhancements:

- Component version maintenance and documentation enhancement.
- Support `esp_driver_i2c` driver.

## v0.1.0 - 2024-5-27

First release version.

- Support I2C bus
