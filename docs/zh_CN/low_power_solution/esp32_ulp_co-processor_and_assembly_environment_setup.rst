:orphan:

ESP32 的 ULP 协处理器简介和汇编编译环境设置
===========================================

:link_to_translation:`en:[English]`

本文介绍 ESP32 强大的超低功耗协处理器 (ULP
co-processor)，给出汇编环境的搭建步骤。

1. ULP 协处理器简介
-------------------

ULP 协处理器是一个功耗极低的协处理器设备，无论主 CPU
是处于正常运行模式还是 Deep-sleep 模式，ULP
协处理器都可以独立运行。超低功耗协处理器的补充使得 ESP32
能够胜任一些对低功耗要求较高的应用场合。

ULP 协处理器的主要特性有：

-  采用 8 MHz 频率和 8 KB 内存
-  内建 ADC 和 I2C 接口
-  支持正常模式和 Deep-sleep 模式
-  可唤醒主 CPU 或向主 CPU 发送中断
-  能够访问主 CPU 的外围设备、内部传感器及 RTC 寄存器

更多特性及介绍详见 `ESP32
技术参考手册 <http://www.espressif.com/sites/default/files/documentation/esp32_technical_reference_manual_cn.pdf>`__\ 。

2. ULP 协处理器电流消耗
-----------------------

鉴于以上的特性，ULP 协处理器能够在消耗较低电流的情况下，完成 ADC
采样，进行 I2C Sensor 的读写，驱动 RTC GPIO
口动作，可以在某些超低功耗场景中完全替代主 CPU。

下表给出一般测得的电流数值：

+---------------------+----------+
| 场景                | 电流值   |
+=====================+==========+
| Deep-sleep          | 6 μA     |
+---------------------+----------+
| Delay Nop           | 1.4 mA   |
+---------------------+----------+
| GPIO Toggle         | 1.4 mA   |
+---------------------+----------+
| SAR\_ADC Sampling   | 2.3 mA   |
+---------------------+----------+

**注意：** 上表列出的电流消耗是在 ULP 协处理器引用 VDD3P3\_RTC
工作电压，运行在 8 MHz，并且没有进入睡眠情况下测得的。当 ULP
协处理器设置为周期性的进入深度睡眠，在短时间内唤醒工作时，这些数字将会大大减少。因此，ULP
协处理器的理想工作状态是周期性唤醒，短暂工作之后，继续进入休眠状态，以短时间高效的工作和长时间的休眠换取低功耗平衡。

3. 配置 ULP 协处理器编译环境
----------------------------

ULP
协处理器目前只支持汇编开发，我们提供了三种不同平台（Linux，Windows，MacOS）的
ULP 协处理器编译环境的设置方法。

需说明：在安装汇编工具链之前，我们默认你已经安装和配置好 ESP32 ESP-IDF C
语言编译工具链，\ `安装参考链接 <https://docs.espressif.com/projects/esp-idf/en/stable/get-started/index.html#setup-toolchain>`__\ 。

3.1 Linux
^^^^^^^^^

Linux
下配置编译环境比较简单，只需要下载汇编编译工具链，解压到安装目录中，再添加环境变量即可完成设置。
1. 下载工具链 `binutils-esp32ulp toolchain
Linux(x64) <https://github.com/espressif/binutils-esp32ulp/wiki#downloads>`__
2. 解压到要安装的目录,添加工具链的 ``bin`` 目录到环境变量 ``PATH``
中。例如，解压目录是 ``/opt/esp32ulp-elf-binutils`` 那么添加
``export PATH=/opt/esp32ulp-elf-binutils/bin:$PATH`` 这一行到
``.bashrc`` 文件中，运行 ``source .bashrc``
使上述环境变量生效，这样，ULP 的汇编环境就设置完毕了。

3.2 Windows
^^^^^^^^^^^

首先安装 Windows / MSYS2 下的 C
编译工具链可参考\ `说明链接 <https://docs.espressif.com/projects/esp-idf/en/stable/get-started/windows-setup.html>`__
，之后再安装 ULP
协处理器\ `编译工具链链接 <https://github.com/espressif/binutils-esp32ulp/wiki#downloads>`__
1. 解压 ULP 工具链 ``esp32ulp-elf-binutils-win32-...`` 到 MSYS2 的
``opt`` 目录中，一般建议使用 ``C:\msys32\opt`` 目录，因为
``ESP32 toolchain xtensa-esp32-elf`` 也在这个目录下。

2. 打开位于目录 ``C:\msys32\etc\profile.d`` 下的 ``esp32_toolchain.sh``
   文件，把 ULP 工具链的 ``bin/``\ 目录添加到 path 中，设置例如：

   ::

       # This file was created by ESP-IDF windows_install_prerequisites.sh
       # and will be overwritten if that script is run again.
       export PATH="$PATH:/opt/xtensa-esp32-elf/bin:/opt/esp32ulp-elf-binutils/bin"

