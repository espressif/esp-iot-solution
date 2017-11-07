# Component: param

* This component provide api to save/load parameters to/from SPI flash with protect, which means the parameters will be garanteed to be complete at any time)

* Call iot_param_save() to save parameter to flash.
* Call iot_param_load() to load parameter from flash. 

### NOTE:
> Call nvs_flash_init() at first if you want to use this component.

# Todo: 

* To use `nvs` APIs for saving/loading data
* To add magic code and `checksum`