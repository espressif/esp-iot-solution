# Component: relay

* This component defines a relay as a well encapsulated object.
    * Usually a relay is only driven by a GPIO.
    * Sometimes a relay will be driven by a D flip-flop.

* A relay device is defined by:
	* gpio number(s) which control(s) the relay
	* closed level decided by peripheral hardware. If the relay you use is closed when the given voltage is high, set it to RELAY_CLOSE_HIGH, else set it to RELAY_CLOSE_LOW 
	* control mode which decides how to control the relay(by gpio or d flip-flop)
	* io mode which decides the mode of gpio(normal or rtc)

* A relay device can provide:
	* iot_relay_state_write to set the state of relay
	* iot_relay_state_read to read the state of relay

* To use the relay device, you need to:
	* create a relay device returned by iot_relay_create
	* to free the object, you can call iot_relay_delete to delete the relay object and free the memory