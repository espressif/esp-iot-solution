# ESP Board Manager Adater基础示例

本示例演示如何使用 ESP Board Manager Adater 获取板级信息和设备句柄。

## 示例功能

1. 初始化 ESP Board Manager Adater
2. 获取板级信息，包括：
   - 音频播放和录制设备
   - LCD 面板句柄（如果可用）
   - SD 卡句柄（如果可用）
   - 板级音频配置（采样率、位数、通道数、麦克风布局）
3. 演示 LVGL UI 创建，包含一个简单的计数器按钮示例
4. 将所有信息打印到控制台

## 使用步骤

### 1. 设置目标芯片

为您的板子设置目标芯片：

```bash
idf.py set-target esp32s3
```

### 2. 设置环境变量

设置 `IDF_EXTRA_ACTIONS_PATH` 环境变量，指向 esp_board_manager 目录：

```bash
export IDF_EXTRA_ACTIONS_PATH=managed_components/espressif__esp_board_manager
```

**注意**：此路径应相对于项目根目录。

### 3. 生成板级管理器配置

在构建之前，需要为目标板生成板级管理器配置。

首先，查看支持的板子列表：

```bash
idf.py gen-bmgr-config -l
```

然后，为您的板子生成配置（将 `esp32_s3_korvo2_v3` 替换为您的板子名称）：

```bash
idf.py gen-bmgr-config -b esp32_s3_korvo2_v3
```

**示例**：对于 ESP32-S3-Korvo-2 V3 板子：
```bash
idf.py gen-bmgr-config -b esp32_s3_korvo2_v3
```

此命令将在 `components/gen_bmgr_codes/` 目录中生成必要的板级配置文件。

## 如何构建和运行

1. 构建项目：
   ```bash
   idf.py build
   ```

2. 烧录并监控：
   ```bash
   idf.py flash monitor
   ```

## 预期输出

```
I (xxx) BSP_MANAGER_EXAMPLE: ESP BSP Manager Example
I (xxx) ESP_BOARD_MANAGER_ADAPTER: Playback device configured with volume: 30
I (xxx) ESP_BOARD_MANAGER_ADAPTER: Recording device configured with gain: 32.0
I (xxx) ESP_BOARD_MANAGER_ADAPTER: Board: esp32s3_korvo2_v3
I (xxx) ESP_BOARD_MANAGER_ADAPTER: Mic layout: RMNM
I (xxx) ESP_BOARD_MANAGER_ADAPTER: Sample rate: 16000 Hz
I (xxx) ESP_BOARD_MANAGER_ADAPTER: Sample bits: 32
I (xxx) ESP_BOARD_MANAGER_ADAPTER: Channels: 2
I (xxx) BSP_MANAGER_EXAMPLE: === BSP Manager Information ===
I (xxx) BSP_MANAGER_EXAMPLE: Play device: 0x3f...
I (xxx) BSP_MANAGER_EXAMPLE: Record device: 0x3f...
...
```

## 注意事项

- **板级配置**：在构建之前，请确保已使用 `idf.py gen-bmgr-config` 生成板级配置
- **环境变量**：必须正确设置 `IDF_EXTRA_ACTIONS_PATH`，板级管理器命令才能正常工作
- **支持的板子**：使用 `idf.py gen-bmgr-config -l` 查看所有可用的板级配置
- **设备可用性**：某些设备（LCD、SD 卡、触摸屏）可能并非在所有板子上都可用，这是正常的
- **BSP Manager**：BSP Manager 会根据您的配置自动初始化板级管理器和所有设备
- **LVGL**：示例包含一个简单的 LVGL UI 演示，带有计数器按钮。请确保在配置中启用了 LCD 和 LVGL
