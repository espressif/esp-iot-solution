# Component: power_meter

* The power meter module uses hardware pulse counter to calculate the measurement result from the external power meter.
* First of all, call iot_powermeter_create to create a handle to manage the power meter device. You can decide which pin and pcnt unit to use for power, voltage and current measurement.
* Different mode of power meter is shown:
	1. A power meter has a power output pin, a voltage output pin and a current output pin and has not a mode-select pin.
	In this case, you should set different value to pm_config_t.xxx_io_num, pm_config_t.xxx_pcnt_unit and pm_config_t.xxx_ref_param.
	pm_config_t.sel_io_num must be set to 0xff and pm_config_t.pm_mode must be set to PM_BOTH_VC.
	iot_powermeter_change_mode can't be called in this case.
	2. (`Usually in this mode`)A power meter has a power output pin, a voltage/current pin and a mode-select pin and the mode-select pin is connected to gpio to change mode while program is running.
	In this case, pm_config_t.xxx_io_num and pm_config_t.xxx_pcnt_unit of voltage and current must be set to the same value because voltage and current are outputed by the same pin. However, pm_config_t.xxx_ref_param of voltage and current must be set to different values.For example, if the voltage/current pin output voltage when the level of mode-select pin is set to high, you have to set pm_config_t.sel_level to 1 and set pm_config_t.pm_mode to PM_SINGLE_VOLTAGE. It is the same for current. You can call iot_powermeter_change_mode in this case to change the value output by voltage/current pin.
	3. A power meter has a power output pin, a voltage/current pin and a mode-select pin and the mode-select pin is connected to either VCC or GND.
	In this case, you can't change the value outputed by voltage/current pin because mode-select pin is fixed in advance.For example, if you choose the voltage mode(voltage/current pin outputs voltage),set pm_config_t.current_io_num and pm_config_t.sel_io_num to 0xff and set pm_config_t.pm_mode to PM_SINGLE_VOLTAGE.iot_powermeter_change_mode can't be called in this case.

* The output of power pin and voltage/current pin is pulse sequence.The actual value of power(or vaoltage and current) is in direct proportion to the frequency of pulse sequence.They meet the following formula: value_ref * period_ref = value * period. In our module pm_config_t.xxx_ref_param actually equals to value_ref * period_ref. For example, if you want to know the value of power_ref_param, you have to choose a reference value of power and get the period of pulse sequence of this reference power. 
* Call iot_powermeter_delete to delete a power meter device and free it's memory.Don't call iot_powermeter_delete more than once for the same pm_handle and had better to set the pm_handle to NULL after delete it.