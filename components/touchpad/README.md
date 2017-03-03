# Component: touchpad

* This component defines a touchpad as a well encapsulated object.
    * Based on touchpad driver in esp-idf.

* A touchpad device is defined by:
	* touchpad number(refer to comments of touch_pad_t for touchpad number and gpio number pair) 
	* threshold of the touchpad
	* filter value which decide the smallest time interval of two touchpad triggers
	* trigger mode which decide whether touchpad events trigger continuously or not while touchpad is pressed
	* queue of freertos for message send and receive

* A touchpad device can provide:
	* a message queue which contains the handle of touchpad pressed
	* touchpad_read to get the sample value of touchpad
	* touchpad_num_get to get the touchpad number of the touchpad device
	* touchpad_set_threshold/filter/trigger to set the threshold value, filter value or trigger mode of the touchpad

* To use the touchpad device, you need to:
	* create a touchpad object return by touchpad_create()
	* To delete the device, you can call touchpad_delete to delete the object and free the memory

### NOTE:
> Don't delete the same touchpad_handle more than once and you'd better to set the handle to NULL after call touchpad_delete()

> This module send different types of event to message queue and you have to receive message from queue to deal with events.(refer to touchpad_test.c)
	* The message queues are created in function touchpad_create() and you have to set the xQueueHandle which queue_ptr point to to NULL.
	* A touchpad can alse share the same queue with other touchpads. In this case, queue_ptr of touchpad_create() must point to the xQueueHandle of other touchpad.