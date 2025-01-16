# Component: MCP3201

MCP3201 device is a successive approximation 12-bit Analog-to-Digital (A/D) Converter with on-board sample and hold circuitry. The device provides a single pseudo-differential input.Differential Nonlinearity (DNL) is specified at ±1 LSB, and Integral Nonlinearity (INL) is offered in ±1 LSB (MCP3201-B) and ±2 LSB (MCP3201-C) versions. Communication with the device is done using a simple serial interface compatible with the SPI protocol. The device is capable of sample rates of up to 100 ksps at a clock rate of 1.6 MHz. The MCP3201 device operates over a broad voltage range (2.7V-5.5V).Low-current design permits operation with typical standby and active currents of only 500 nA and 300 μA, respectively. 

## Add component to your project

Please use the component manager command `add-dependency` to add the `mcp3201` to your project's dependency, during the `CMake` step the component will be downloaded automatically

```
idf.py add-dependency "espressif/mcp3201=*"
```

## Example of MCP3201 usage

```c
static spi_bus_handle_t spi_bus = NULL;
static mcp3201_handle_t mcp3201 = NULL;
int16_t data;

// Step1: Init SPI bus
spi_config_t bus_conf = {
    .miso_io_num = SPI_MISO_IO,
    .mosi_io_num = -1,
    .sclk_io_num = SPI_SCLK_IO,
};
spi_bus = spi_bus_create(SPI2_HOST, &bus_conf);
TEST_ASSERT(spi_bus != NULL);

// Step2: Init MCP3201
spi_device_config_t dev_cfg = {
    .mode = 0,
    .clock_speed_hz = SPI_FREQ_HZ,
    .cs_io_num = SPI_CS_IO,
};
mcp3201 = mcp3201_create(spi_bus, &dev_cfg);

// Step3: Read ADC value
mcp3201_get_data(mcp3201, &data);
ESP_LOGI(TAG, "MCP3201:%d,Convert:%.2f", data, 1.0f * data * (3.3f / 4096));
```
