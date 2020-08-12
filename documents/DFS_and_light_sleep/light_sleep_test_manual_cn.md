## Ligth_sleep 电流测试说明

这篇文档将介绍如何在 esp-iot-solution 平台下进行 Light_sleep 期间电流的测试. Light_sleep 测试代码在 esp-iot-solution 中可以找到[测试代码链接](/tools/unit-test-app/components/light_sleep).

- **1. 工具链的安装**

esp-iot-solution 使用 esp-idf 同一套工具链, 所以如果你的平台上能够编译 esp-idf 中的 example, 则工具链无须重复安装. 工具链的详细安装步骤可以在 [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/stable/get-started/index.html) 找到, 这里与不多加描述.

- **2. 测试代码下载**

你需要将完整的 esp-iot-solution 工程下载下来, 才能进行测试. 使用如下指令:

```
1.    $ git clone -b release/v1.1 https://github.com/espressif/esp-iot-solution.git
2.    $ cd esp-iot-solution
3.    $ git submodule update --init --recursive
```

更新 submodule 需要花费一点时间, 所以请耐心等待. 如果本地已经有 esp-iot-solution 仓库, 请更新到最新版本.

- **3. 测试代码编译及下载**

esp-iot-solution  里放置了很多测试代码和示例工程, 一般是放在 `esp-iot-solution/components` 或 `esp-iot-solution/tools/unit-test-app/components` 目录下. 所有测试代码编译方式都基本相同, 这里以 Light_sleep 为示例(Light_sleep测试代码放在了 `esp-iot-solution/tools/unit-test-app/components` 目录下).

* 1 工程配置

```
1.    $ make menuconfig
```

在 menuconfig-->Serial flasher config 中修改串口号和波特率.

- 2 代码编译

```
1.    $ cd esp-iot-solution/tools/unit-test-app
2.    $ make TEST_COMPONENTS=light_sleep
```

- 3 固件烧写

代码编译通过后, 将编译生成的 .bin 文件烧写到开发板上. 首先将 GPIO0 拉低, 然后按下复位键让芯片进入下载模式. 执行指令:

```
1.    $ make flash
```

`波特率太高, 可能会下载失败, 太低则烧写过程缓慢. 一般可以配置成 921600.`

(Light_sleep 测试代码可以直接在 ESP32_ULP_EB 测试板上进行测试. 如果使用其他开发板, 请修改测试代码后再进行测试).

- **4. Light_sleep 电流测试**

打开串口终端, 按下复位键后, 芯片启动, 你将看到如下打印信息:

```
...

Here's the test menu, pick your combo:
(1)	"Light_sleep get wake_up cause test" [light_sleep][iot]
(2)	"Light_sleep EXT0 wakeup test" [light_sleep][iot]
(3)	"Light_sleep EXT1 wakeup test" [light_sleep][iot]
(4)	"Light_sleep touch_pad wakeup test" [light_sleep][iot]
(5)	"Light_sleep time wakeup test" [light_sleep][iot]
(6)	"Time to enter light_sleep test" [light_sleep][iot]
```

log 上列举的就是我们可以进行的测试选项, 输入 5 则进行 Light_sleep time wakeup test. 在 Light_sleep 期间, 我们可以对电流进行测试.[接线图](../evaluation_boards/esp32_ulp_eb_cn.md#compileAndRun). Light_sleep 期间, 电流是 800uA 左右.

`Note : 在 Light_sleep 期间, 会有部分 GPIO(GPIO18, GPIO19, GPIO21, GGPIO22) 漏电, 所以建议在调用 esp_light_sleep_start 之前, 可以调用gpio_set_direction(), 将这些 GPIO 的输入与输出关闭.`
