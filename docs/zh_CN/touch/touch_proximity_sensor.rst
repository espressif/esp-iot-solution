触摸接近感应传感器
=======================

:link_to_translation:`en:[English]`

``touch_proximity_sensor`` 组件基于 ESP32-S3 内置的触摸传感器进行开发，使用该组件可以轻松实现触摸接近感应功能。

.. note::
   - ESP32/ESP32-S2/ESP32-S3 触摸相关组件仅用于测试或演示目的。由于触摸功能的抗干扰能力差，可能无法通过 EMS 测试，因此不建议用于量产产品。
   - 该组件目前仅适用于 ESP32-S3，并且需要 IDF 版本大于等于 v5.0。

实现原理
------------

触摸接近感应传感器是以 ESP32-S3 的触摸传感器接近感应功能为基础实现的，其硬件原理结构图如下：

.. figure:: ../../_static//touch/touch_proximity_sensor/prox_sensor_principle.png
    :align: center
    :width: 75%
    :alt: 触摸接近感应传感器硬件原理结构图

当目标物体靠近传感器时，其等效电容会发生变化。目标物体可以是人的手指、手或任何导电物体。
当触摸传感器被配置为接近感应模式时，传感器输出的值是累加值，当目标物体靠近传感器面板时，传感器输出的累计值会变大。
基于这一特点，本方案将触摸传感器输出的原始数据（累加值）定义为 `raw_value` ，并从中衍生出 `baseline` 和 `smooth_value` 两个数据变量，再结合合理的阈值检测算法，最终实现接近感应功能。

具体的软件实现有以下三个步骤：

1. 判断新数据的有效性。
2. 依据 `smooth_value` 和 `baseline` 的更新逻辑，以 `raw_value` 为源数据更新 `smooth_value` 和 `baseline` 。
3. 判断 `smooth_value - baseline` 的值是否大于 0，若大于 0 则再判断是否大于 **触发阈值** ，若大于则判定为有效的感应触发动作；
   若 `smooth_value - baseline` 的值小于 0，先判断当前是否处于触发状态，若处于触发状态，则再判断其绝对值是否大于 **解除触发阈值** ，若大于则判定为有效的触发解除动作。

测试硬件参考
--------------------

- 开发板

   - 可以使用的 `ESP-S2S3-Touch-DevKit-1 <https://docs.espressif.com/projects/espressif-esp-dev-kits/zh_CN/latest/esp32s2/esp32-s2-touch-devkit-1/user_guide.html>`__ 开发套件进行验证测试，主板为 ``MainBoard v1.1``，接近感应子板为 ``Proximity Board V1.0``。

配置参考
------------

创建接近感应传感器
^^^^^^^^^^^^^^^^^^^^^^^^

使用 ``touch_proximity_sensor`` 组件，可通过 :cpp:type:`proxi_config_t` 结构体来配置接近感应传感器。

.. code:: c

    // Configuration structure for touch proximity sensor
    typedef struct {
        uint32_t channel_num;
        uint32_t channel_list[TOUCH_PROXIMITY_NUM_MAX];
        uint32_t meas_count;
        float smooth_coef;
        float baseline_coef;
        float max_p;
        float min_n;
        float threshold_p[TOUCH_PROXIMITY_NUM_MAX];
        float threshold_n[TOUCH_PROXIMITY_NUM_MAX];
        float hysteresis_p;
        float noise_p;
        float noise_n;
        uint32_t debounce_p;
        uint32_t debounce_n;
        uint32_t reset_p;
        uint32_t reset_n;
        uint32_t gold_value[TOUCH_PROXIMITY_NUM_MAX];
    } proxi_config_t;

具体参数说明如下：

+----------------+------------------------------------------------------------------+
|    配置参数    |                               说明                               |
+================+==================================================================+
| channel_num    | 触摸接近感应通道数量，最多支持 3 个                              |
+----------------+------------------------------------------------------------------+
| channel_list   | 触摸接近感应通道列表，即触摸通道                                 |
+----------------+------------------------------------------------------------------+
| meas_count     | 接近感应通道的累计测量次数，值越大，数据更新越慢                 |
+----------------+------------------------------------------------------------------+
| smooth_coef    | 数据平滑处理系数，降低数据波动                                   |
+----------------+------------------------------------------------------------------+
| baseline_coef  | 基线系数，确定基线调整的速率，用于消除环境变化的影响             |
+----------------+------------------------------------------------------------------+
| max_p          | 最大有效正变化率                                                 |
+----------------+------------------------------------------------------------------+
| min_n          | 最小有效负变化率                                                 |
+----------------+------------------------------------------------------------------+
| threshold_p    | 正向触发阈值                                                     |
+----------------+------------------------------------------------------------------+
| threshold_n    | 负向触发阈值                                                     |
+----------------+------------------------------------------------------------------+
| hysteresis_p   | 正阈值迟滞系数，在触发和解除触发之间提供缓冲区，以防止连续误触发 |
+----------------+------------------------------------------------------------------+
| noise_p        | 正噪声阈值，基线更新与该值有关                                   |
+----------------+------------------------------------------------------------------+
| noise_n        | 负噪声阈值，基线更新与该值有关                                   |
+----------------+------------------------------------------------------------------+
| debounce_p     | 正阈值的去抖动次数，以减少误触发                                 |
+----------------+------------------------------------------------------------------+
| debounce_n     | 负阈值的去抖动次数，以减少误解除触发                             |
+----------------+------------------------------------------------------------------+
| reset_p        | 触发基线重置的正向阈值                                           |
+----------------+------------------------------------------------------------------+
| reset_n        | 触发基线重置的负向阈值                                           |
+----------------+------------------------------------------------------------------+
| gold_value     | 金标准值，用于在特殊情况下恢复正常值                             |
+----------------+------------------------------------------------------------------+

