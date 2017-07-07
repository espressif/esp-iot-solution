# 方案概述

* TOUCH 功能是 ESP32 芯片的一项重要功能。ESP32 芯片上 TOUCH pad 设计的原理是因手指触摸产生额外电容改变充放电时间， 芯片内部通过对充放电次数的变化进行检测处理后实时反映手指触摸的有效性。
* 在 ESP-IDF 中提供了 TOUCH pad 采样值读数，此读数会随着 TOUCH pad 上的等效电容的增大而减小。为了能够实时地捕捉 TOUCH pad 上的触摸事件而不占用 CPU 资源, ESP-IDF中提供了中断机制。用户需要设置 TOUCH pad 采样的阈值，当当前采样值小于此阈值时，TOUCH 中断会触发。
* 由于 TOUCH pad 自身的电容会随温度、电压、湿度等外界因素的变化而变化，所以在 TOUCH pad 无触摸的情况下其采样读数也会产生波动。由于 TOUCH pad 自身引起的读数变化会导致灵敏度下降、误捕捉触摸事件等情况。

# 数学模型

* 假设有 N 个 TOUCH pad ，对于第 i 个 pad t时刻其采样读数的数学期望为 Eval<sub>it</sub> , N 个 pad 在 t 时刻的读数期望的平均值为 AVG<sub>t</sub>  = (Eval<sub>0</sub> + Eval<sub>1</sub> + ... + Eval<sub>N-1</sub>)/N 。
* 我们为 TOUCH pad 建立的模型满足 Eval<sub>it</sub> = θ<sub>t</sub> * μ<sub>i</sub> * P 。
	* θ<sub>t</sub> 是与 t 时刻的外界因素（如温度、电压、湿度等）有关的参数，不同的 pad θ<sub>t</sub>是相同的
	* μ<sub>i</sub> 是对于不同的 pad 取不同值的参数，它与 pad 的形状、pad 在 pcb 上的位置、pcb 的走线等相关的
	* P 描述了不同 pad 之间存在的固有共性，例如材料等
* 在同一时刻 t 对于第 i 个 pad ，其数学期望与 N 个 pad 的数学期望平均值的比值满足 β<sub>i</sub> = Eval<sub>it</sub>/AVG<sub>t</sub> = N * μ<sub>i</sub> / (μ<sub>0</sub> + μ<sub>1</sub> + ... + μ<sub>N-1</sub>) 是一个不随时间变化的定值。我们对 pad 进行了温度与电压实验，通过观察电压与温度变化时用实验数据计算的 Eval<sub>it</sub>/AVG<sub>t</sub> 是否为定值验证此模型的准确性。
	
# 实验验证

* 为了验证第二部分的模型，我们对 TOUCH pad 进行了温度测试与电压测试。
* 不同温度与电压下各 pad 的读数如下二图所示，从图中可以看出温度与电压的变化都会对 TOUCH pad 的读数有影响，使用过程中动态地调整 pad 的无触摸读数期望值与中断触发阈值是有必要的。

    | <img src="https://gitlab.espressif.cn:6688/rd/esp-iot-solution/raw/doc/touchpad/documents/touchpad/pad%E6%B8%A9%E5%BA%A6_%E8%AF%BB%E6%95%B0%E6%9B%B2%E7%BA%BF.png" width = "500" alt="touchpad_temp0" align=center />|<img src="https://gitlab.espressif.cn:6688/rd/esp-iot-solution/raw/doc/touchpad/documents/touchpad/pad%E7%94%B5%E5%8E%8B_%E8%AF%BB%E6%95%B0%E6%9B%B2%E7%BA%BF.png" width = "500" alt="touchpad_volt0" align=center /> |
    |--|--|
    |  |  |
    

    

* 温度测试的 pad 读数数据如下表所示：

    <img src="https://gitlab.espressif.cn:6688/rd/esp-iot-solution/raw/doc/touchpad/documents/touchpad/pad%E6%B8%A9%E5%BA%A6_%E8%AF%BB%E6%95%B0.png" width = "500" alt="touchpad_temp1" align=center />

*  不同温度下第 i 个 pad 的数学期望与 N 个 pad 的数学期望平均值的比值 Eval<sub>it</sub>/AVG<sub>t</sub> 如下表所示：

    <img src="https://gitlab.espressif.cn:6688/rd/esp-iot-solution/raw/doc/touchpad/documents/touchpad/pad%E6%B8%A9%E5%BA%A6_%E6%AF%94%E5%80%BC.png" width = "500" alt="touchpad_temp2" align=center />

