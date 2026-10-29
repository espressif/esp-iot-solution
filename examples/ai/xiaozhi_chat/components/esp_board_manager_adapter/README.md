# ESP Board Manager Adapter

ESP BSP Manager 是一个基于 **esp_board_manager** 构建的高级封装组件，用于简化板级支持包（BSP）设备初始化和管理。它提供了统一的接口来管理音频设备、LCD 显示、触摸屏、SD 卡等板级外设，并集成了 LVGL 图形库支持。

## 功能特性

- **音频设备管理**：支持音频播放（DAC）和录制（ADC）设备的初始化
- **LCD 显示管理**：支持 LCD 面板初始化和背光控制
- **触摸屏支持**：支持触摸输入设备的初始化
- **LVGL 集成**：可选的 LVGL 图形库初始化支持
- **SD 卡支持**：支持 SD 卡文件系统初始化
- **配置驱动**：根据 sdkconfig 和用户配置自动启用/禁用功能模块

## 快速开始

### 1. 添加依赖

在 `idf_component.yml` 中添加：

```yaml
dependencies:
  espressif/esp_board_manager_adapter: "*"
```

### 2. 基本使用

```c
#include "esp_board_manager_adapter.h"

void app_main(void)
{
    esp_board_manager_adapter_info_t bsp_info;
    esp_board_manager_adapter_config_t config = ESP_BOARD_MANAGER_ADAPTER_CONFIG_DEFAULT();
    
    // 初始化 BSP Manager
    esp_err_t ret = esp_board_manager_adapter_init(&config, &bsp_info);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize BSP manager");
        return;
    }
    
    // 使用设备句柄
    if (bsp_info.play_dev) {
        // 使用音频播放设备
    }
    
    if (bsp_info.lcd_panel) {
        // 使用 LCD 面板
    }
}
```

### 3. 配置选项

通过 `esp_board_manager_adapter_config_t` 结构体控制要启用的功能：

```c
esp_board_manager_adapter_config_t config = {
    .enable_lvgl          = true,   // 启用 LVGL
    .enable_lcd_backlight = true,   // 启用 LCD 背光控制
    .enable_lcd           = true,   // 启用 LCD 显示
    .enable_touch         = true,   // 启用触摸屏
    .enable_audio_adc     = true,   // 启用音频录制
    .enable_audio_dac     = true,   // 启用音频播放
    .enable_sdcard        = false,  // 禁用 SD 卡
};
```

## API 参考

### 主要函数

- `esp_board_manager_adapter_init()` - 初始化 BSP Manager 并获取设备句柄
- `esp_board_manager_adapter_set_lcd_backlight()` - 设置 LCD 背光亮度（0-100%）

### 数据结构

- `esp_board_manager_adapter_config_t` - BSP Manager 配置结构体
- `esp_board_manager_adapter_info_t` - BSP Manager 信息结构体，包含所有设备句柄

## 架构说明

ESP BSP Manager 基于 **esp_board_manager** 构建，通过调用 `esp_board_manager` 的接口来获取和管理板级设备。它在此基础上提供了：

- 更高层次的抽象接口
- 统一的设备句柄管理
- 自动化的 LVGL 初始化
- 简化的配置方式

## 依赖组件

- **`espressif/esp_board_manager`** - 核心依赖，提供板级设备管理功能（必需）
- `espressif/esp_lcd_touch` - 触摸屏驱动
- `espressif/esp_lvgl_port` - LVGL 图形库端口

## 示例

完整示例请参考 `example/basic` 目录。

## 注意事项

1. **基于 esp_board_manager**：本组件依赖于 `esp_board_manager` 来管理底层设备。确保已正确配置 `esp_board_manager` 并生成相应的板级配置文件。

2. 功能模块的启用需要同时满足两个条件：
   - sdkconfig 中对应的配置项已启用（如 `CONFIG_ESP_BOARD_DEV_AUDIO_CODEC_SUPPORT`）
   - `esp_board_manager_adapter_config_t` 中对应的标志为 `true`

3. 如果某个设备不可用，对应的句柄将被设置为 `NULL`，使用前请检查。

4. LVGL 初始化需要 LCD 支持，确保 `enable_lcd` 和 `enable_lvgl` 都已启用。
