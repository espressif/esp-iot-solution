# BTHome bulb 示例说明

``BTHome bulb`` 基于 ``bthome component``，通过 BLE 广播接收和解析符合 BTHome 协议的广播包。

软件实现的效果：
  * 接收由 dimmer 示例发送的加密广播包，并控制LED灯状态。


### 编译和烧写

1. 进入 `bulb` 目录：

    ```linux
    cd ./esp-iot-solution/examples/lighting/bulb
    ```

2. 使用 `idf.py` 工具设置编译芯片，随后编译下载，指令分别为：

    ```linux
    # 设置编译芯片
    idf.py idf.py set-target esp32h2

    # 编译并下载
    idf.py -p PORT build flash
    ```

    请将 `PORT` 替换为当前使用的端口号

### 示例输出结果



关于 BTHome 实现的具体细节请参考：![BTHOME 主页](https://bthome.io/)
