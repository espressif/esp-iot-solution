# BLE HID Remote Control Example
## Introduction

In this walkthrough, we will review the BLE HID Remote Control example code which implements a joystick using an ESP32. The example will consist of the handling of GAP and GATTS APIs, HID joystick implementation and the GPIO interface for reading input.

For more detailed explanation into specific segments of the code, kindly refer to the other IDF examples as follows:
- [GATT Server Service Table Example](https://github.com/espressif/esp-idf/tree/master/examples/bluetooth/bluedroid/ble/gatt_server_service_table)
- [ADC Read](https://github.com/espressif/esp-idf/tree/master/examples/peripherals/adc)
- [Generic HID Device](https://github.com/espressif/esp-idf/tree/master/examples/bluetooth/bluedroid/ble/ble_hid_device_demo)

## File Structures
In this example, the files are structured as follows for ease of understanding:
* `main.c` : Entry point, demonstrate initialization and the main task of handling user input and sending hid report task
* `controller.c` : Demonstrate initialization, reading and tearing down of analog and digital input, for the joystick and buttons respectively
* `gap_gatts.c` : Initializationa and processing of Generic Access Profile (GAP) and Generic Attribute Profile (GATT) events
* `hidd.c`: Define the HID profile for a joystick, and also processing of HID events specific to a joystick usecase

The header files define macros and structs that will be relevant for the corresponding `.c` file. 

## Main Entry Point

The entry point to this example is the app_main() function:

```c
void app_main(void)
{
    // Resource initialisation
    esp_err_t ret;
    ret = joystick_init();
    if (ret != ESP_OK) {
        ESP_LOGE(HID_DEMO_TAG, "%s config joystick failed\n", __func__);
        return;
    }
    ret = button_init();
    if (ret != ESP_OK) {
        ESP_LOGE(HID_DEMO_TAG, "%s config button failed\n", __func__);
        return;
    }
    ret = ble_config();
    if (ret != ESP_OK) {
        ESP_LOGE(HID_DEMO_TAG, "%s config ble failed\n", __func__);
        return;
    }
    ESP_LOGI(HID_DEMO_TAG, "BLE configured");

    // GAP and GATTS initialisation
    esp_ble_gap_register_callback(gap_event_callback);
    esp_ble_gatts_register_callback(gatts_event_callback);
    ESP_LOGI(HID_DEMO_TAG, "GAP and GATTS Callbacks registered");
    gap_set_security();
    ESP_LOGI(HID_DEMO_TAG, "Security set");
    esp_ble_gatts_app_register(ESP_GATT_UUID_HID_SVC); // Trigger ESP_GATTS_REG_EVT (see gap_gatts.c and hidd.c)
    ret = esp_ble_gatt_set_local_mtu(500);
    if (ret){
        ESP_LOGE(HID_DEMO_TAG, "set local  MTU failed, error code = %x", ret);
    }

    // Create tasks and queue to handle input events
    input_queue = xQueueCreate(10, sizeof(input_event_t));

#ifdef CONFIG_JOYSTICK_INPUT_MODE_CONSOLE
    print_console_read_help();
    xTaskCreate(joystick_console_read, "console_read_joystick", 2048, NULL, tskIDLE_PRIORITY, NULL);
#else //CONFIG_JOYSTICK_INPUT_MODE_ADC
    xTaskCreate(joystick_ext_read, "ext_hardware_joystick", 2048, NULL, tskIDLE_PRIORITY, NULL);
#endif

    // Main joystick task
    joystick_task(); 

#ifdef CONFIG_EXAMPLE_ENABLE_TEAR_DOWN_DEMO
    // Tear down, after returning from joystick_task()
    tear_down();
    ESP_LOGI(HID_DEMO_TAG, "End of joystick demo, restarting in 5 seconds");
    DELAY(5000);
    esp_restart();
#endif
}
```
The main function shows the high level programme flow of the demo, in the order of:
- Resource initialization (GPIO, Bluetooth controller)
- Setting up GAP and GATT 
- Create tasks for user input
- (if enabled) Tearing down and reset

## Initialization
The first portion of the `app_main()` function is dedicated to setting up the resources needed for the ESP Joystick. The purpose of the various functions are as follows:
- `joystick_init()` : To setup the ADC pins of the joystick for analog input readings.
- `button_init()`: To setup the digital pins and corresponding interrupt handlers for the controller buttons.
- `ble_config()`: To setup the bluetooth controller
- `esp_ble_gap_register_callback()` : An API to register the callback to respond to GAP events (advertisement & establishing connections)
- `esp_ble_gatts_register_callback()` : An API to register GATT server callback, setting the behaviour of the HID device as it responds to the client and sending user input (joystick & buttons) to the client.

### Controller Input Initialization
#### Button Inputs (Digital)
```c
esp_err_t button_init(void)
{   
    const gpio_config_t pin_config = {
        .pin_bit_mask = BUTTON_PIN_BIT_MASK,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_POSEDGE
    };

    ESP_ERROR_CHECK(gpio_config(&pin_config));

    ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1));

#ifdef CONFIG_BUTTON_INPUT_MODE_BOOT
    ESP_ERROR_CHECK(gpio_isr_handler_add(PIN_BUTTON_BOOT, button_isr_handler, (void*) PIN_BUTTON_BOOT));
#else // CONFIG_BUTTON_INPUT_MODE_GPIO
    ESP_ERROR_CHECK(gpio_isr_handler_add(PIN_BUTTON_A, button_isr_handler, (void*) PIN_BUTTON_A));
    ESP_ERROR_CHECK(gpio_isr_handler_add(PIN_BUTTON_B, button_isr_handler, (void*) PIN_BUTTON_B));
    ESP_ERROR_CHECK(gpio_isr_handler_add(PIN_BUTTON_C, button_isr_handler, (void*) PIN_BUTTON_C));
    ESP_ERROR_CHECK(gpio_isr_handler_add(PIN_BUTTON_D, button_isr_handler, (void*) PIN_BUTTON_D));
#endif

    esp_timer_early_init();
    return ESP_OK;
}
```
Setting up the button pins involve the following:
- Configuring the pin properties
- Attaching the interrupt service routine (ISR) to the pins, with the corresponding priority level
- Starting the timer for simple debouncing

> Note:
> User should take note of the fields `.pull_up_en` and `.pull_down_en` if using external hardware buttons

> User can consider using the button component from the ESP Component Registry for debouncing and various button events (long press, double press etc): [link](https://components.espressif.com/components/espressif/button), [repository](https://github.com/espressif/esp-iot-solution)

> User can pick the Button Pins using `idf.py menuconfig` under `Example Configuration`, the options will be available if `Button input mode (GPIO)` is selected; Alternatively, user can change the macros as defined in `controller.h` to select the button input pins.

#### Joystick Inputs (Analog)

```c
esp_err_t joystick_init(void) 
{
    // ADC Init
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));

    // ADC Config
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH,
        .atten = ADC_ATTEN,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, JOYSTICK_IN_X_ADC_CHANNEL, &config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, JOYSTICK_IN_Y_ADC_CHANNEL, &config));

    // ADC Calibration
    esp_err_t ret;
    ret = adc_calibration_init(JOYSTICK_IN_X_ADC_CHANNEL, &calibrate_handle_x);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ADC Calibration failed for X axis");
        return ret;
    }
    ret = adc_calibration_init(JOYSTICK_IN_Y_ADC_CHANNEL, &calibrate_handle_y);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ADC Calibration failed for Y axis");
        return ret;
    }

    return ESP_OK;
}
```
Setting up the ADC Pins for Joystick input involves the following:
- Configuring the ADC Unit & Channel
- Initialize ADC calibration unit

> Note:
> User should check the Hardware specification datasheet of the specific chipset used, as each chipset might have specific hardware limitations on the ADC units & channels

### Bluetooth Initialization

```c
/**
 * @brief Initialize bluetooth resources
 * @return ESP_OK on success; any other value indicates error
*/
esp_err_t ble_config(void)
{
    esp_err_t ret;
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(HID_DEMO_TAG, "%s initialize controller failed\n", __func__);
        return ESP_FAIL;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(HID_DEMO_TAG, "%s enable controller failed\n", __func__);
        return ESP_FAIL;
    }

    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(HID_DEMO_TAG, "%s init bluedroid failed\n", __func__);
        return ESP_FAIL;
    }

    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(HID_DEMO_TAG, "%s init bluedroid failed\n", __func__);
        return ESP_FAIL;
    }

    return ESP_OK;
}
```
Before initialization of the controller, we first erase the default Non-Volatile Storage (NVS) and free unused BT Classic memory to the heap.

We then initializes the BT controller by first creating a BT controller configuration structure named `esp_bt_controller_config_t` with default settings generated by the `BT_CONTROLLER_INIT_CONFIG_DEFAULT()` macro. 

The BT controller implements the Host Controller Interface (HCI) on the controller side, the Link Layer (LL) and the Physical Layer (PHY). The BT Controller is invisible to the user applications and deals with the lower layers of the BLE stack. The controller configuration includes setting the BT controller stack size, priority and HCI baud rate. With the settings created, the BT controller is initialized and enabled with the `esp_bt_controller_init()` function:

After the initialization of the BT controller, the Bluedroid stack, which includes the common definitions and APIs for both BT Classic and BLE, is initialized (`esp_bluedriod_init()`) and enabled (`esp_bluedriod_enable()`).

The Bluetooth stack is up and running at this point in the program flow, however the functionality of the application has not been defined yet. The functionality is defined by reacting to events such as what happens when another device tries to read or write parameters and establish a connection. This would require the GAP and GATT event handlers as shown below.

### GAP and GATTS Registration 
```c
// GAP and GATTS initialisation
esp_ble_gap_register_callback(gap_event_callback);
esp_ble_gatts_register_callback(gatts_event_callback);
ESP_LOGI(HID_DEMO_TAG, "GAP and GATTS Callbacks registered");
```
The above API registers the GAP and GATT event handlers/callbacks. 

The functions `gap_event_callback()` and `gatts_event_callback()` will be the ones to handle all the events that are pushed to the application from the BLE stack. The full list of events under GAP and GATTS are described in `esp_gap_ble_api.h` and `esp_gatts_api.h` respectively.

### Human Interface Device Profile 
> Note: User should be familiar with the Bluetooth HID Over GATT Profile (HOGP) specification and the USD HID Specification for this segment

For this example, only the mandatory Services and Characteristics are implemented. 

For the HID Profile, the mandatory services and characteristics are as follow:
- HID Service 				(UUID: 0x1812)
    - HID Information Characteristic	(UUID: 0x2A4A)
    - Report Map Characteristic			(UUID: 0x2A4B)
    - HID Control Point Characteristic	(UUID: 0x2A4C)
    - Report Characteristic				(UUID: 0x2A4D)
- Battery Service			(UUID: 0x180F)
    - Battery Level Characteristic		(UUID: 0x2A19)
- Device Information Service	(UUID: 0x180A)
    - Manufacturer Name String Characteristic	(UUID: 0x2A29)
    - PnP ID Characteristic						(UUID: 0x2A50)

The UUIDs are also defined in `esp_gatt_defs.h`. 

The GATT Services are created using attribute tables following GATT Service Table Example.

For example, the Battery Service attribute table is defined as follow: 
```c
// Attribute table for Battery Service
const esp_gatts_attr_db_t attr_tab_bat_svc[LEN_BAT_SVC] =
{
    [IDX_BAT_SVC] = {   // Service declaration: to describe the properties of a service
        {ESP_GATT_AUTO_RSP},  // Attribute Response Control (esp_attr_control_t)
        {                   // Attribute description (esp_attr_desc_t)
            ESP_UUID_LEN_16,            // UUID length
            (uint8_t *) &UUID_PRI_SVC,  // UUID pointer
            ESP_GATT_PERM_READ,         // Permissions
            sizeof(uint16_t),           // Max length of element
            sizeof(UUID_BAT_SVC),       // Length of element
            (uint8_t *) &UUID_BAT_SVC,  // Pointer to element value
        },
    },
    [IDX_BAT_LVL_CHAR] = { // Characteristic declaration: to describe the properties of a characteristic
        {ESP_GATT_AUTO_RSP},
        {
            ESP_UUID_LEN_16, 
            (uint8_t *) &UUID_CHAR_DECLARE, 
            ESP_GATT_PERM_READ,
            sizeof(uint8_t),
            sizeof(CHAR_PROP_READ_NOTIFY),
            (uint8_t *) &CHAR_PROP_READ_NOTIFY,
        },
    },
    [IDX_BAT_LVL_VAL] = { // Characteristic value: to contain the actual value of a characteristic
        {ESP_GATT_AUTO_RSP},
        {
            ESP_UUID_LEN_16, 
            (uint8_t *) &UUID_BAT_LVL_VAL, 
            ESP_GATT_PERM_READ,
            sizeof(uint8_t),
            sizeof(val_batt_level),
            (uint8_t *) &val_batt_level,
        },
    },
};
```
One thing to take note about HID devices (in this case a joystick) is the report descriptor that describes the format and organization of the data exchanged between a HID device and a host system. It provides a standardized way to define the capabilities and characteristics of the HID device, including the types of data it can send or receive and the structure of the data packets.

For this demo, we will be using the example Joystick report descriptor (with minor modification) provided in the USB Device Class Definition (v1.11) appendix. See `hidd.c` for more details. 

The characteristics and descriptor values can be obtained from the HOGP and USB HID specification documents. 

In this demo, we also defined `gatts_profile_inst_t` to store various information of a server profile. As shown:
```c
/**
 * @brief   GAP GATT Server Profile
 * @details Store information about the server profile for easier access (such as for tear down) 
 * @details Allows for multiple profiles to be registered
*/
typedef struct {
    uint16_t gatts_interface;
    uint16_t app_id;
    uint16_t conn_id;
    esp_gatts_cb_t callback;
    esp_bd_addr_t remote_bda;
} gatts_profile_inst_t;
``` 
As we are creating a HID profile for the joystick, we create a profile instance as shown: 
```c
static gatts_profile_inst_t hid_profile = {
    .gatts_interface = ESP_GATT_IF_NONE,    // default value
    .callback = &hid_event_callback,        // user defined callback function (in hidd.c)
};
```
The `hid_event_callback()` function describes how GATTS are handled specifically by the HID profile. (see `hidd.c` for more details).

## Program Flow

### GATT Server Events
The sequence of GATTS events are as follows:

1. Registration of GATT Server application identifier (`esp_ble_gatts_app_register()` in `app_main()`)
    - `ESP_GATTS_REG_EVT` is triggered, the event is cascaded down to the GATT profile, for this demo, we only have the HID profile.
    - `esp_ble_gap_config_adv_data()` is called in this subroutine which triggers `ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT` once complete
    - The device name, gatts_interface, local icon are configured in this event. (see `gap_gatts.c`)
    - The GATT server attribute tables are created (via `esp_ble_gatts_create_attr_tab()`) as handled by the HID profile. This triggers `ESP_GATTS_CREAT_ATTR_TAB_EVT` once done
2. Attribute table creation complete (`ESP_GATTS_CREAT_ATTR_TAB_EVT`)
    - Once the attribute table is created, the attribute handles are stored (in `handle_table`, defined in `hidd.h`) as they will be needed to send notification to the clients.
    - Proceed to start the services with `esp_ble_gatts_start_service()`, triggering `ESP_GATTS_START_EVT`
3. Receive connection (`ESP_GATTS_CONNECT_EVT`)

### GAP Events
The sequence of GAP events are as follows:

1. Configuration of Advertising Data (`ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT`)
    - `esp_ble_gap_config_adv_data()` is called during the registration of GATTS 
    - The advertising behaviour is configured as specified by `esp_ble_adv_data_t adv_data`
    - `ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT` is triggered once the above is done.
2. Advertising of HID Device (`ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT`)
    - The bluetooth controller starts advertising the device. 
    - Trigger `ESP_GAP_BLE_ADV_START_COMPLETE_EVT`
3. Agreement of Connection Parameters (`ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT`)
    - When a connection is received, the client may request to update connection parameter

## Send HID User Report

### Flow Setup
```c
// Create tasks and queue to handle input events
    input_queue = xQueueCreate(10, sizeof(input_event_t));

#ifdef CONFIG_JOYSTICK_INPUT_MODE_CONSOLE
    print_console_read_help();
    xTaskCreate(joystick_console_read, "console_read_joystick", 2048, NULL, tskIDLE_PRIORITY, NULL);
#else //CONFIG_JOYSTICK_INPUT_MODE_ADC
    xTaskCreate(joystick_ext_read, "ext_hardware_joystick", 2048, NULL, tskIDLE_PRIORITY, NULL);
#endif

    // Main joystick task
    joystick_task(); 
```
An FreeRTOS queue is created to receive the different input events pushed to the queue.

### Input detection & Processing
Multiple tasks are created to detect changes in user controller input, the behaviours are as follows:
1. `joystick_console_read()` : (if in `JOYSTICK_INPUT_MODE_CONSOLE`) Emulate joystick movements by reading console input and mapping it to one of 9 controller directions (see `char_to_joystick_input()`)
2. `joystick_ext_read()` : (if in `JOYSTICK_INPUT_MODE_ADC`) Read actual joystick inputs based on variable voltage (see [tutorial](hardware_tutorial.md) here) and push to RTOS queue if the changes in voltage is more than the 10% threshold (defined in `controller.h`)
3. `button_isr_handler()` : (used in both `BUTTON_INPUT` modes) An ISR that sends to RTOS queue whenever a positive edge is detected on the button. 

As a demonstration, we defined a simple structure to pass information to `joystick_task()` as follows:

```c
/**
 * @brief   Simple struct to store input event data in RTOS queue
 * @note    This struct serves as an simplified implementation
 *          Up to user modify for more complex use cases or performance
*/
typedef struct {
    uint8_t input_source;
    uint8_t data_button;
    uint8_t data_console;
    uint8_t data_joystick_x;
    uint8_t data_joystick_y;
} input_event_t;
```

In `joystick_task()`, the controller input will be processed in the following sequence:
- event detected
- check source of input
- retrieve input information
- set HID report
- send to client

```c
// Send report if there are any inputs (refer to controller.c)
if (xQueueReceive(input_queue, &input_event, 100) == pdTRUE) {
    switch (input_event.input_source) {
    case (INPUT_SOURCE_BUTTON):
        button_in = input_event.data_button;
        joystick_adc_read(&x_axis, &y_axis);  
        ESP_LOGI(HID_DEMO_TAG, "Button input received");
        break;
    case (INPUT_SOURCE_CONSOLE):
        button_read(&button_in);
        char_to_joystick_input(input_event.data_console, &x_axis, &y_axis);
        ESP_LOGI(HID_DEMO_TAG, "Console input received");
        break;
    case (INPUT_SOURCE_JOYSTICK):
        button_read(&button_in);
        x_axis = input_event.data_joystick_x;
        y_axis = input_event.data_joystick_y;
        ESP_LOGI(HID_DEMO_TAG, "Joystick input received");
        break;
    default:
        ESP_LOGE(HID_DEMO_TAG, "Unknown input source, source %d", input_event.input_source);
        break;
    }

    set_hid_report_values(x_axis, y_axis, button_in, hat_switch, throttle);
    print_user_input_report(x_axis, y_axis, hat_switch, button_in, throttle);
    ret = send_user_input();
} 
```

## Tearing Down
> Note: 
> This feature is only available if both `BUTTON_INPUT_MODE_GPIO` and `EXAMPLE_ENABLE_TEAR_DOWN_DEMO` are enabled in `idf.py menuconfig`

The tear down sequence is triggered when the button sequence representing the `TEAR_DOWN_BIT_MASK` is pressed simultaneously. 

The program would return from `joystick_task()`, and initiate the tear down. 

At the high level, the tear down sequence is as follows:
```c
/**
 * @brief Tear down and free resources used
*/
esp_err_t tear_down(void)
{
    ESP_ERROR_CHECK(joystick_deinit());
    ESP_ERROR_CHECK(button_deinit());
    ESP_ERROR_CHECK(gap_gatts_deinit());

    ESP_ERROR_CHECK(esp_bluedroid_disable());
    ESP_ERROR_CHECK(esp_bluedroid_deinit());
    ESP_ERROR_CHECK(esp_bt_controller_disable());
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));
    ESP_ERROR_CHECK(nvs_flash_erase());

    return ESP_OK;
}
```
Each of the tear down functions are briefly as follows:
- `joystick_deinit()`: delete adc handle and tear down the calibration handle
- `button_deinit()` : remove the isr handle and reset GPIO pins to default
- `gap_gatts_deinit()` : close the connection and unregister the HID profile

The remaining tear down functions are API as defined in ESP documentations, kindly refer to the [ESP-IDF API Guides](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/index.html)

## Conclusion
In this demonstration, we looked at the program flow of creating a BLE HID Remote Control using an ESP32.

Creating a BLE Remote Control involved multiple components including:
- ADC for joystick input
- GPIO for button input
- GAP and GATTS for bluetooth connection
- BLE and HID specifications to define behaviour of controller

We hope that this example would prove useful for anyone who would like to create their own controller using an ESP. The example is written in a way to be easily understandable and customizable for your own use (such as changing HID report descriptor to accommodate extra buttons etc).

## Future Reference
- [Espressif ADC API](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/adc_oneshot.html)
- [Espressif GPIO API](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/gpio.html)
- [Espressif BLE GAP API](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/bluetooth/esp_gap_ble.html)
- [Espressif BLE GATTS API](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/bluetooth/esp_gatts.html)
- [USB Human Interface Device Specification](https://www.usb.org/hid)
- [Bluetooth Specifications, HID Profile and HOGP](https://www.bluetooth.com/specifications/specs/)