然后使用 :cpp:func:`touch_proximity_sensor_create` 配置并创建接近感应传感器对象。

.. code:: c

    proxi_config_t config = (proxi_config_t)DEFAULTS_PROX_CONFIGS();
    esp_err_t ret = touch_proximity_sensor_create(&config, &s_touch_proximity_sensor, &example_proxi_callback, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "touch proximity sense create failed");
    }

其中， `s_touch_proximity_sensor` 为触摸接近感应传感器句柄， `example_proxi_callback` 为接近感应传感器事件回调函数。

启动和停止接近感应传感器
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
使用 :cpp:func:`touch_proximity_sensor_start` 启动接近感应传感器：

.. code:: c

    // Start the touch proximity sensor
    touch_proximity_sensor_start(s_touch_proximity_sensor);

使用 :cpp:func:`touch_proximity_sensor_stop` 停止接近感应传感器：

.. code:: c

    // Stop the touch proximity sensor
    touch_proximity_sensor_stop(s_touch_proximity_sensor);

.. Note:: 接近感应传感器的启动和停止过程需要一定时间才能完成，因此，在调用启动和停止 API 之后，添加等待时间是有必要的，通常，启动时间为 300 ms，停止过程需 200 ms，详情请参考示例程序。

删除接近感应传感器
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
使用 :cpp:func:`touch_proximity_sensor_delete` 删除接近感应传感器对象，并释放资源：

.. code:: c

    // Delete the touch proximity sensor
    touch_proximity_sensor_delete(s_touch_proximity_sensor);

参数调节参考
------------------

* channel_num 最大为 3。
* channel_list 数组必须赋值为 `touch_pad_t` 枚举变量中的值。
* meas_count 数值越大，触摸传感器新数据的更新速率越慢。
* smooth_coef 是数据平滑处理系数，平滑后的 `smooth` 值等于 `smooth * (1.0 - smooth_coef) + raw * smooth_coef`， `smooth_coef` 数值越大， `raw` 的权重就越大，平滑效果越差， `smooth` 波形越抖， `smooth` 跟随 `raw` 值速度越快，触发响应越快，抗干扰能力越弱； `smooth_coef` 数值越小， `raw` 的权重就越小，平滑效果越好， `smooth` 波形越平滑， `smooth` 跟随 `raw` 值速度越慢，触发响应越慢，抗干扰能力越强。
* baseline_coef 是基线更新系数，基线新值等于 `baseline * (1.0 - baseline_coef) + smooth * baseline_coef`，该值越大，基线跟随 `smooth` 速度越快，触发响应越慢，抗干扰能力越强。
* max_p 当 `raw - baseline` 的值大于 `baseline * max_p` 时， `raw` 值为异常值，忽略掉。
* min_n 当 `baseline - raw` 的值大于 `baseline * min_n` 时， `raw` 值为异常值，忽略掉。
* threshold_p 值越大，接近感应触发的距离越近，抗干扰能力越强，反之相反。
* threshold_n 值越大，接近感应触发的距离越近，抗干扰能力越强，反之相反。
* noise_p 和 noise_n 的值越大，基线更容易跟随 `smooth`，接近感应距离会相应变小，抗干扰能力越好。
* debounce_p 和 debounce_n 的值需要参考 `meas_count` 的值进行调整， `meas_count` 越小， `debounce_p` 和 `debounce_n` 应相应增大，以提高抗干扰能力。

调参波形对比
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
默认的触摸接近感应传感器配置参数如下：

+----------------+----------------+
|      参数      |     默认值     |
+================+================+
| channel_num    | 1              |
+----------------+----------------+
| channel_list   | TOUCH_PAD_NUM8 |
+----------------+----------------+
| meas_count     | 50             |
+----------------+----------------+
| smooth_coef    | 0.2            |
+----------------+----------------+
| baseline_coef  | 0.1            |
+----------------+----------------+
| max_p          | 0.2            |
+----------------+----------------+
| min_n          | 0.08           |
+----------------+----------------+
| threshold_p    | 0.002          |
+----------------+----------------+
| threshold_n    | 0.002          |
+----------------+----------------+
| hysteresis_p   | 0.2            |
+----------------+----------------+
| noise_p        | 0.001          |
+----------------+----------------+
| noise_n        | 0.001          |
+----------------+----------------+
| debounce_p     | 2              |
+----------------+----------------+
| debounce_n     | 1              |
+----------------+----------------+
| reset_p        | 1000           |
+----------------+----------------+
| reset_n        | 3              |
+----------------+----------------+

