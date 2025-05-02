# aht20
I2C driver for Aosong AHT20 humidity and temperature sensor using esp-idf.
Tested with AHT20 using ESP32 and ESP32-S3 devkits.

# Features

    Temperature and humidity measurement

    Thread-safe via esp-i2c-driver

    CRC checksum verification (optional via menuconfig)

    Configurable I2C clock speed (pre-compilation, not runtime,via menuconfig))

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

This driver includes a demo example project.

    Follow the example to learn how to initialize the driver and read the sensor data.

    All public APIs are documented in aht20.h.



However, following are the general guiedlines.
```c
    //create a AHT20 device object and receive a device handle for it
    // my_i2c_bus_handle is a preintialized i2c_bus_handle_t object
    aht20_handle_t aht20_handle =  aht20_create( my_i2c_bus_handle, AHT20_ADDRESS_LOW ); //addresses are in aht.h

    //use the previously created AHT20 device handle for initializing the associated device
    aht20_init(aht20_handle);
    
    //read both humidity and temperature at once from device, using AHT20 device handle
    aht20_read_humiture(aht20_handle); //Other public APIs are documented in AHT20.h.

    //access the results stored in AHT20 device object, using the AHT20 device handle
    //other apis require user to explicitly pass variable address to hold data
    printf("tempertature = %.2fC  humidity = %.3f \n", aht20_handle->humiture.temperature, aht20_handle->humiture.humidity);

    //to get reaw values create a object of following data type
    aht20_raw_reading_t raw_value;
    aht20_read_raw( aht20_handle, &raw_value);
    printf("tempertature = %uC  humidity = %u \n", raw_value.temperature, raw_value.humidity);
```


# How to Configure CRC and I2C clock speed
Additionally, select in menuconfig under Component Config → AHT20,; to use CRC(default is not used)
or change the clock speed of device (default is 100KHz). 

Note : It is recommended to use clock speeds in upper ranges of 100kHz to 200kHz.
Higher clock speeds may cause occasional data inconsistencies depending on your board layout and wiring.

![image](https://github.com/user-attachments/assets/fc8680fb-1567-477c-92f8-52dd126e6f9d)

or 
In sdkconfig under Component Config → AHT20,
![image](https://github.com/user-attachments/assets/1f9612df-8d73-4ad1-bec7-75cbe6ed327a)
