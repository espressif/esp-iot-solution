# 功率计量模块

* 功率计模块使用硬件脉冲计数器计算外部功率计的测量结果。

* 首先，调用 `iot_powermeter_create` 创建一个句柄来管理功率计设备。您可以决定使用哪个引脚和pcnt单元进行功率，电压和电流测量。

* 不同的功率计模式：
	1. 功率计有一个功率输出引脚，一个电压输出引脚和一个电流输出引脚，没有模式选择引脚。
在这种情况下，您应该为 `pm_config_t.xxx_io_num`，`pm_config_t.xxx_pcnt_unit` 和 `pm_config_t.xxx_ref_param` 设置不同的值。
`pm_config_t.sel_io_num` 必须设置为 0xff，`pm_config_t.pm_mode` 必须设置为 `PM_BOTH_VC`。
在这种情况下，无法调用 `iot_powermeter_change_mode`。
	2. （通常在此模式下）功率计具有功率输出引脚，电压/电流引脚和模式选择引脚，模式选择引脚连接到 gpio 以在程序运行时更改模式。
在这种情况下，电压和电流的 `pm_config_t.xxx_io_num` 和 `pm_config_t.xxx_pcnt_unit` 必须设置为相同的值，因为电压和电流是由相同的引脚输出的。但是，电压和电流的 `pm_config_t.xxx_ref_param` 必须设置为不同的值。例如，如果当模式选择引脚的电平设置为高电平时电压/电流引脚输出电压，则必须将 `pm_config_t.sel_level` 设置为 1 并且将 `pm_config_t.pm_mode` 设置为 `PM_SINGLE_VOLTAGE`。对于电流来说也是如此。在这种情况下，您可以调用 `iot_powermeter_change_mode` 来更改电压/电流引脚的输出值。
	3. 功率计具有功率输出引脚，电压/电流引脚和模式选择引脚，模式选择引脚连接到 VCC 或 GND。
在这种情况下，您无法更改电压/电流引脚输出的值，因为模式选择引脚已预先固定。例如，如果选择电压模式（电压/电流引脚输出电压），请设置 `pm_config_t.current_io_num` 和 `pm_config_t.sel_io_num` 为 0xff， 并将 `pm_config_t.pm_mode` 设置为 `PM_SINGLE_VOLTAGE`。在这种情况下，无法调用 `iot_powermeter_change_mode`。

* 功率引脚和电压/电流引脚的输出为脉冲序列。功率（或电压和电流）的实际值与脉冲序列的频率成正比。它们符合以下公式： `value_ref * period_ref = value * period`。在我们的模块中，`pm_config_t.xxx_ref_param` 实际上等于 `value_ref * period_ref`。例如，如果您想知道 `power_ref_param` 的值，则必须选择功率的参考值并获得该参考功率的脉冲序列周期。

* 调用 `iot_powermeter_delete` 删除功率计设备并释放它的内存。不要为同一个 `pm_handle` 多次调用 `iot_powermeter_delete`，最好在删除后将 `pm_handle` 设置为 NULL。

## 示例

BL0937 功率计参考电路：
<div align="center"><img src="../../../../documents/_static/power_meter/reference_circuit.png" width = "550" alt="reference_circuit" align=center /></div>  

采用 3.3V 供电。电流信号通过合金电阻采样后接入BL0937 的 IP 和 IN 管脚,电压信号则通过电阻分压网络后输入到 BL0937 的 VP 管脚。CF、CF1、SEL 直接接入到 MCU 的管脚,通过计算 CF、CF1 的脉冲周期来计算功率值、电流有效值和电压有效值的大小。

BL0937 对输入的电压和电流两个通道的输入电压求乘积,并通过信号处理,把获取的有功功率信息转换成频率;在这个过程中,同时通过运算计算出电压有效值和电流有效值并转换成频率。有功功率、电压和电流有效值分别以高电平有效的方式从 CF、CF1 输出相关的频率信号。

根据 BL0937 数据手册：

<div align="center"><img src="../../../../documents/_static/power_meter/formula.png" width = "400" alt="formula" align=center /></div>  

结合 `value_ref * period_ref = value * period` 和 `pm_config_t.xxx_ref_param = value_ref * period_ref`，可以计算出功率、电压、电流的 `pm_config_t.xxx_ref_param` 值分别为：
1. `pm_config_t.power_ref_param = 0.00000086`
2. `pm_config_t.voltage_ref_param = 0.00007911`
3. `pm_config_t.current_ref_param = 0.00001287`

结合该驱动中计算的脉冲周期单位为 us，故在上述的 `pm_config_t.xxx_ref_param` 上乘 10的6次方：
1. `pm_config_t.power_ref_param = 0.86`
2. `pm_config_t.voltage_ref_param = 79.11`
3. `pm_config_t.current_ref_param = 12.87`

使用上述的参数，通过 `iot_powermeter_read` 函数获取的电压、电流的单位分别为：V、A，且都为整数，为了提高数值精度，可以在上述的 `pm_config_t.xxx_ref_param` 上继续乘 10的6次方，将电压、电流的单位变换为 uV、uA，这样输出的数据精度会提高很多，根据实际情况还可以进一步调整。

此时，得到的电压信号是通过电阻分压网络后输入到 BL0937 的值，得到的电流信号是通过合金电阻采样后接入 BL0937 的值。

要得到未通过电阻分压的电压信号，需要根据外部电流的分压电阻确定，如参考电路中，使用的分压电阻 200k * 6 + 510，若 BL0937 VP 端的电压为 x，那么未通过电阻分压的电压 y 为：(200000 * 6 + 510) / 510 * x。

在实际应用中由于外围电路的合金采样电阻、电压采样网络电阻误差、计量芯片基准偏差等会带来一定的偏差,使得计量芯片输出的脉冲频率与理论计算频率有偏差,所以我们需要进行计量校准。
推荐使用单点校准方式, BL0937 在校准时可以在智能插座施加额定电压 U0,电流 I0,有功功率 P0 时 MCU 测得的对应脉冲频率 U_Freq0、 I_Freq0、 P_Freq0,换算出对应的转换系数:

<div align="center"><img src="../../../../documents/_static/power_meter/transfer.png" width = "500" alt="transfer" align=center /></div>  

并在系统中保存这些系数,校准后在实际测量点计量芯片输出的频率值系统软件应该与对应系数相乘以获得正确的测量值。
