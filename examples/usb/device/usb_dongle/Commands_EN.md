### Command Set

#### 1.help

**Function:**

List all registered commands.

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

Set/get softAP configuration.

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

Note:

>password is optional. If you didn't set any password, then the ESP softAP will be set into the open mode.

#### 3.sta

**Function:**

Set station mode, query the information of the AP it connected to.

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

Note:

>password is optional. 

**Function:**

Disconnect from AP.

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

Set Wi-Fi mode.

**Command:**

* Set Station mode

    ```
    mode sta
    ```

* Set AP mode

    ```
    mode ap
    ```

#### 5.smartconfig

**Function:**

* Start SmartConfig to connect to target AP.

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

* Stop SmartConfig.

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

    > After called `smartconfig 1` command, if the ESP device connects to the target AP successfully, then you don't need to call `smartconfig 0` to stop SmartConfig.
    >
    >`smartconfig 0` command only needs to be called, when the SmartConfig fails.

SmartConfig steps:

>* Download ESPTOUCH APP to your phone: [Android source code](https://github.com/EspressifApp/EsptouchForAndroid)    [iOS source code](https://github.com/EspressifApp/EsptouchForIOS) 
>* Connect your phone to the target AP (2.4GHz).
>* Open ESPTOUCH APP, input AP's password.
>* PC sends command `smartconfig 1` to USB port to start SmartConfig.

#### 6.scan

**Function:**

Scan APs, list their SSID and RSSI.

**Command:**

* Scan a specific AP.

    ```
    scan <SSID>
    ```

* Scan all APs nearby.

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

Get the size of available heap, and get the minimum heap that has ever been available.

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

Restart the system.

**Command:**

```
restart
```

#### 9.version

**Function:**

Get the ESP-IDF and chip version.

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

