## Steps to configure BMM150 Interrupts
1. Set the value of "desired_settings" from the list of macros from **TABLE 1.0-Macros for enabling the desired_settings**
    to enable the configuration of the selected settings. (Multiple settings can be set by bitwise OR-ing of the macros)
	>   Ex. desired_settings = BMM150_SEL_DRDY_PIN_EN | BMM150_SEL_DRDY_POLARITY;
	
2. Set the desired configuration in the dev structure using macros from the **TABLE 1.1-Interrupt configuration settings**.
	> NOTE : Configuration settings will be set only when we set the corresponding "desired_settings" which enables the settings.
``` c
	/* Select the DRDY pin and polarity settings */
	desired_settings = BMM150_SEL_DRDY_PIN_EN | BMM150_SEL_DRDY_POLARITY;
	/* Set the desired configurations of DRDY pin and its polarity */
  	dev->settings.int_settings.drdy_pin_en = BMM150_INT_ENABLE;
	dev->settings.int_settings.drdy_polarity = BMM150_ACTIVE_LOW_POLARITY;
```
3. Invoke the "bmm150_set_sensor_settings" API after setting the desired configuration in dev structure and enabling the corresponding "desired_settings" value,

4. Invoke the "bmm150_get_interrupt_status" API to get the interrupt status value in the dev->int_status.

   The value of dev->int_status is taken and a bitwise AND operation is performed with predefined interrupt status macros (listed below) to find the interrupt status which is either set or reset.

   - BMM150_INT_THRESHOLD_X_LOW 
   - BMM150_INT_THRESHOLD_Y_LOW    
   - BMM150_INT_THRESHOLD_Z_LOW    
   - BMM150_INT_THRESHOLD_X_HIGH	   
   - BMM150_INT_THRESHOLD_Y_HIGH	   
   - BMM150_INT_THRESHOLD_Z_HIGH	   
   - BMM150_INT_DATA_OVERFLOW   
   - BMM150_INT_DATA_OVERRUN 	   
   - BMM150_INT_DATA_READY		   
   
``` c
 /* Example for checking interrupt status*/
 if (dev->int_status & BMM150_INT_ASSERTED_DRDY) {
 	/*Occurrence of data ready interrupt */
	printf("\n Interrupt asserted");
 } else {
 	/* No interrupt occurred */
	printf("\n Interrupt not asserted");
 }
```   

> NOTE : The interrupt configuration settings which are set can be read from the sensor by invoking the "bmm150_get_sensor_settings" API.

> NOTE : In BMM150 the interrupt pin INT is in default latched state and the polarity of INT and DRDY pins in default is active high.

> NOTE : The macros mentioned below are present in the bmm150_defs.h file for setting the desired user configuration.
 

   ### TABLE 1.0-Macros for enabling the desired_settings	
Macros to set for "desired_settings" | Settings which can be enabled/Disabled
-------------------------------------|----------------------------------------------------------
BMM150_SEL_DRDY_PIN_EN               | Data ready interrupt mapping to DRDY PIN 		
BMM150_SEL_INT_PIN_EN                | Low-Threshold, High-Threshold and Overflow interrupts mapping to INT PIN 
BMM150_SEL_DRDY_POLARITY             | Polarity of DRDY pin 
BMM150_SEL_INT_LATCH                 | Latch settings of INT pin
BMM150_SEL_INT_POLARITY              | Polarity of INT pin
BMM150_SEL_DATA_OVERRUN_INT          | Data overrun interrupt setting
BMM150_SEL_OVERFLOW_INT              | Data overflow interrupt setting
BMM150_SEL_HIGH_THRESHOLD_INT        | High threshold interrupt setting
BMM150_SEL_LOW_THRESHOLD_INT         | Low threshold interrupt setting
BMM150_SEL_LOW_THRESHOLD_SETTING     | Setting Low threshold value to trigger interrupt 
BMM150_SEL_HIGH_THRESHOLD_SETTING    | Setting High threshold value to trigger interrupt
  

   ### TABLE 1.1-Interrupt configuration settings
