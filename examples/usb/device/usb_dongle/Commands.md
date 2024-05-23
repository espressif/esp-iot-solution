### 命令介绍

#### 1.help

**Function:**

列出所有注册的命令

**Command:**

```
help
```

**Response:**

```
help:
 Lists all the registered commands

ap <ssid> [<password>]: configure ssid and password
sta -s <ssid> [-p <password>]: join specified soft-AP
sta -d: disconnect specified soft-AP
mode <mode>: <sta> station mode; <ap> ap mode
smartconfig [op]: op:1, start smartconfig; op:0, stop smartconfig
scan [<ssid>]: <ssid>  SSID of AP want to be scanned
ram: Get the current size of free heap memory and minimum size of free heap memory
restart: Software reset of the chip
version: Get version of chip and SDK
>
```

#### 2.ap

**Function:**

设置 AP 模式、查询 AP 设置

**Set Command:**

```
ap Soft_AP espressif
```

**Query Command:**

```
ap
```

**Response:**

```
AP mode:Soft_AP,espressif
>
```

Note：

>password 为可选项，若不配置默认不加密

#### 3.sta

**Function:**

启动 Station 模式、查询所连接 AP 信息 

**Set Command:**

```
sta -s AP_Test -p espressif
```

**Query Command:**

```
sta
```

**Response:**

```
<ssid>,<channel>,<listen_interval>,<authmode>
>
```

| authmode_value | mode                      |
| :------------: | :------------------------ |
|       0        | WIFI_AUTH_OPEN            |
|       1        | WIFI_AUTH_WEP             |
|       2        | WIFI_AUTH_WPA_PSK         |
|       3        | WIFI_AUTH_WPA2_PSK        |
|       4        | WIFI_AUTH_WPA_WPA2_PSK    |
|       5        | WIFI_AUTH_WPA2_ENTERPRISE |
|       6        | WIFI_AUTH_WPA3_PSK        |
|       7        | WIFI_AUTH_WPA2_WPA3_PSK   |
|       8        | WIFI_AUTH_WAPI_PSK        |

Note：

>password 为可选项

**Function:**

断开与 AP 的连接

**Set Command:**

```
sta -d
```

**Response:**

```
OK
>
```

#### 4.mode

**Function:**

设置 WiFi 模式

**Command:**

* 设置 Station 模式

    ```
    mode sta
    ```

* 设置 AP 模式

    ```
    mode ap
    ```

#### 5.smartconfig

**Function:**

* 开启 SmartConfig 配网

    **Command:**

    ```
    smartconfig 1
    ```

    **Response:**

    ```
    >SSID:FAST_XLZ,PASSWORD:12345678
    OK
    >
    ```

* 关闭 SmartConfig 配网

    **Command:**

    ```
    smartconfig 0
    ```

    **Response:**

    ```
    OK
    >
    ```

    Note:

    >使用 `smartconfig 1` 命令开启 SmartConfig 配网并成功连接后，不需要再使用 `smartconfig 0` 命令来关闭 SmartConfig 配网
    >
    >`smartconfig 0` 命令只需要在 SmartConfig 配网失败时进行调用

配网步骤：

>* 下载 ESPTOUCH APP ：[Android source code](https://github.com/EspressifApp/EsptouchForAndroid)    [iOS source code](https://github.com/EspressifApp/EsptouchForIOS) 
>* 确保你的手机连接至目标 AP（2.4GHz）
>* 打开 ESPTOUCH APP 输入 password 并确认
>* PC 端通过 USB 端口发送 `smartconfig 1` 命令

#### 6.scan

**Function:**

扫描 AP 并列出对应 SSID 以及 RSSI

**Command:**

* 扫描特定 AP

    ```
    scan <SSID>
    ```

* 扫描所有 AP

    ```
    scan
    ```

**Response:**

```
>
[ssid][rssi=-22]
>
```

#### 7.ram

**Function:**

获取当前剩余内存大小以及系统运行期间最小时内存大小

**Command:**

```
ram
```

**Response:**

```
free heap size: 132612, min heap size: 116788
>
```

#### 8.restart

**Function:**

重启系统

**Command:**

```
restart
```

#### 9.version

**Function:**

获取当前 IDF 版本以及芯片信息

**Command:**

```
version
```

**Response:**

```
IDF Version:v4.4-dev-2571-gb1c3ee71c5
Chip info:
	cores:1
	feature:/802.11bgn/External-Flash:2 MB
	revision number:0
>
```

