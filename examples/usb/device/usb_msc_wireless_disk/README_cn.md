## USB MSC 无线磁盘

使用 ESP32-Sx 作为带有无线访问功能的 USB 磁盘。HTTP 文件服务器可用于上传和下载文件。

**此演示仅用于功能预览，如果您发现错误请不要感到意外**

### 硬件

- 开发板：ESP32-S3-USB-OTG，或任何 ESP32-Sx 开发板
- 微控制器（MCU）：ESP32-S2，ESP32-S3
- Flash 存储：4MB NOR Flash
- 硬件连接：
  - GPIO19 连接到 D-
  - GPIO20 连接到 D+
  - SD 卡 IO 根据不同的开发板而异，您可以在代码中自定义定义。

注意：如果您使用的是自供电设备，请参考[自供电设备](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/usb_device.html#self-powered-device)

### 功能

1. 支持 USB MSC，可以通过主机读写板载 Flash 或 SD 卡；
2. 通过 Wi-Fi 下载和上传数据，ESP32-SX 可以作为 Wi-Fi AP 或 STA。

### 如何使用

1. 使用微型 USB 电缆直接插入主机的 USB 端口；
2. 默认情况下，在您的“文件资源管理器”中可以找到一个大小为 `1.5MB` 的磁盘；
3. 通过 Wi-Fi 连接到 ESP32S2，SSID：`ESP-Wireless-Disk`，默认没有密码；
4. 在浏览器中输入 `192.168.4.1`，您可以查看磁盘上的文件列表；
5. 拖放到磁盘的任何文件都将显示在网页中；
6. 从网页上传的任何文件也会显示在磁盘中（此演示中文件大小必须小于 20MB）。

### 配置 Wi-Fi

* 您可以在 `menuconfig → USB MSC Device Demo → Wi-Fi Settings` 中配置 Wi-Fi AP 的 SSID 和密码，以更改 esp32-sx 的热点名称。
* 您也可以设置 Wi-Fi STA 的 SSID 和密码，以使 esp32-sx 同时连接到路由器。
* 您还可以通过网页端的 `settings` 来配置 Wi-Fi 的相关信息。

### 已知问题

* 通过 USB 磁盘添加或删除的文件后，请主动刷新网页