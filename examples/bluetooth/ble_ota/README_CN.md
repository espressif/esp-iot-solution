# ESP BLE OTA 示例说明

``ble ota demo`` 基于 ``ble ota component``，通过 BLE 接收待升级的固件，然后以扇区为单位，依次写入 flash，直到升级完成。

## 1. Services 定义
 
组件包含两个服务：

  - `DIS Service`：展示软硬件版本信息

  - `OTA Service`：用于 OTA 升级，包含 4 个 characteristics，如下表所示：

|  特征值   | UUID  |  权限   | 说明  |
|  ----  | ----  |  ----  | ----  |
|  RECV_FW_CHAR | 0x8020 | Write, notify  | 固件接收，回复ACK |
|  PROGRESS_BAR_CHAR  | 0x8021 | Read, notify  | 读取进度条，上报进度条 |
|  COMMAND_CHAR  | 0x8022 | Write, notify  | 命令下发与ACK回复 |
|  CUSTOMER_CHAR  | 0x8023 | Write, notify  | ⽤用户⾃自定义数据收发 |

## 2. 数据传输

### 2.1 命令包格式

|  单位   | Command_ID  |  PayLoad   | CRC16  |
|  ----  | ----  |  ----  | ----  |
|  字节 | Byte: 0 ~ 1 | Byte: 2 ~ 17  | Byte: 18 ~ 19 |

Command_ID:

  - 0x0001: 开始 OTA，Payload bytes(2 ~ 5)，用于指示固件的长度信息，其余 Payload 默认置 0，CRC16 计算的是 bytes(0 ~ 17)。
  - 0x0002: 结束 OTA，剩余 Payload 全部置 0，CRC16 计算的是 bytes(0 ~ 17)。
  - 0x0003: 命令包的回复，Payload bytes(2 ~ 3)，填充准备回复的 Command_ID；Payload bytes(4 ~ 5) 是对该命令的响应，0x0000 表示接受，0x0001 表示拒绝；其余 Payload 全部置 0，CRC16 计算的是 bytes(0 ~ 17)。

### 2.2 固件包格式

主机发送的固件包格式如下：

|  单位   | Sector_Index  |  Packet_Seq   | PayLoad  |
|  ----  | ----  |  ----  | ----  |
|  字节 | Byte: 0 ~ 1 | Byte: 2  | Byte: 3 ~ (MTU_size - 4) |

  - Sector_Index：指示写⼊的是第几个扇区，扇区号从 0 开始递增，不能跳跃，必须发送完 4K 数据才能开始下一个扇区，否则会⽴立刻回复错误，请求重传。
  - Packet_Seq：如果 Packet_Seq 是 0xFF，表示这是这个扇区的最后⼀包，Payload 有效载荷的最后 2字节 是整个扇区 4K 数据的 CRC16 的值, 剩余字节填充 0x0。从机会校验数据的总⻓长度和 CRC，并给主机回复正确的 ACK，然后开始发送下一个扇区数据。

从机回复主机固件包的应答包格式如下：

|  单位   | Sector_Index  |  ACK_Status   | CRC6  |
|  ----  | ----  |  ----  | ----  |
|  字节 | Byte: 0 ~ 1 | Byte: 2 ~ 3  | Byte: 18 ~ 19 |

ACK_Status:

  - 0x0000：成功
  - 0x0001：CRC 错误
  - 0x0002：Sector_Index 错误，bytes(4 ~ 5) 表示期望的 Sector_Index
  - 0x0003：Payload length 错误

## 3. 主机示例程序

[APP](https://github.com/EspressifApps/esp-ble-ota-android/releases/tag/rc)

## 4. 从机示例代码

[ESP BLE OTA](https://github.com/espressif/esp-iot-solution/tree/master/examples/bluetooth/ble_ota)
