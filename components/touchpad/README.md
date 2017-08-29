# Component: touchpad

* This component defines a touchpad as a well encapsulated object.
    * Based on touchpad driver in esp-idf.

* A touchpad device is defined by:
	* touchpad number(refer to comments of touch_pad_t for touchpad number and gpio number pair) 
	* thres_percent which decide the sensitivity of touchpad
	* filter value which decide the smallest time interval of two touchpad triggers

* A touchpad device can provide:
	* touchpad_add_cb to add callback functions to three different events
	* touchpad_set_serial_trigger to make touchpad trigger events continuesly while pressing
	* touchpad_add_custom_cb to add callback function to a custom press duration
	* touchpad_read to get the sample value of touchpad
	* touchpad_num_get to get the touchpad number of the touchpad device
	* touchpad_set_threshold/filter to set the threshold value, filter value or trigger mode of the touchpad

* To use the touchpad device, you need to:
	* create a touchpad object return by touchpad_create()
	* To delete the device, you can call touchpad_delete to delete the object and free the memory

* More than one touchpad can make up a touchpad slide:
	* call touchpad_slide_create to get a touchpad slide object
	* call touchpad_slide_position to get relative position of your touch on slide

### NOTE:
> Don't delete the same touchpad_handle more than once and you'd better to set the handle to NULL after calling touchpad_delete()

> For thres_percent, you can refer to follow cases:
* If the pads are touched directly, set thres_percent to 70~80
* If the pads are isolated by pcb board(about 1mm thickness), set thre_percent to 97
* If the pads are isolated by pcb board(about 1mm thickness) and a plastic plate(about 1mm thickness), set thre_percent to 99
