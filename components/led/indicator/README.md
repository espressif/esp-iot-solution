# Component: led

* This component defines an led as a well encapsulated object.

* An led device is defined by:
	* `GPIO number` on which the led is attached
	* `dark_level` which decided by peripheral hardware

* An led device can provide:
	* `LED_OFF`, `LED_ON`, `LED_QUICK_BLINK` and `LED_SLOW_BLINK` four states which can be set by calling iot_led_state_write()
	* `LED_NORMAL_MODE` and `LED_NIGHT_MODE` two modes which can be set by calling iot_led_mode_write()
	* iot_led_state_read() or iot_led_mode_read() can be called to get the current state or mode of the led
    * iot_led_update_blink_freq() can be called to change the blink frequency of all the leds
    * iot_led_night_duty_write() can be called to set the duty of led in night mode

* To use the led device, you need to:
	* call iot_led_setup to initialize ledc timers and set the frequency of quick_blink and slow_blink
	* create a led device object by iot_led_create()
	* to free the led object, you can call iot_led_delete() to delete the object and free the memory

### NOTE:
> `LEDC_TIMER_0`, `LEDC_TIMER_1` and `LEDC_TIMER_2` has been `used` by this module.

> `LEDC_CHANNEL_0`, `LEDC_CHANNEL_1`, `LEDC_CHANNEL_2`, `LEDC_CHANNEL_3` has been `used` by this module.

> Don't call iot_led_delete() `more than once` for the same led_handle and you'd better to set the led_hanle to `NULL` after `iot_led_delete`. 



## 小米 LED 指示灯规范

https://iot.mi.com/new/doc/standard/embedded-standard/embedded-development-standard

| 状态             | 闪烁方式                  | 对应net命令的结果 |
| ---------------- | ------------------------- | ----------------- |
| 等待快连中       | 黄灯闪烁（黄0.1s 灭0.2s） | uap               |
| 成功连接到路由器 | 蓝灯长亮                  | local 或cloud     |
| 掉线重连中       | 蓝灯闪烁（蓝0.1s 灭0.2s） | offline           |
| 升级中           | 黄灯慢闪（黄0.2s 灭0.8s） | updating          |
| 配网功能关闭     | 灯灭                      | unprov            |
|                  |                           |                   |
|                  |                           |                   |

| 状态             | 单色LED闪烁方式                    | 对应net命令   |
| ---------------- | ---------------------------------- | ------------- |
| 等待快连中       | 慢闪（亮0.2s灭0.8s）               | uap           |
| 成功连接到路由器 | 长亮                               | local 或cloud |
| 掉线重连中       | 快闪（亮0.1s灭0.2s）               | offline       |
| 升级中           | 双闪（亮0.05s灭0.1s亮0.05s灭0.8s） | updating      |
| 配网功能关闭     | 灯灭                               | unprov        |
|                  |                                    |               |

阿里智能插座

| 状态                                         | 默认LED显示                                                  |
| :------------------------------------------- | :----------------------------------------------------------- |
| 配网模式                                     | 插座LED反复闪烁，亮0.8秒，灭0.8秒。                          |
| 恢复出厂设置                                 | 插座LED反复闪烁，亮0.2秒，灭0.2秒。                          |
| 连接AP 超时/连接AP 认证失败（超时时间2分钟） | 插座LED反复闪烁的模式更改为，亮0.5秒、灭0.5秒，闪烁两分钟之后停止闪烁。停止闪烁之后，如果插座配电使能则LED灯点亮，否则LED灯灭掉。 |
| 连接AP成功、尝试连云                         | 插座LED反复闪烁，亮0.8 秒，灭0.8秒，然后开始尝试连接云端。   |
| 连云失败                                     | 连接云端失败后，需要再次尝试连接，其间LED的显示与"连接AP成功、尝试连云"模式一样。 |
| 连云成功                                     | 当设备连接云端成功，则停止LED闪烁，若插座配电打开则LED点亮，若插座配电未打开则LED灭掉。 |

Rainmaker Parameters

|Name|Type|Data Type|UI Type|Properties|Min, Max, Step|
| :-----: | :------: | :------: | :------: | :------: |:------: |
|Name|esp.param.name|String|Read, Write|1, 32, -|
|Power|esp.param.power|Bool|esp.ui.toggle|Read, Write|
|Brightness|esp.param.brightness|Int|esp.ui.slider|Read, Write|0, 100, 1|
|Color Temperature|esp.param.cct|Int|esp.ui.slider|Read, Write|2700, 6500, 100|
|Hue|esp.param.hue|Int|esp.ui.slider|Read, Write|0, 360, 1|
|Saturation|esp.param.saturation|Int|esp.ui.slider|Read, Write|0, 100, 1|
|Intensity|esp.param.intensity|Int|esp.ui.slider|Read, Write|0, 100, 1|
|Speed|esp.param.speed|Int|esp.ui.slider|Read, Write|0, 5, 1|
|Direction|esp.param.direction|Int|esp.ui.dropdown|Read, Write|0, 1, 1|
|Temperature|esp.param.temperature|Float||Read|
|OTA URL|esp.param.ota_url|String||Write|
|OTA Status|esp.param.ota_status|String||Read|
|OTA Info|esp.param.ota_info|String||Read|