以下调参对比都将在以上参数基础上 **仅修改一个参数** 进行对比。

1. 修改 `meas_count` 的值，将改变传感器数据更新速率，其值越大，传感器数据更新速率越慢。
   测试现象：将手放在感应面板上方 10cm 处保持 3 秒时间，较小的 `meas_count` 数值，感应时的波形宽度将更宽，波形对比图如下：

    .. figure:: ../../_static//touch/touch_proximity_sensor/meas_count.png
        :align: center
        :width: 75%
        :alt: 不同 meas_count 下的波形对比图

2. 修改 `smooth_coef` 的值，将改变 `smooth` 波形的平滑效果。 `smooth_coef` 值越小，平滑效果越好，抗干扰能力越强， `smooth` 跟随 `raw` 越慢，触发响应越慢，反之相反。
   不同 `smooth_coef` 下的波形对比图如下：

    .. figure:: ../../_static/touch/touch_proximity_sensor/smooth_coef.png
        :align: center
        :width: 75%
        :alt: 不同 smooth_coef 下的波形对比图

3. 修改 `baseline_coef` 的值，将改变 `baseline` 的更新效果。 `baseline_coef` 值越小， `baseline` 跟随 `smooth` 越慢，触发响应越慢，触发的持续时间越长，反之相反。
   不同 `baseline_coef` 下的波形对比图如下：

    .. figure:: ../../_static/touch/touch_proximity_sensor/baseline_coef.png
        :align: center
        :width: 75%
        :alt: 不同 baseline_coef 下的波形对比图

4. 修改 `max_p` 和 `min_n` 的值，将改变 `smooth` 和 `baseline` 的更新逻辑。 `max_p` 值太小，会导致手接近感应面板时 `smooth` 被“锁住”，进而可能引发无法触发的情况； `min_n` 值太小，会导致手离开感应面板时 `smooth` 和 `baseline` 都被“锁住”，进而引发无法解除触发的情况。
    `max_p` 和 `min_n` 太小时的波形图如下：

    .. figure:: ../../_static/touch/touch_proximity_sensor/max_p_and_min_n.png
        :align: center
        :width: 75%
        :alt: 不同 max_p 和 min_n 下的波形对比图

5. 修改 `threshold_p` 的值，将改变接近感应的距离，其值越小，能够感应的距离越远，但抗干扰能力越差，易引发误触发。
   不同 `threshold_p` 下的波形对比图如下：

    .. figure:: ../../_static/touch/touch_proximity_sensor/threshold.png
        :align: center
        :width: 75%
        :alt: 不同 threshold_p 下的波形对比图

6. 修改 `hysteresis_p` 的值，将改变触发和解除触发的时间点，即触发迟滞和解除触发迟滞。 `hysteresis_p` 的值越小，触发响应越快，反之相反。
   不同 `threshold_p` 下的波形对比图如下：

    .. figure:: ../../_static/touch/touch_proximity_sensor/hysteresis_p.png
        :align: center
        :width: 75%
        :alt: 不同 hysteresis_p 下的波形对比图

7. 修改 `noise_p` 和 `noise_n` 的值，将改变 `baseline` 的更新效果。 `noise_p` 的值越小， `baseline` 跟随 `smooth` 越慢，触发响应越慢，触发的持续时间越长，反之相反。
   不同 `noise_p` 和 `noise_n` 下的波形对比图如下：

    .. figure:: ../../_static/touch/touch_proximity_sensor/noise.png
        :align: center
        :width: 75%
        :alt: 不同 noise_p 和 noise_n 下的波形对比图

8. 修改 `debounce_p` 和 `debounce_n` 的值，将改变触发和解除触发的时间点和抗干扰能力。 `debounce_p` 的值越大，触发响应越慢，抗干扰能力越强，反之相反； `debounce_n` 的值越大，解除触发响应越慢，抗干扰能力越强，反之相反。
   `debounce_p` 和 `debounce_n` 的值需要结合 `meas_count` 来调节， `meas_count` 的值减小， `debounce_p` 和 `debounce_n` 的值应适当增大。
   不同 `noise_p` 和 `noise_n` 下的波形对比图如下：

    .. figure:: ../../_static/touch/touch_proximity_sensor/debounce.png
        :align: center
        :width: 75%
        :alt: 不同 debounce_p 和 debounce_n 下的波形对比图

.. Note:: 要达到理想的接近感应效果，仅对一两个参数进行简单调节是不够的，需要综合调整多个参数。

示例程序
-------------

- :example:`touch/touch_proximity`

API Reference
-----------------

.. include-build-file:: inc/touch_proximity_sensor.inc
