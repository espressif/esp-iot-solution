# Component: touchpad

* This component defines a touchpad as a well encapsulated object.
    * Based on touchpad driver in esp-idf.

* A touchpad device is defined by:
	* touchpad number(refer to comments of touch_pad_t for touchpad number and gpio number pair) 
	* thres_percent which decide the sensitivity of touchpad (range from 0 to 999)
	* filter value which decide the smallest time interval of two touchpad triggers

* A touchpad device can provide:
	* touchpad_add_cb to add callback functions to three different events
	* touchpad_set_serial_trigger to make touchpad trigger events continuesly while pressing
	* touchpad_add_custom_cb to add callback function to a custom press duration
	* touchpad_read to get the sample value of touchpad
	* touchpad_num_get to get the touchpad number of the touchpad device
	* touchpad_set_threshold/filter to set the threshold value or filter value  mode of the touchpad

* To use the touchpad device, you need to:
	* create a touchpad object return by touchpad_create()
	* To delete the device, you can call touchpad_delete to delete the object and free the memory

* More than one touchpad can make up a touchpad slide:
	* call touchpad_slide_create to get a touchpad slide object
	* call touchpad_slide_position to get relative position of your touch on slide

* If plenty of touchpads are needed, you can use touchpad_matrix device:
	* call touchpad_matrix_create to get a touchpad matirx object
	* m+n touchpad sensors are required to create a touchpad matrix which can provide m*n touchpads
	* many functions of touchpad matrix device are similar with narmal touchpad device 

* Proximity sensor device is provided to catch a hand proximity:
	* create a proximity sensor device by proximity_sensor_create()
	* call proximity_sensor_add_cb to set the callback function of proximity sensor and callback function will be called if proximity status changes (refer to enum proximity_status_t)
	* call proximity_sensor_read to get the proximity of your hand to sensor, the closer your hand approachs sensor, the larger this value is

* Gesture sensor device is provided to recognize different gestures above sensor:
	* create a gesture sensor by gesture_sensor_create()
	* call gesture_sensor_add_cb to set the callback function of gesture sensor and callback function will be called if any type of gesture is recognized (refer to enum gesture_type_t)

### NOTE:
> Don't delete the same touchpad_handle more than once and you'd better to set the handle to NULL after calling touchpad_delete()

> For thres_percent, you can refer to follow cases:
* If the pads are touched directly, set thres_percent to 700~800
* If the pads are isolated by pcb board(about 1mm thickness), set thre_percent to 950
* If the pads are isolated by pcb board(about 1mm thickness) and a plastic plate(about 2mm thickness), set thre_percent to 990

> For filter_value, you can refer to follow cases:
* If response time is not important, set filter_value to 100~150
* If the touch event should response as fast as possible, set filter_value to 30~50