* 电压测试的 pad 读数数据如下表所示：

    <img src="https://gitlab.espressif.cn:6688/rd/esp-iot-solution/raw/doc/touchpad/documents/touchpad/pad%E7%94%B5%E5%8E%8B_%E8%AF%BB%E6%95%B0.png" width = "500" alt="touchpad_volt1" align=center />

*  不同温度下第 i 个 pad 的数学期望与 N 个 pad 的数学期望平均值的比值 Eval<sub>it</sub>/AVG<sub>t</sub> 如下表所示：

    <img src="https://gitlab.espressif.cn:6688/rd/esp-iot-solution/raw/doc/touchpad/documents/touchpad/pad%E7%94%B5%E5%8E%8B_%E6%AF%94%E5%80%BC.png" width = "500" alt="touchpad_volt2" align="center" />

* 从表2与表4可以看出无论温度与电压如何变化，每一个 pad 的 Eval<sub>it</sub>/AVG<sub>t</sub> 都是一个较为稳定的数值，其波动范围仅为 1%左右。通过这两个实验可以证明我们的模型是准确的。

# 自校准方案

* 根据第二部分的模型与第三部分的实验验证，我们将利用 N 个 pad 的无触摸状态全局平均读数估计值 AVG'<sub>t</sub> 与每个 pad 固定不变的 β<sub>i</sub> 更新 pad i 的无触摸状态读数期望值：Eval<sub>it</sub> = AVG'<sub>t</sub> * β<sub>i</sub> 。其中 AVG'<sub>t</sub> = (c<sub>0t</sub> + c<sub>1t</sub> + ... + c<sub>(N-1)t</sub> ) / N 。如果当前判断 pad i 为无触摸状态则 c<sub>it</sub> 取当前的读数值，否则 c<sub>it</sub> = c<sub>i(t-1)</sub> 。
* 具体步骤如下：
	* 1、系统初始化时读取每个 pad m个数据，计算所有 pad 的 β<sub>i</sub> = AVG'<sub>0</sub> / Eval'<sub>i0</sub>，这是一个固定的值不会随时间变化。Eval'<sub>i</sub> = (val<sub>0</sub> + val<sub>1</sub> + ... + val<sub>(m-1)</sub>) / m, 其中 val<sub>j</sub> 表示第 j 个读数。
	* 2、更新所有 pad 的无触摸状态读数期望值 Eval<sub>it</sub> = AVG'<sub>t</sub> * β<sub>i</sub>
	* 3、更新所有 pad 的中断触发阈值 thres<sub>it</sub> = λ<sub>i</sub> * Eval<sub>it</sub> ，λ<sub>i</sub> 是一个决定触发灵敏度的系数。
	* 4、 启动自校准定时，定时触发时从步骤2开始执行

# 加隔板灵敏度测试

* 在 TOUCH 传感器上增加隔板能起到保护作用，但是会使得传感器灵敏度下降，我们对此进行了测试。
* TOUCH pad 无触摸、隔一层pcb触摸和隔一层pcb+1mm塑料隔板触摸时的读数变化如下表与下图所示：

    <img src="https://gitlab.espressif.cn:6688/rd/esp-iot-solution/raw/doc/touchpad/documents/touchpad/touchpad_%E9%9A%94%E6%9D%BF%E6%B5%8B%E8%AF%95%E5%9B%BE%E8%A1%A8.png" width = "500" alt="touchpad_geban2" align=center />

    <img src="https://gitlab.espressif.cn:6688/rd/esp-iot-solution/raw/doc/touchpad/documents/touchpad/touchpad_%E9%9A%94%E6%9D%BF%E6%B5%8B%E8%AF%95.png" width = "300" alt="touchpad_geban1" align=center />

* 在 TOUCH 传感器与手指间隔 pcb 或者塑料隔板会严重影响 TOUCH pad 灵敏度，可以看到触摸引起的读数相对变化量仅为 3% 左右

# 单个 pad 驱动方案

