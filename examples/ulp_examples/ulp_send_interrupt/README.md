# ULP 中断发送程序

该示例展示了当 ULP 通过 [WAKE](https://docs.espressif.com/projects/esp-idf/en/stable/api-guides/ulp_instruction_set.html#wake-wake-up-the-chip) 指令向 RTC 控制器发送中断信号时，主程序该如何配置中断处理函数以响应该中断信号。

根据 ULP 的 WAKE 指令说明：

- 如果主 CPU 处于睡眠模式，且 ULP 唤醒被使能，则该中断信号会唤醒主 CPU；
- 如果主 CPU 处于非睡眠模式，且 RTC 中断使能寄存器(RTC_CNTL_INT_ENA_REG) 中对应的 ULP 中断使能位(RTC_CNTL_ULP_CP_INT_ENA) 被置位，则中断信号会触发 RTC 控制器产生一个中断；

以下为 ULP 中断注册和中断处理函数：

```c
static void ulp_isr_install()
{
    ESP_ERROR_CHECK( rtc_isr_register(ulp_isr_handler, NULL, RTC_CNTL_ULP_CP_INT_ENA_M) );
    REG_SET_BIT(RTC_CNTL_INT_ENA_REG, RTC_CNTL_ULP_CP_INT_ENA_M);
}

// ulp interrupt handler
static void IRAM_ATTR ulp_isr_handler(void *args)
{
    ets_printf("ULP interrupt: Active ---> Deepsleep\n");

    // Add customer code here.

    ESP_ERROR_CHECK( esp_sleep_enable_ulp_wakeup() );
    esp_deep_sleep_start();
}
```

示例启动后，ULP 会一直处于周期（时间为 20ms）运行状态，检测 GPIO0 是否被按下。在按键被按下且释放时：

- 如果主 CPU 处于睡眠模式，则该操作会唤醒主 CPU；
- 如果主 CPU 处于非睡眠模式，则该操作会发送一个 RTC 中断，中断程序完成相应的处理后，将主 CPU 设置进入睡眠模式；

系统的日志打印如下：

```
Not ULP wakeup, start ULP program
    system in active mode, time: 0s
    system in active mode, time: 1s
ULP interrupt: Active ---> Deepsleep
ULP wakeup: Deepsleep ---> Active
    system in active mode, time: 0s
    system in active mode, time: 1s
    system in active mode, time: 2s
ULP interrupt: Active ---> Deepsleep
ULP wakeup: Deepsleep ---> Active
    system in active mode, time: 0s
    system in active mode, time: 1s
    system in active mode, time: 2s
ULP interrupt: Active ---> Deepsleep
ULP wakeup: Deepsleep ---> Active
    system in active mode, time: 0s
    system in active mode, time: 1s
ULP interrupt: Active ---> Deepsleep
ULP wakeup: Deepsleep ---> Active
    system in active mode, time: 0s
    system in active mode, time: 1s
    system in active mode, time: 2s
    system in active mode, time: 3s
    system in active mode, time: 4s
    system in active mode, time: 5s
ULP interrupt: Active ---> Deepsleep
```