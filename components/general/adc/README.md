# Component: adc

* This component defines an adc as a well encapsulated object.

* An adc object is defined by:
	* `channel` ADC channel（based on GPIO used）
	* `atten` Attenuation level
	* `unit` ADC unit index

* An adc object can provide:
  * read voltage

### NOTE:
> The voltage range should be [0 ~ 1.1V].