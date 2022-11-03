## LED 指示灯

LED 指示灯是最简单的输出外设之一，可以通过不同形式的闪烁指示系统当前的工作状态。ESP-IoT-Solution 提供的 LED 指示灯组件具有以下功能：

* 支持定义多组闪烁类型
* 支持定义闪烁类型优先级
* 支持创建多个指示灯

### 使用方法

#### 预定义闪烁类型

闪烁步骤结构体 blink_step_t 定义了该步骤的类型、指示灯状态和状态持续时间。多个步骤组合成一个闪烁类型，不同的闪烁类型可用于标识不同的系统状态。闪烁类型的定义方法如下：

例 1 . 定义一个循环闪烁：亮 0.05 S，灭 0.1 S，开始之后一直循环。

```
const blink_step_t test_blink_loop[] = {
    {LED_BLINK_HOLD, LED_STATE_ON, 50},               // step1: turn on LED 50 ms
    {LED_BLINK_HOLD, LED_STATE_OFF, 100},             // step2: turn off LED 100 ms
    {LED_BLINK_LOOP, 0, 0},                           // step3: loop from step1
};
```

例 2 . 定义一个循环闪烁：亮 0.05 S，灭 0.1 S，亮 0.15 S，灭 0.1 S，执行完毕灯熄灭。

```
const blink_step_t test_blink_one_time[] = {
    {LED_BLINK_HOLD, LED_STATE_ON, 50},               // step1: turn on LED 50 ms
    {LED_BLINK_HOLD, LED_STATE_OFF, 100},             // step2: turn off LED 100 ms
    {LED_BLINK_HOLD, LED_STATE_ON, 150},              // step3: turn on LED 150 ms
    {LED_BLINK_HOLD, LED_STATE_OFF, 100},             // step4: turn off LED 100 ms
    {LED_BLINK_STOP, 0, 0},                           // step5: stop blink (off)
};
```

定义闪烁类型之后，需要在 `led_indicator_blink_type_t` 添加该类型对应的枚举成员，然后将其添加到闪烁类型列表 `led_indicator_blink_lists`，示例如下：

```
typedef enum {
    BLINK_TEST_BLINK_ONE_TIME, /**< test_blink_one_time */
    BLINK_TEST_BLINK_LOOP,     /**< test_blink_loop */
    BLINK_MAX,                 /**< INVALIED type */ 
} led_indicator_blink_type_t;

blink_step_t const * led_indicator_blink_lists[] = {
    [BLINK_TEST_BLINK_ONE_TIME] = test_blink_one_time,
    [BLINK_TEST_BLINK_LOOP] = test_blink_loop,
    [BLINK_MAX] = NULL,
};
```

#### 预定义闪烁优先级

对于同一个指示灯，高优先级闪烁可以打断正在进行的低优先级闪烁，当高优先级闪烁结束，低优先级闪烁恢复执行。可以通过调整闪烁类型 `led_indicator_blink_type_t` 枚举成员的顺序调整闪烁的优先级，数值越小的成员执行优先级越高。

例如，在以下示例中闪烁 test_blink_one_time 比 test_blink_loop 优先级高，可优先闪烁。

```
typedef enum {
    BLINK_TEST_BLINK_ONE_TIME, /**< test_blink_one_time */
    BLINK_TEST_BLINK_LOOP,     /**< test_blink_loop */
    BLINK_MAX,                 /**< INVALIED type */ 
} led_indicator_blink_type_t;
```

#### 控制指示灯闪烁

创建一个指示灯：指定一个 IO 和一组配置信息创建一个指示灯

```
led_indicator_config_t config = {
    .off_level = 0,                              // attach led positive side to esp32 gpio pin
    .mode = LED_GPIO_MODE,
};
led_indicator_handle_t led_handle = led_indicator_create(8, &config); // attach to gpio 8
```

开始/停止闪烁：控制指示灯开启/停止指定闪烁类型，函数调用后立刻返回，内部由定时器控制闪烁流程。同一个指示灯可以开启多种类型的闪烁，将根据闪烁优先级执行。

```
led_indicator_start(led_handle, BLINK_TEST_BLINK_LOOP); // call to start, the function not block

/*
*......
*/

led_indicator_stop(led_handle, BLINK_TEST_BLINK_LOOP); // call stop
```

删除指示灯：您也可以在不需要进一步操作时，删除指示灯以释放资源

```
led_indicator_delete(&led_handle);
```

> 该组件支持线程安全操作，您可使用全局变量共享 LED 指示灯的操作句柄 led_indicator_handle_t，也可以使用 led_indicator_get_handle 在其它线程通过 LED 的 IO 号获取句柄以进行操作。