# Component: touchpad

* This component defines a touchpad as a well encapsulated object.
    * Based on touchpad driver in esp-idf

* A touchpad device is defined by:
	* touchpad number (refer to comments of touch_pad_t for touchpad number and gpio number pair)
	* sensitivity which defines the sensitivity of touchpad (range from 0 to 1.0)

* A touchpad device can provide:
	* iot_tp_add_cb to add callback functions to three different events
	* iot_tp_set_serial_trigger to make touchpad trigger events continuesly while pressing
	* iot_tp_add_custom_cb to add callback function to a custom press duration
	* iot_tp_read to get the sample value of touchpad
	* iot_tp_num_get to get the touchpad number of the touchpad device
	* iot_tp_set_threshold to set the threshold value of the touchpad

* To use the touchpad device, you need to:
	* create a touchpad object return by iot_tp_create()
	* To delete the device, you can call iot_tp_delete to delete the object and free the memory

* More than one touchpad can make up a touchpad slide:
	* call iot_tp_slide_create to get a touchpad slide object
	* call iot_tp_slide_position to get relative position of your touch on slide

* If plenty of touchpads are needed, you can use tp_matrix device:
	* call iot_tp_matrix_create to get a touchpad matrix object
	* m+n touchpad sensors are required to create a touchpad matrix which can provide m*n touchpads
	* many functions of touchpad matrix device are similar with regular touchpad device

* Proximity sensor device is provided to catch a hand proximity:
	* create a proximity sensor device by proximity_sensor_create()
	* call proximity_sensor_add_cb to set the callback function of proximity sensor and callback function will be called if proximity status changes (refer to enum proximity_status_t)
	* call proximity_sensor_read to get the proximity of your hand to sensor, the closer your hand approaches sensor, the larger this value is

* Gesture sensor device is provided to recognize different gestures above sensor:
	* create a gesture sensor by gesture_sensor_create()
	* call gesture_sensor_add_cb to set the callback function of gesture sensor and callback function will be called if any type of gesture is recognized (refer to enum gesture_type_t)

* TouchSensor tune tool `ESP-Tuning Tool`.
    * ESP-Tuning Tool [EN](../../../documents/touch_pad_solution/esp_tuning_tool_user_guide_en.md) [中文](../../../documents/touch_pad_solution/esp_tuning_tool_user_guide_cn.md)
	* Monitor the data of each touch channel
	* Determine the threshold for each channel
	* Evaluate the performance of touch sensors, including sensitivity, SNR, stability, channel coupling and etc.
	* "ESP-Tuning Tool" can be downloaded from [Espressif's official website](https://www.espressif.com/en/support/download/other-tools)

>***NOTE***:
* ***For hardware and firmware design guidelines on ESP32 touch sensor system, please refer to [Touch Sensor Application Note](https://github.com/espressif/esp-iot-solution/blob/master/documents/touch_pad_solution/touch_sensor_design_en.md), where you may find comprehensive information on how to design and implement touch sensing applications, such as linear slider, wheel slider, matrix buttons and spring buttons.***
* ***Don't delete the same tp_handle more than once and you'd better set the handle to NULL after calling iot_tp_delete()***
