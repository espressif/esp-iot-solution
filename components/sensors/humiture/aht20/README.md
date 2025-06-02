# Component: AHT20
I2C driver with Sensor Hub support for Aosong AHT20 humidity and temperature sensor using esp-idf.
Tested with AHT20 using ESP32 and ESP32-S3 devkits.

# Features

    Temperature and humidity measurement

    Thread-safe via esp-i2c-driver

    CRC checksum verification (optional, only via menuconfig)

    Configurable I2C clock speed (pre-compilation, not runtime, only via menuconfig)

    Unit tested 
         
         
         ┌───────────────────┐
         │    Application    │
         └────────┬──────────┘
                  │
                  ▼
         ┌───────────────────┐
         │    AHT20 Driver   │
         │ (this component)  │
         └────────┬──────────┘
                  │
                  ▼
         ┌───────────────────┐
         │ i2c_bus component │
         └────────┬──────────┘
                  │
                  ▼
         ┌───────────────────┐
         │     I2C Bus       │
         └────────┬──────────┘
                  │
                  ▼
     ┌────────────────────────────┐
     │ AHT20 Temperature/Humidity │
     │          Sensor            │
     └────────────────────────────┘



# How To Use

All public APIs are documented in aht20.h.

## Driver

Following are the general guidelines.
```c
    //create a AHT20 device object and receive a device handle for it
    // my_i2c_bus_handle here is a preintialized i2c_bus_handle_t i2c_bus object
    aht20_handle_t aht20_handle =  aht20_create( my_i2c_bus_handle, AHT20_ADDRESS_LOW ); //addresses are in aht20.h

    //use the previously created AHT20 device handle for initializing the AHT20 
    aht20_init(aht20_handle);
    
    float_t temperature;

    aht20_read_temperature( aht20_handle, &temperature);

    printf("Temperature = %.2f°C\n", temperature);

    vTaskDelay(pdMS_TO_TICKS(2000));
    
    float_t temperature;

    aht20_read_temperature( aht20_handle, &temperature);

    printf("Temperature = %.2f°C\n", temperature);
```

## Senosr Hub

Following are the general guidelines.
``` 
    
    /*create a sensor with specific sensor_id and configurations*/
    iot_sensor_create("aht20", &sensor_config, &sensor_handle); 
   
     /*register handler with sensor's handle*/
    iot_sensor_handler_register(sensor_handle, sensor_event_handler, NULL);

    /*start the sensor, data ready events will be posted once data is acquired successfully*/
    iot_sensor_start(sensor_handle); 
 
```

## How to Configure CRC and I2C clock speed
Additionally, select in menuconfig under Component Config → AHT20; to use CRC(default is not used)
or change the clock speed of device (default is 100KHz). 

Note : It is recommended to use clock speeds in upper ranges of 100kHz to 200kHz.
Higher clock speeds may cause occasional data inconsistencies depending on your board layout and wiring.

![image](https://github.com/user-attachments/assets/fc8680fb-1567-477c-92f8-52dd126e6f9d)

or 
In sdkconfig under Component Config → AHT20,
![image](https://github.com/user-attachments/assets/1f9612df-8d73-4ad1-bec7-75cbe6ed327a)