3. 保存文件并重新打开 MSYS2 控制台。
4. 设置到此完成，你可以编译 ULP 协处理器的例程了，可尝试编译 :example:`system/ulp_adc`
   例子。

3.3 MacOS
^^^^^^^^^

MacOS 平台编译设置几乎是和 Linux 一样的，但需下载 MacOS
平台对应的编译工具链. 1. `下载 MacOS
平台的工具链 <https://github.com/espressif/binutils-esp32ulp/wiki>`__ 2.
解压工具链到安装目录，然后添加工具链的 ``bin/`` 目录地址到 PATH
环境变量中，生效即可。

4. 编译 ULP 代码
----------------

如果把 ULP 汇编代码编译为应用组件的一部分，必须采取以下步骤：

1. ULP 的代码是汇编写的，并且文件后缀格式是.S,
   这些汇编文件必须放在一个单独的目录中，例如 ulp/
2. 修改 makefile 添加如下内容，一些详细的定义在 `Compiling ULP
   code <https://docs.espressif.com/projects/esp-idf/en/stable/api-guides/ulp.html#compiling-ulp-code>`__
   可以找到解释。

   ::

       ULP_APP_NAME ?= ulp_$(COMPONENT_NAME)
       ULP_S_SOURCES = $(COMPONENT_PATH)/ulp/ulp_source_file.S
       ULP_EXP_DEP_OBJECTS := main.o
       include $(IDF_PATH)/components/ulp/component_ulp_common.mk

3. 至此，可以编译包含 ULP 协处理器汇编代码的应用程序了。

5. ULP 协处理器的汇编指令
-------------------------

汇编指令目前有 24 条，在 `ULP coprocessor instruction
set <https://docs.espressif.com/projects/esp-idf/en/stable/api-guides/ulp_instruction_set.html#add-add-to-register>`__
获得更详细的说明。

算数与逻辑类运算的指令
^^^^^^^^^^^^^^^^^^^^^^^

-  算数指令：加 ADD，减 SUB
-  逻辑指令：与 AND，或 OR
-  移位指令：左移 LSH，右移 RSH
-  寄存器指令：移动 MOVE
-  计数器寄存器操作：重置 STAGE\_RST，加 STAGE\_INC，减 STAGE\_DEC

加载与数据储存类指令
''''''''''''''''''''

-  从内存加载数据指令：LD
-  储存数据至内存指令：ST
-  从外围寄存器读取数据指令：REG\_RD
-  写数据到外围寄存器指令：REG\_WR

跳转类指令
''''''''''

-  跳转至绝对地址：JUMP
-  跳转至相对地址（基于 R0 寄存器判断）：JUMPR
-  跳转至相对地址（基于阶段计数器寄存器判断）：JUMPS

测量类指令
''''''''''

-  模拟量测量：ADC
-  内部温度传感器测量：TSENS

I2C 通讯指令
''''''''''''

-  I2C 读指令：I2C\_RD
-  I2C 写指令：I2C\_WR

管理程序执行指令
''''''''''''''''

-  等待延时指令：WAIT
-  结束停止指令：HALT

控制协处理器睡眠周期
''''''''''''''''''''

-  休眠指令：SLEEP

唤醒 CPU 及与 SOC 通信
''''''''''''''''''''''

-  唤醒 CPU 指令：WAKE

6. ULP 协处理器的例程
---------------------

在
`esp-iot-solution <https:404/tree/master/examples/ulp_examples>`__
目录下目前有一些 ULP 协处理器使用的例子供参考

+-------+-------------------------+------------------------------------------------------------+
| Num   | Examples                | Note                                                       |
+=======+=========================+============================================================+
| 1     | ulp\_hall\_sensor       | 在超低功耗模式下读取片内霍尔传感器例程                     |
+-------+-------------------------+------------------------------------------------------------+
| 2     | ulp\_rtc\_gpio          | 在 ULP mode 下操作 RTC GPIO 管脚翻转例子                   |
+-------+-------------------------+------------------------------------------------------------+
| 3     | ulp\_tsens              | 在超低功耗模式下读片内温度传感器                           |
+-------+-------------------------+------------------------------------------------------------+
| 4     | ulp\_watering\_device   | 在超低功耗模式下使用 SAR\_ADC 监测土壤湿度浇花的综合例程   |
+-------+-------------------------+------------------------------------------------------------+

7. 引用文档
-----------

-  `ESP-IOT-SOLUTION
   的低功耗方案文档 <https:404/tree/master/documents/low_power_solution>`__
-  `ULP coprocessor
   programming <https://docs.espressif.com/projects/esp-idf/en/stable/api-guides/ulp.html>`__

