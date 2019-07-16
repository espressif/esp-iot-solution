[[EN]](./readme_en.md)
# ULP 协处理器在低功耗模式下读片内霍尔传感器 HALL SENSOR
本文提供了 ULP 协处理器如何在低功耗模式下读片内霍尔传感器的例子  

## 1. 霍尔传感器
根据霍尔效应，当电流垂直于磁场通过 N 型半导体时，会在垂直于电流和磁场的方向产生附加电场，从而在半导体两端形成电势差，具体高低与电磁场的强度和电流大小有关。当恒定电流穿过磁场或电流存在于恒定磁场时，霍尔效应传感器可用于测量磁场强度。霍尔传感器的应用场合非常广泛，包括接近探测、定位、测速与电流检测等。

## 2. 霍尔传感器读取示例
本例子 ULP 协处理器每隔 3 S 唤醒一次，唤醒后在低功耗模式下读取霍尔传感器值, 通过 hall phase shift 两次，读取 vp 和 vn 值 各两次一共四个值，减去共模的部分可以得出 offset 值，这个值可以用来表征环境对霍尔传感器的影响。如图，第一次打印的数值是周围未有强磁场的情况下测得的霍尔传感器数值；第二次打印的数值是使用了一枚钕铁硼磁铁的 N 极接近 ESP32 时获取的数值；第三次打印的数值是钕铁硼磁铁的 S 极接近 ESP32 时获取的数值，可以看出霍尔传感器的数值发生了较大的变化。

![](../../../documents/_static/ulp_hall_sensor/hall_sensor.png)

## 3. 系统连接
 HALL SENSOR 和 SAR ADC 连接情况见下图，HALL SENSOR 的 SENSOR_VP 和 SENSOR_VN 管脚分别连接到 SAR ADC1 的 SAR_MUX = 1 和 SAR_MUX = 4 上。

![](../../../documents/_static/ulp_hall_sensor/sar_adc.png)

下表是 SAR ADC1 的输入信号及 SAR_MUX 通道

|信号名/GPIO|SAR_ADC1，SAR_MUX|
|---|:---:|
|SENSOR_VP (GPIO36)|1|
|SENSOR_CAPP (GPIO37)|2|
|SENSOR_CAPN (GPIO38)|3|
|SENSOR_VN (GPIO39)|4|
|32K_XP (GPIO33)|5|
|32K_XN (GPIO32)|6|
|VDET_1 (GPIO34)|7|
|VDET_2 (GPIO35)|8	|


