# ESP32 的 ULP 协处理器简介和汇编编译环境设置
本例的目的是介绍 ESP32 强大的超低功耗协处理器 (ULP Co-processor)，给出汇编环境的搭建步骤

## 1. 什么是 ULP 协处理器
ULP Co-processor 是一个功耗极低的协处理器设备，无论主CPU是处于正常运行模式或是 DeepSleep 模式,ULP 协处理器都可以独立运行。ULP 协处理器的补充使得 ESP32 能够胜任一些对低功耗要求较高的应用场合

下表列出了 ULP 协处理器的主要特性，更多特性及介绍详见乐鑫的[ESP32技术参考手册](http://www.espressif.com/sites/default/files/documentation/esp32_technical_reference_manual_cn.pdf)

|序号|特性说明|
|---|---|
|1|采用 8 MHz 频率和 8 KB 内存|
|2|内建 ADC 和 I2C 接口|
|3|支持正常模式和 Deep-sleep 模式|
|4|可唤醒主 CPU 或向主 CPU 发送中断|
|5|能够访问主 CPU 的外围设备、内部传感器及 RTC 寄存器|

## 2. ULP协处理器能够做什么
鉴于以上的特性，ULP协处理器能够在消耗较低电流的情况下，完成 ADC 采样，进行 I2C SENSOR 的读写，驱动RTC GPIO口动作，数据保存 FLASH 等功能，可以在某些场景中完全替代主 CPU

下表给出一般测得的电流数值

|场景|电流值|
|---|---|
|Deep Sleep|6uA|
|Delay Nop|1.4mA|
|GPIO Toggle|1.4mA|
|SAR_ADC Sampling|2.3mA|
* 注：考虑到 ULP 协处理器工作电压是引用 VDD3P3_RTC,而且工作频率是 8MHz, 以上电流也符合预期。因此，ULP 协处理器的理想工作状态是周期性唤醒，短暂工作之后，继续进入休眠状态，以短时间高效的工作和长时间的休眠换取低功耗平衡。

## 3. 如何使用 ULP 协处理器 
ULP 协处理器目前是只支持汇编开发的，需要配置新的编译工具链，好消息是只需两步即可配置汇编环境,在 [Read the Docs](http://esp-idf.readthedocs.io/en/latest/api-guides/ulp.html) 可获得更多的信息
>* 第一步, 工具链 [binutils-esp32ulp toolchain]( https://github.com/espressif/binutils-esp32ulp/wiki#downloads)下载, 解压到要安装的目录
>* 第二步，添加工具链的 `bin` 目录到环境变量`PATH`中。例如我的解压目录是`/opt/esp32ulp-elf-binutils`那么添加`export PATH=/opt/esp32ulp-elf-binutils/bin:$PATH`这一行到`.bashrc`文件中，`source .bashrc` 使上述环境变量生效，或者登出再登录可以

到此，ULP 的汇编环境就设置完毕了

## 4. 如何使用汇编指令
汇编指令目前有24条，可以在这个 [链接地址](http://esp-idf.readthedocs.io/en/latest/api-guides/ulp_instruction_set.html#add-add-to-register) 获得更详细的说明

##### 算数于逻辑类运算的指令：
>* 算数指令， 加 ADD, 减 SUB
>* 逻辑指令， 与 AND, 或 OR
>* 移位指令， 左移 LSH, 右移 RSH
>* 寄存器指令， 移动 MOVE
>* 计数器寄存器操作， 重置STAGE_RST, 加STAGE_INC, 减STAGE_DEC

##### 加载与数据储存类指令
>* 从内存加载数据指令 LD
>* 储存数据至内存指令 ST
>* 从外围寄存器读取数据指令 REG_RD
>* 写数据到外围寄存器指令 REG_WR

##### 跳转类指令
>* 跳转至绝对地址 JUMP
>* 跳转至相对地址（基于 R0 寄存器判断）JUMPR 
>* 跳转至相对地址（基于阶段计数器寄存器判断）JUMPS

##### 测量类指令
>* 模拟量测量 ADC
>* 内部温度传感器测量 TSENS

##### I2C 通讯指令
>*  I2C 读指令 I2C_RD
>*  I2C 写指令 I2C_WR

##### 管理程序执行指令
>* 等待延时指令 WAIT
>* 结束停止指令 HALT

##### 控制协处理器睡眠周期
>* 休眠指令 SLEEP

##### 唤醒 CPU 及与 SOC 通信
>* 唤醒 CPU 指令 WAKE