* 在 esp-iot-solution 中，用户可以直接创建一个 TOUCH pad 实例，用户需要设定 pad i 中断触发阈值百分比λ<sub>i</sub>，真实的阈值为 thres<sub>it</sub> = λ<sub>i</sub> * Eval<sub>it</sub>，其中 Eval<sub>it</sub> 会定时地自校准。同时用户需要设置滤波时间 filter<sub>i</sub> 。
* 驱动步骤：
	* 1、pad 中断触发，开启软件定时器，定时时间为filter<sub>i</sub>毫秒
	* 2、定时中断触发，读取 pad 采样值 val<sub>i</sub> 。若 val<sub>i</sub> < thres<sub>it</sub> ，执行 pad push 回调，重启软件定时
	* 3、定时中断触发，读取 pad 采样值 val<sub>i</sub> 。若 val<sub>i</sub> > thres<sub>it</sub> ，执行 pad tap 与 pad release 回调。否则重启定时器重复步骤3。
* 用户可以设置连续触发模式，这时在步骤三种若 val<sub>i</sub> < thres<sub>it</sub> ，就会执行pad serial trigger 回调。
* esp-iot-solution 中的使用步骤：

    ```c
/*
 创建 touchpad 对象
thres_percent 的设置取决于使用场景下 touchpad 的灵敏度，大致取值可以参考如下：
	1、如果使用是 pad 直接与手指接触，则将 thres_percent 设置为 70~80 即可得到很好的灵敏度，同时又能保证很高的稳定性
	2、如果 pad 放置于 pcb 的最底层，手指与 pad 之间会间隔 1mm 左右厚度的 pcb，则将 thres_percent 设置为97左右
	3、若果还要在 pcb上加一层 1mm 左右的塑料保护层，则将 thres_percent 设置为99，这时稳定性会比较低，容易发生误触发 
filter_value 用于判断释放的轮询周期，一般设置为150即可
touchpad_handle_t 用于后续对此 touchpad 的控制
*/
touchpad_handle_t tp = touchpad_create(touch_pad_num, thres_percent, filter_value);
touchpad_add_cb(tp, cb_type, cb, arg);		// 添加 push、release 或者 tap 事件的回调函数
touchpad_add_custom_cb(tp, press_sec, cb, arg);		// 添加用户定制事件的回调函数，用户设置持续按住 touchpad 多少秒触发该回调
/*
设置连续触发模式：
	trigger_thres_sec 决定按住 pad 多少秒后开始连续触发
	interval_ms 决定连续触发是两次触发间的时间间隔
*/
touchpad_set_serial_trigger(tp, trigger_thres_sec, interval_ms, cb, arg);
```

# TOUCH pad 滑块驱动方案

*  touchpad 滑块采用如下图所示的结构布置多个 pad ，手指触碰滑块时，用户可以读取触碰点在滑块上的相对位置。

    <img src="https://gitlab.espressif.cn:6688/rd/esp-iot-solution/raw/doc/touchpad/documents/touchpad/slide_touchpad.png" width = "500" alt="touchpad_volt2" align=center />

* touchpad 滑块触碰点位置的计算采用质心计算的灵感。首先，按顺序为每一个 pad i 赋予位置权重 w<sub>i</sub> = i * ξ ，式中 ξ 决定了定位的精度，ξ 越大位置分割越细。例如图中将从左到右的5个 pad 的权值依次赋值为 0，10，20，30，40。然后读取 t 时刻每个 pad 上的读数变化量 Δval<sub>it</sub> = Eval<sub>it</sub> - real<sub>it</sub> ， real<sub>it</sub> 是事时读数。最后，计算相对位置：posi<sub>t</sub> = (Δval<sub>0t</sub> * w<sub>0</sub> + Δval<sub>1t</sub> * w<sub>1</sub> + ... + Δval<sub>(N-1)t</sub> * w<sub>N-1</sub>) / (Δval<sub>0t</sub> + Δval<sub>1t</sub> + ... + Δval<sub>(N-1)t</sub>) 。此相对位置将是一个在 0 和 (N-1)*ξ 的值。
* 驱动步骤：
	* 1、初始化时按位置顺序创建 pad 实例，为每一个 pad 实例添加 push 与release 回调函数。
	* 2、在 push 和 release 回调函数中，读取此滑块中每一个 touchpad 的 值，按上面的公式计算相对位置。
* esp-iot-solution 中的使用步骤：

    ```c++
/*
创建 touchpad 滑块对象
num 决定使用多少个 pad 组成一个滑块
tps 是 TOUCH_PAD_NUM 的数组，每一个 TOUCH_PAD_NUM 在数组中的位置要跟 pcb 上的实际位置严格对应
*/
touchpad_slide_handle_t tp_slide = touchpad_slide_create(num, tps);
uint8_t pos = touchpad_slide_position(tp_slide);		// 用于读取手指触碰位置在滑块上的相对位置，手指没触碰时返回 255
```