## 4. 编译配置及烧录程序
ESP32 的 C 语言编译环境安装和配置参照 [链接地址](https://docs.espressif.com/projects/esp-idf/en/stable/get-started/index.html#setup-toolchain)，另外 ULP 协处理器目前只支持汇编编程，所以还需要安装汇编工具链，下面介绍汇编工具链的安装和配置。
##### 4.1 汇编环境的配置
ULP 协处理器配置汇编编译工具链，只需两步即可安装配置完毕，下面给出 ubuntu 操作系统下配置的步骤，或者点击 [链接地址](https://docs.espressif.com/projects/esp-idf/en/stable/api-guides/ulp.html) 获得更多 ULP 编程信息
>* 第一步, 下载工具链 `binutils-esp32ulp toolchain`  [链接地址]( https://github.com/espressif/binutils-esp32ulp/wiki#downloads), 解压到需要安装的目录
>* 第二步，添加工具链的 `bin` 目录到系统环境变量 `PATH` 中。例如我的解压目录是 `/opt/esp32ulp-elf-binutils` 那么添加 `export PATH=/opt/esp32ulp-elf-binutils/bin:$PATH` 这一行到 /home 目录的隐藏文件 `.bashrc` 文件最后一行，保存关闭文件并使用命令 `source .bashrc` 使上述环境变量生效

##### 4.2 配置编译烧录
至此，汇编编译环境就安装好了，在 [esp-iot-solution](https://github.com/espressif/esp-iot-solution) /examples/ulp_hall_sensor/ 目录下依次运行以下命令，进行 default config 配置并编译、烧录程序。

Make:
>* make defconfig
>* make all -j8 && make flash monitor

CMake
>* idf.py defconfig
>* idf.py flash monitor

## 5. 软件分析
ULP 协处理器没有内置读霍尔传感器相关的汇编指令，所以我们需要设置相关寄存器来读取片内霍尔传感器。

在 ` void init_ulp_program() ` 函数中设置 ADC1 通道 1/2 输入电压衰减，用户可以自己定义这个衰减值，较大的衰减将得到较小的 ADC 值。
```C
    /* The ADC1 channel 0 input voltage will be reduced to about 1/2 */
    adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_6);
    /* The ADC1 channel 3 input voltage will be reduced to about 1/2 */
    adc1_config_channel_atten(ADC1_CHANNEL_3, ADC_ATTEN_DB_6);
    /* ADC capture 12Bit width */
    adc1_config_width(ADC_WIDTH_BIT_12);
    /* enable adc1 */
    adc1_ulp_enable();                 
```

在超低功耗模式下，需要预先设置相关的寄存器之后才可以通过 SAR ADC1 来读取 HALL SENSOR 值。
```C
	/* SENS_XPD_HALL_FORCE = 1, hall sensor force enable, XPD HALL is controlled by SW */
	WRITE_RTC_REG(SENS_SAR_TOUCH_CTRL1_REG, SENS_XPD_HALL_FORCE_S, 1, 1)

	/* RTC_IO_XPD_HALL = 1, xpd hall, Power on hall sensor and connect to VP and VN */
	WRITE_RTC_REG(RTC_IO_HALL_SENS_REG, RTC_IO_XPD_HALL_S, 1, 1)

	/* SENS_HALL_PHASE_FORCE = 1, phase force, HALL PHASE is controlled by SW */
	WRITE_RTC_REG(SENS_SAR_TOUCH_CTRL1_REG, SENS_HALL_PHASE_FORCE_S, 1, 1)

	/* RTC_IO_HALL_PHASE = 0, phase of hall sensor */
	WRITE_RTC_REG(RTC_IO_HALL_SENS_REG, RTC_IO_HALL_PHASE_S, 1, 0)

	/* SENS_FORCE_XPD_SAR, Force power up */
	WRITE_RTC_REG(SENS_SAR_MEAS_WAIT2_REG, SENS_FORCE_XPD_SAR_S, 2, SENS_FORCE_XPD_SAR_PU)
```

之后，使用 `ADC ` 指令多次读取片内霍尔传感器 phase_vp 和 phase_vn 的值，累加并计算平均值后，将霍尔传感器值保存到 `Sens_Vp0` ，`Sens_Vn0` 这两变量中。
```asm
	/* do measurements using ADC */
	/* r2, r3 will be used as accumulator */
	move r2, 0
	move r3, 0	
	/* initialize the loop counter */
	stage_rst
measure0:
	/* measure Sar_Mux = 1 to get vp0   */
	adc r0, 0, 1
	add r2, r2, r0

	/* measure Sar_Mux = 4 to get vn0 */
	adc r1, 0, 4
	add r3, r3, r1

	/* increment loop counter and check exit condition */
	stage_inc 1
	jumps measure0, adc_oversampling_factor, lt

	/* divide accumulator by adc_oversampling_factor.
	   Since it is chosen as a power of two, use right shift */
	rsh r2, r2, adc_oversampling_factor_log

	/* averaged value is now in r2; store it into Sens_Vp0 */
	move r0, Sens_Vp0
	st r2, r0, 0

	/* r3 divide 4 which means rsh 2 bits */
	rsh r3, r3, adc_oversampling_factor_log
	/* averaged value is now in r3; store it into Sens_Vn0 */
	move r1, Sens_Vn0
	st r3, r1, 0
```

接下来，需要 shift 霍尔传感器的 phase，设置寄存器 `RTC_IO_HALL_SENS_REG` 的 `RTC_IO_HALL_PHASE` 位置 1 ， 并再次读取片内霍尔传感器 phase_vp 和 phase_vn 的值，同上，累加并计算平均值后，保存到 `Sens_Vp1` ，`Sens_Vn1` 中。
```C
	/* RTC_IO_HALL_PHASE = 1, phase of hall sensor */
	WRITE_RTC_REG(RTC_IO_HALL_SENS_REG, RTC_IO_HALL_PHASE_S, 1, 1)
```

最后，在唤醒主 CPU 后，通过以上四个数值计算出 offset 的值并打印出来。
```C
static void print_hall_sensor()
{
    printf("ulp_hall_sensor:Sens_Vp0:%d,Sens_Vn0:%d,Sens_Vp1:%d,Sens_Vn1:%d\r\n",
        (uint16_t)ulp_Sens_Vp0,(uint16_t)ulp_Sens_Vn0,(uint16_t)ulp_Sens_Vp1,(uint16_t)ulp_Sens_Vn1);  
    printf("offset:%d\r\n",  ((uint16_t)ulp_Sens_Vp0 - (uint16_t)ulp_Sens_Vp1) - ((uint16_t)ulp_Sens_Vn0 - (uint16_t)ulp_Sens_Vn1));
}
```


## 6. 总结
ESP32 中的霍尔传感器经过专门设计，可向低噪放大器和 SAR ADC 提供电压信号，实现磁场传感功能。在超低功耗模式下，该传感器可由 ULP 协处理器控制。ESP32 内置了霍尔传感器在位置传感、接近检测、测速以及电流检测等应用场景下成为一种极具吸引力的解决方案。

## 7. 参考文档
1. esp32_technical_reference_manual_cn.pdf 文档的 27.5 霍尔传感器
2. esp32_technical_reference_manual_cn.pdf 文档的 27.3 逐次逼近数字模拟转换器

 