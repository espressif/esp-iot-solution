# ESP File Transfer 示例

[English](README.md) | **中文**

本示例使用两个独立工程演示 `esp_gmp` File Transfer profile 在两块开发板
之间的文件传输：

- `sender_demo`：主动建立链路，用户通过 Console 决定何时发送文件。
- `receiver_demo`：等待链路连接并被动接收文件。

目录名表示默认演示方向，不限制组件能力。连接建立后，两端都启用
`CONFIG_ESP_GMP_PROFILE_FILE_TRANSFER` 并注册完整 Console，因此任意一端都可以发送文件。

示例集成层选择 BLE GATT 作为物理承载，并通过
`esp_gmp -> GMP Flux adapter -> esp_flux` 建立可靠消息通道。
File Transfer profile 本身不依赖或访问 BLE/Flux。

**范围说明：** 示例与 profile worker 按单链路验证。`sessions[]` 仅为预留结构，
本版本不支持并发多链路文件传输。

## 构建

先加载本地 ESP-IDF 环境：

```bash
source ~/workspace/esp/esp-idf/export.sh
```

构建 sender：

```bash
cd examples/esp_file_transfer/sender_demo
idf.py --preview set-target esp32-h4
idf.py build
```

构建 receiver：

```bash
cd examples/esp_file_transfer/receiver_demo
idf.py --preview set-target esp32-h4
idf.py build
```

分别烧录到两块开发板：

```bash
idf.py flash monitor
```

两个工程都包含一个 FATFS `storage` 分区，挂载点为 `/fatfs`。
构建时生成并烧录 FATFS 镜像，不会在挂载失败时自动格式化。
初始镜像包含 `/fatfs/send/demo.txt`。
示例将单文件上限设置为 2 MiB，为约 2.375 MiB 的 FATFS 分区预留文件系统开销。
如需验证取消或断链，可在构建 sender 前准备一个较大的文件：

```bash
cd examples/esp_file_transfer
dd if=/dev/urandom of=sender_demo/storage/send/large.bin bs=1024 count=1024
```

## 使用

两块板启动后，sender 自动搜索并连接 receiver。开始验证前，确认两端均输出：

```text
File transfer link ready
```

接收文件保存在 `/fatfs/recv`。如果目标名称已存在，组件会依次使用
`_1`、`_2` 等后缀生成新名称。

### 检查配置与初始状态

在两端 Console 分别执行：

```text
ft config
```

预期包含：

```text
Config: recv_dir=/fatfs/recv, max_file_size=2097152, block_size=auto
Link: ready
```

然后执行：

```text
ft status
```

预期：

```text
Status: idle
```

### 正向传输

在 sender Console 执行：

```text
ft send /fatfs/send/demo.txt
```

sender 预期事件顺序：

```text
Transfer started
Transfer metadata sent
Transfer peer accepted
Progress
Transfer complete
```

receiver 预期事件顺序：

```text
Transfer metadata received
Transfer started
Progress
Transfer verifying
Transfer complete
```

其中 sender 只有在 receiver 完成文件写入、SHA256 校验、安全落盘并返回
final confirm success 后，才会输出 `Transfer complete`。

完成后在两端分别执行：

```text
ft status
```

预期均为：

```text
Status: idle
```

### 同名文件自动重命名

在 sender 再次执行：

```text
ft send /fatfs/send/demo.txt
```

如果 `/fatfs/recv/demo.txt` 已存在，完成日志应包含：

```text
saved=demo_1.txt
```

继续发送时依次使用 `demo_2.txt`、`demo_3.txt` 等名称，已有文件不会被覆盖。

### 反向传输

在 receiver Console 执行：

```text
ft send /fatfs/send/demo.txt
```

此时 receiver 作为发送端，sender 被动接收并保存到 `/fatfs/recv`。两端事件
顺序与正向传输相同，只是 sender/receiver 角色互换。receiver 不需要
`ft receive` 命令。

### 取消传输

使用传输时间足够长的文件，在发起端执行：

```text
ft send /fatfs/send/large.bin
```

传输过程中立即执行：

```text
ft cancel
```

预期发起端输出：

```text
Cancel requested
Transfer cancelled
```

对端应结束当前传输并删除对应 `ft_tmp/*.part`。完成清理后，两端执行
`ft status` 均应回到 `Status: idle`。

### 断链恢复

传输大文件时复位或断开任意一端。预期当前传输失败，接收端不保留 `.part`；
重新建立连接并再次出现 `File transfer link ready` 后，可重新执行发送命令。

注意：完整执行 `idf.py flash` 会重新烧录示例 FATFS 镜像，覆盖运行期间接收的
文件。只更新应用且希望保留 `/fatfs` 时使用 `idf.py app-flash`。

## Console 命令

| 命令 | 说明 |
| --- | --- |
| `ft send <path>` | 异步发送指定文件，远端名称默认为 basename。 |
| `ft status` | 显示当前角色、状态、文件名和进度。 |
| `ft cancel` | 取消当前 active transfer。 |
| `ft config` | 显示接收目录、大小配置和链路状态。 |

receiver 不需要 `receive` 命令。收到 metadata 后，接收状态机会自动启动。
