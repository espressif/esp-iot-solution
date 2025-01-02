# Bootloader TinyUF2 示例

UF2是一种由微软为[PXT](https://github.com/Microsoft/pxt)开发的文件格式，它尤其适用于通过海量存储设备类（Mass Storage Class）对微控制器进行闪存烧录。如需更通俗易懂的解释，请查看[这篇博客文章](https://makecode.com/blog/one-chip-to-flash-them-all)。

## 如何使用

### 编译和烧录

运行 `idf.py build` 编译示例

(要退出串口监视器，请按 Ctrl-])

请参阅[入门指南](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html)，获取配置和使用ESP-IDF构建项目的完整步骤。

### 拖放以进行升级

* 构建并烧录首个支持 Bootloader TinyUF2 的固件。
* 通过将 `CONFIG_BOOTLOADER_UF2_GPIO_NUM` 拉到 `CONFIG_BOOTLOADER_UF2_GPIO_LEVEL` 电平 `CONFIG_BOOTLOADER_UF2_GPIO_PULL_TIME_SECONDS` 秒后，将会进入 TinyUF2 模式。
* 进入到 TinyUF2 模式后，连接至 `BOOTLOADER_UF2_LED_INDICATOR_GPIO_NUM` 的指示灯将会闪烁。
* 通过 USB 将`ESP32S3`插入电脑，在文件管理器中将会发现名为`ESP32S3 - UF2`的新磁盘。
* 运行`idf.py uf2-ota`来生成或将后续的固件升级文件转换为UF2格式，更多详细信息请参阅[esp_tinyuf2](../../../../components/usb/esp_tinyuf2/)。
* 将UF2格式的固件拖放到磁盘中以进行升级。
* 升级完毕后，设备将自动重启，并从 `Factory` 分区启动

![UF2磁盘](../../../../components/usb/esp_tinyuf2/uf2_disk.png)

### 通过 `INI` 文件修改 `NVS` 中的配置

准备工作：

* 构建并烧录首个支持 Bootloader TinyUF2 的固件。
* 通过将 `CONFIG_BOOTLOADER_UF2_GPIO_NUM` 拉到 `CONFIG_BOOTLOADER_UF2_GPIO_LEVEL` 电平 `CONFIG_BOOTLOADER_UF2_GPIO_PULL_TIME_SECONDS` 秒后，将会进入 TinyUF2 模式。
* 通过 USB 将`ESP32S3`插入电脑，在文件管理器中将会发现名为`ESP32S3 - UF2`的新磁盘。
* 打开 `CONFIG.INI` 文件，修改其中的配置，然后保存。
* 保存的文件将会被自动解析，并将新的键值对写入 NVS 中。

### 已知问题：

打印出以下报错
```
uf2: No such namespace entry was found
```

- 这是因为 NVS 分区中还未存在 `CONFIG_BOOTLOADER_UF2_NVS_NAMESPACE_NAME`(uf2_nvs) 的命名空间，不影响正常使用。

### 更多文档介绍：

[bootloader_uf2](https://docs.espressif.com/projects/esp-iot-solution/zh_CN/latest/usb/usb_device/esp_tinyuf2.html)

### 示例输出

```
I (8456) esp_image: segment 0: paddr=00010020 vaddr=3f000020 size=0857ch ( 34172) map
I (8466) esp_image: segment 1: paddr=000185a4 vaddr=3ffbd8c0 size=01d98h (  7576)
I (8466) esp_image: segment 2: paddr=0001a344 vaddr=40022000 size=05cd4h ( 23764)
I (8476) esp_image: segment 3: paddr=00020020 vaddr=40080020 size=18248h ( 98888) map
I (8496) esp_image: segment 4: paddr=00038270 vaddr=40027cd4 size=05be4h ( 23524)
I (8506) esp_image: segment 5: paddr=0003de5c vaddr=50000000 size=00010h (    16)
I (8556) uf2_example: Firmware update complete
I (8556) uf2_example: Restarting in 5 seconds...
I (9556) uf2_example: Restarting in 4 seconds...
I (10556) uf2_example: Restarting in 3 seconds...
I (11556) uf2_example: Restarting in 2 seconds...
I (12556) uf2_example: Restarting in 1 seconds...
I (13556) uf2_example: Restarting in 0 seconds...
I (14556) uf2_example: Restarting now

```