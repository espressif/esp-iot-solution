# bootloader support plus

[![Component Registry](https://components.espressif.com/components/espressif/bootloader_support_plus/badge.svg)](https://components.espressif.com/components/espressif/bootloader_support_plus)

  * [English Version](https://github.com/espressif/esp-iot-solution/blob/master/components/bootloader_support_plus/README.md)

## 概述
bootloader support plus 是乐鑫基于 [ESP-IDF](https://github.com/espressif/esp-idf) 的 [custom_bootloader](https://github.com/espressif/esp-idf/tree/master/examples/custom_bootloader)  推出的增强版 bootloader，支持在 bootloader 阶段对`压缩`的固件进行 `解压缩`，来升级原有固件。在该方案中，您可以直接使用 ESP-IDF 中原生的 OTA 接口（比如 [esp_ota](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/ota.html#api-reference)、[esp_https_ota](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/esp_https_ota.html#api-reference)）。下表总结了适配 `bootloader support plus` 的乐鑫芯片以及其对应的 ESP-IDF 版本：

| Chip     | ESP-IDF Release/v5.0                                         | ESP-IDF Release/v5.1                                  | ESP-IDF Release/v5.4+                 | ESP-IDF Release/v5.5+                 |
| -------- | ------------------------------------------------------------ | ------------------------------------------------------------ |------------------------------ |----------------------------- |
| ESP32 | Supported | Supported | Supported | Supported |
| ESP32-S3 | N/A | Supported | Supported | Supported |
| ESP32-C2 | Supported | Supported | Supported | Supported |
| ESP32-C3 | Supported | Supported | Supported | Supported |
| ESP32-C5 | N/A | N/A | Supported | Supported |
| ESP32-C6 | N/A | Supported | Supported | Supported |
| ESP32-H2 | N/A | Supported | Supported | Supported |
| ESP32-C61 | N/A | N/A  | N/A | Supported |

## 压缩率

| Chip     | Demo            | Original firmware size(Bytes) | Compressed firmware size(Bytes) | Compression ratio(%) |
| -------- | --------------- | ----------------------------- | ------------------------------- | -------------------- |
| ESP32-C3 | hello_world     | 156469                        | 75140                           | 51.98%               |
| ESP32-C3 | esp_http_client | 954000                        | 502368                          | 47.34%               |
| ESP32-C3 | wifi_prov_mgr   | 1026928                       | 512324                          | 50.11%               |

上述示例都是在 ESP-IDF Tag/v5.0 上编译的。

其中压缩率是指`Original_firmware_size-Compressed_firmware_size)/Original_firmware_size)`，因此压缩率越大代表生成的压缩固件的体积越小。大部分情况下，压缩率在 40%～60%。因此，存放压缩固件的分区的大小可以预留为存放正常的固件分区大小的 60%。

## 优势

- 节省硬件 flash 空间。压缩后的固件体积更小，因此需要的存储空间更小。例如原始固件大小为 1MB，如果使用全量更新，则必须使用两个 1MB 大小的分区。此时，你需要使用一个 4MB 大小的 flash。使用压缩 OTA 时，您可以设置一个大小为 1MB 的分区和一个大小为 640KB 的分区。 现在您可以使用 2MB 大小的 flash 来完成您的项目。

    ```
        ****************               ****************
        * ota_0 (1M）  *               * ota_0 (1M)   *
        ****************               ****************
        * ota_1 (1M)   *               * ota_1 (640KB)*
        ****************               ****************
           (全量更新)                       (压缩更新)
    ```

- 节省传输时间，提高更新成功率。部署在带宽受限，网络环境差的设备，可能因设备掉线、传输的固件数据大导致更新成功率低，传输时间长。压缩更新缩短传输固件所需的时间，提高更新成功率。
- 节省传输流量。压缩后的固件的体积更小，因此传输需要的流量更小。对流量计费的设备将节省维护成本。  
- 节省 OTA 功耗。测试表明，一次 OTA 过程中，传输固件数据所消耗的能源超过总的能源消耗的 80% ，因此传输的数据量越小，越节省功耗。
- 不需要新增任何 API。该方案支持 ESP-IDF 中原生的 OTA 接口，如 [esp_ota](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/ota.html#api-reference)、[esp_https_ota](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/esp_https_ota.html#api-reference) 。
- 兼容 Secure Boot 和 flash 加密方案。

## 如何使用bootloader support plus

为了使用压缩更新，您需要进行以下更改：

1. 请参阅 [bootloader override example](https://github.com/espressif/esp-idf/tree/master/examples/custom_bootloader/bootloader_override) 在工程中添加 `bootloader_components` 目录。

2. 在 `bootloader_components` 目录中创建 idf_component.yml 文件。

      ```yaml
      dependencies:
        espressif/bootloader_support_plus: ">=0.1.0"
      ```

3. 在 `bootloader_components/main/bootloader_start.c` 中添加头文件 `bootloader_custom_ota.h` 。

   ```
   #if defined(CONFIG_BOOTLOADER_COMPRESSED_ENABLED)
   #include "bootloader_custom_ota.h"
   #endif
   ```

4. 在 `bootloader_components/main/bootloader_start.c` 中调用 `bootloader.custom_ota_main（）`:

   ```c
   ...
   bootloader_state_t bs = {0};
   int boot_index = select_partition_number(&bs);
   if (boot_index == INVALID_INDEX) {
       bootloader_reset();
   }
   #if defined(CONFIG_BOOTLOADER_COMPRESSED_ENABLED)
       // 2.1 Call custom OTA routine
       boot_index = bootloader_custom_ota_main(&bs, boot_index);
   #endif
   ```

5. 添加 `bootloader_support_plus` 组件。你可以通过 `idf.py add-dependency`, 命令添加该组件，如：

   ```shell
   idf.py add-dependency "espressif/bootloader_support_plus=*"
   ```

   可选地，你也可以在当前工程的 main 目录中通过创建一个 idf_component.yml 文件，然后添加下面的内容来添加该组件.

   ```yaml
   dependencies:
     espressif/bootloader_support_plus: ">=0.1.0"
   ```

   关于组件管理器的更多说明请参考 [Espressif's documentation](https://docs.espressif.com/projects/idf-component-manager/en/latest/index.html)。

6. 创建支持压缩更新的分区表。默认情况下，分区表中类型为 `app` 的最后一个分区用作存储压缩固件的分区。因为压缩固件的大小将小于原始固件的大小，所以可以将该分区设置得更小。下述为压缩更新中使用到的一个典型的分区表示例：

      ```
      # Name,   Type, SubType, Offset,   Size, Flags
      phy_init, data, phy,       ,        4k
      nvs,      data, nvs,       ,        28k
      otadata,  data, ota,       ,        8k
      ota_0,    app,  ota_0,     ,        1216k,
      ota_1,    app,  ota_1,     ,        640k,
      ```

7. 在工程目录中通过  `idf.py menuconfig -> Bootloader config (Custom) -> Enable compressed OTA support` 使能压缩更新的支持。

## 如何生成压缩固件

通过 [cmake_utilites](https://components.espressif.com/components/espressif/cmake_utilities) 组件可以制作压缩的固件。在添加该组件后，你可以通过运行下述命令来生成压缩的固件：

```
idf.py gen_compressed_ota
```

在工程目录中运行上述命令将在当前目录中生成 `build/custom_ota_binaries/app_name.bin.xz.packed`。其中 `app_name.bin.xz.packed` 文件就是要传输的压缩固件。有关制作压缩固件的更多说明你还可以参考 [Gen Compressed OTA](https://github.com/espressif/esp-iot-solution/blob/master/tools/cmake_utilities/docs/gen_compressed_ota.md) 。

## 示例

在 esp-iot-solution 中有一个参考示例：[ota/simple_ota_example](https://github.com/espressif/esp-iot-solution/tree/master/examples/ota/simple_ota_example) 。

您可以通过以下命令从此示例创建项目：

```
idf.py create-project-from-example "espressif/bootloader_support_plus=*:simple_ota_example"
```

## 注意

1. 当使用压缩更新时，双分区情况下，不支持版本回滚。
   当您的 flash 分区仅有一个存储 app 的分区，和一个存储压缩固件的分区时，压缩更新将无法提供版本回滚的功能，请在下发压缩固件前验证压缩固件的可用性和正确性。
2. 当启用 secure boot 和 flash 加密时，存储压缩固件的分区的大小 > （压缩固件的大小 + 8KB）。

### Q&A

Q1. 在使用包管理器的时候遇到了如下问题：

```
Executing action: create-project-from-example
CMakeLists.txt not found in project directory /home/username             
```

A1. 这是旧版本的包管理器中存在的兼容性问题, 请在您的 ESP-IDF 开发环境中运行 `pip install -U idf-component-manager` 命令来更新包管理器组件。

Q2. 已经使用 esp bootloader plus 方案的设备，是否可以使用 bootloader support plus 方案？

A2. 可以。您可以使用脚本工具 [gen_custom_ota.py](https://github.com/espressif/esp-iot-solution/blob/master/tools/cmake_utilities/scripts/gen_custom_ota.py) 来执行下述命令生成适用于esp bootloader plus 方案的压缩固件：

```
python3 gen_custom_ota.py -hv v2 -i simple_ota.bin
```

另外，因为这两个方案采取不同的文件格式以及不同的触发更新的机制，请确保已经使用 esp bootloader plus 方案的设备继续使用包含 SubType 为 0x22 的 storage 分区的分区表：

```
# Name,   Type, SubType, Offset,   Size, Flags
phy_init, data, phy, 0xf000,        4k
nvs,      data, nvs,       ,        28k
otadata,  data, ota,       ,        8k
ota_0,    app,  ota_0,     ,        1216k,
storage,  data, 0x22,      ,        640k,
```

然后在 menuconfig 配置菜单中使能 `CONDIF_ENABLE_LEGACY_ESP_BOOTLOADER_PLUS_V2_SUPPORT` 选项。

最后您只需要将压缩固件下载到 storage 分区，然后重启设备即可触发更新。

## 贡献
我们欢迎以错误报告、功能请求和拉取请求的形式对此项目做出贡献。

您可以使用 Github Issues 提交问题报告和功能请求：https://github.com/espressif/esp-iot-solution/issues。 在打开新的 issue 之前，请检查该问题是否已经被上报。

您可以发起一个 Pull Request 来提交您的代码。提交代码应遵循 ESP-IDF 项目的[贡献指南](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/contribute/index.html)。 
