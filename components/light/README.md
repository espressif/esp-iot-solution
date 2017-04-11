# Component: Light

* This component defines a light as a well encapsulated object.
* A light device is defined by:
    * ledc timer which is used to control the pwm channels of light
    * mode of the ledc timer
    * frequency of the ledc timer
    * pwm channel number of the light
    * bit number of the ledc timer
* A light device can provide:
    * light_channel_regist function to add channel to corresponding channel id
    * light_duty_write function to set the duty of corresponding channel and it support setting duty directly or gradually
    * light_breath_write function to set the corresponding channel to breath mode and breath period can be set
    * light_blink_start and light_blink_stop function to make some of channels to blink in appointed period. Note that if any channel works in blink mode, all the other channels would be turned off.

* To use the light device, you need to:
    * create a light object returned by light_create()
    * regist the light channels according the channel number by light_channel_regist()
    * To free the object, you can call light_delete to delete the button object and free the memory.

### NOTE:
> If any channel(s) work(s) in blink mode, all the other channels would be turned off. light_blink_stop() must be called before setting any channel to other mode(write duty or breath). 