|User configuration in the dev structure     |Macros which can be assigned| Significance  
|-------------------------------------------|--------------------------- |-----------------------------------------------
|dev->settings.int_settings.drdy_pin_en     | BMM150_INT_ENABLE          | Maps the data ready interrupt to the DRDY pin of sensor
|^                                          | BMM150_INT_DISABLE         | Disables the mapping of data ready interrupt signal to the DRDY pin
||||                                                                  
|dev->settings.int_settings.int_pin_en      | BMM150_INT_ENABLE          | Enables the mapping of Low-Threshold, High-Threshold and Overflow interrupts to INT (interrupt) pin 
|^                                          | BMM150_INT_DISABLE         | Disables the interrupt mapping to the INT pin
|||| 
|dev->settings.int_settings.drdy_polarity   | BMM150_ACTIVE_HIGH_POLARITY| Sets the interrupt signal polarity in DRDY pin as active high
|^                                          | BMM150_ACTIVE_LOW_POLARITY | Sets the interrupt signal polarity in DRDY pin as active low 
|||| 
|dev->settings.int_settings.int_polarity    | BMM150_ACTIVE_HIGH_POLARITY| Sets the interrupt signal polarity in INT pin as active high
|^                                          | BMM150_ACTIVE_LOW_POLARITY | Sets the interrupt signal polarity in INT pin as active low 
|||| 
|dev->settings.int_settings.int_latch       | BMM150_LATCHED             | Sets the interrupt signal in INT pin in latched mode
|^                                          | BMM150_NON_LATCHED         | Sets the interrupt signal in INT pin in non - latched mode
|||| 
|dev->settings.int_settings.data_overrun_en | BMM150_INT_ENABLE          | Enables the data overrun interrupt
|^                                          | BMM150_INT_DISABLE         | Disables the data overrun interrupt 
|||| 
|dev->settings.int_settings.overflow_int_en | BMM150_INT_ENABLE          | Enables the overflow interrupt
|^                                          | BMM150_INT_DISABLE         | Disables the overflow interrupt 
|||| 
|dev->settings.int_settings.high_int_en     | BMM150_THRESHOLD_X         | Enables the threshold interrupt detection for X axis
|dev->settings.int_settings.high_int_en     | BMM150_THRESHOLD_Y         | Enables the threshold interrupt detection for Y axis
|^                                          | BMM150_THRESHOLD_Z         | Enables the threshold interrupt detection for Z axis
|^                                          |(BMM150_THRESHOLD_X & BMM150_THRESHOLD_Y) |   Enables the threshold interrupt detection for x,y axes 
|^                                          |(BMM150_THRESHOLD_X & BMM150_THRESHOLD_Y &  BMM150_THRESHOLD_Z) | Enables the threshold interrupt interrupt detection for all 3  X,Y and Z axes
 
 
### Configuring and handling BMM150 Interrupts 
#### Example for using high threshold interrupt
> Configuring the high threshold interrupt 
``` c
int8_t high_threshold_interrupt(struct bmm150_dev *dev)
{
	int8_t rslt;
	uint16_t desired_settings;

	/* Enable the desired settings to be set in the sensor */
	desired_settings = BMM150_SEL_HIGH_THRESHOLD_INT | BMM150_SEL_HIGH_THRESHOLD_SETTING |
		BMM150_SEL_INT_PIN_EN | BMM150_SEL_INT_POLARITY | BMM150_SEL_INT_LATCH;
	
	/* Set the desired configuration */
	/* Enable the mapping of interrupt to the interrupt pin*/
	dev->settings.int_settings.int_pin_en = BMM150_INT_ENABLE;
	/* Set the interrupt in non latched mode */
	dev->settings.int_settings.int_latch = BMM150_NON_LATCHED;
	/* Set the interrupt polarity as active high */
	dev->settings.int_settings.int_polarity = BMM150_ACTIVE_HIGH_POLARITY;
	/* Set the threshold value as 0x0A (10*6 = 60 micro-tesla)*/
	dev->settings.int_settings.high_threshold = 0x0A;
	/* Set the high threshold detection for all 3 axis */
	dev->settings.int_settings.high_int_en = BMM150_THRESHOLD_X & BMM150_THRESHOLD_Y & BMM150_THRESHOLD_Z;
	
	/* Set the configurations in the sensor */
	rslt = bmm150_set_sensor_settings(desired_settings, dev);
	
	return rslt;
}
```
> Read the interrupt status continuously to check for interrupt assertion (Polling method)
``` c
int8_t high_threshold_interrupt_handling(struct bmm150_dev *dev)
{
	int8_t rslt;
	
	rslt = bmm150_get_interrupt_status(dev);
	if (rslt == BMM150_OK) {
		if (dev->int_status & BMM150_INT_THRESHOLD_X_HIGH) {
			printf("\n HIGH THRESHOLD INTERRUPT ON X AXIS ");
		}
		if (dev->int_status & BMM150_INT_THRESHOLD_Y_HIGH) {
			printf("\n HIGH THRESHOLD INTERRUPT ON Y AXIS ");
		}
		if(dev->int_status & BMM150_INT_THRESHOLD_Z_HIGH) {
			printf("\n HIGH THRESHOLD INTERRUPT IN Z AXIS ");
		}
	}
	
	return rslt;
}
```

#### Example for using multiple interrupts (data ready and overflow interrupts)
> Configuring the overflow and data ready interrupts and interrupt pins and latch settings 
``` c
int8_t drdy_overflow_int_setting(struct bmm150_dev *dev)
{
	int8_t rslt;
	uint16_t desired_settings;

	/* Enable the desired settings to be set in the sensor */
	desired_settings = BMM150_SEL_DRDY_PIN_EN | BMM150_SEL_OVERFLOW_INT |
		BMM150_SEL_INT_PIN_EN | BMM150_SEL_INT_LATCH;
	
	/* Set the desired configuration */
	/* Enable the drdy interrupt  */
	dev->settings.int_settings.drdy_pin_en = BMM150_INT_ENABLE;
	/* Enable the mapping of interrupt to the interrupt pin */
	dev->settings.int_settings.int_pin_en = BMM150_INT_ENABLE;
	/* Enable the overflow interrupt */
	dev->settings.int_settings.overflow_int_en = BMM150_INT_ENABLE;
	/* Set the interrupt in non latched mode */
	dev->settings.int_settings.int_latch = BMM150_NON_LATCHED;

	/* Set the configurations in the sensor */
	rslt = bmm150_set_sensor_settings(desired_settings, dev);
	
}
```

> Read the interrupt status continuously to check for interrupt assertion (Polling method)
``` c
int8_t drdy_overflow_int_handling(struct bmm150_dev *dev)
{
	int8_t rslt;

	rslt = bmm150_get_interrupt_status(dev);
	if (rslt == BMM150_OK) {
		/* Multiple interrupt assertion can be checked as follows */
		if ((dev->int_status & BMM150_INT_ASSERTED_DRDY) ||
			(dev->int_status & BMM150_INT_DATA_OVERFLOW)) {
			/* Either data ready or overflow has occurred */
			printf("\n Interrupt asserted ");
		} else {
			printf("\n Interrupt not asserted ");
		}
	}
	
	return rslt;
}
```
