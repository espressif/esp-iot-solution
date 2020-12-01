# Component: is31fl13xxx

 
* This component will show you how to use I2C module contorl external LED driver and matrix driver chip.

* The IS31FL3218 is comprised of 18 constant current channels each with independent PWM control.

* The IS31FL3736 is a general purpose 12*8 LEDs matrix driver.
 
* Pin assignment:

    * master:
        * GPIO18 is assigned as the data signal of i2c master port
        * GPIO19 is assigned as the clock signal of i2c master port
 
* Connection:
 
    * connect sda of sensor with GPIO18  
    * connect scl of sensor with GPIO19
    * no need to add external pull-up resistors, driver will enable internal pull-up resistors.

