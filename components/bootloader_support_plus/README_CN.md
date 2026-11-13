# bootloader support plus

[![Component Registry](https://components.espressif.com/components/espressif/bootloader_support_plus/badge.svg)](https://components.espressif.com/components/espressif/bootloader_support_plus)

  * [English Version](https://github.com/espressif/esp-iot-solution/blob/master/components/bootloader_support_plus/README.md)

## 概述

bootloader support plus 是乐鑫基于 [ESP-IDF](https://github.com/espressif/esp-idf) 的 [custom_bootloader](https://github.com/espressif/esp-idf/tree/master/examples/custom_bootloader) 推出的增强版 bootloader。它的主要功能是：

- 支持在 bootloader 阶段对`压缩`固件进行`解压缩`，并加载解压后的固件。
- 支持在 bootloader 阶段对`差分压缩`固件进行`解压缩`和`差分解码`，并加载还原后的固件。
- 可直接使用 ESP-IDF 中原生的 OTA 接口，例如 [esp_ota](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/ota.html#api-reference)、[esp_https_ota](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/esp_https_ota.html#api-reference)。

下表总结了适配 `bootloader support plus` 的乐鑫芯片以及其对应的 ESP-IDF 版本：

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
| ESP32-P4 | N/A | N/A | N/A | v5.5.5+，rev ≥ 3 |
| ESP32-S31 | N/A | N/A | N/A | v6.1+ |

## 压缩更新

### 压缩更新的压缩率

| Chip     | Demo            | Original firmware size(Bytes) | Compressed firmware size(Bytes) | Compression ratio(%) |
| -------- | --------------- | ----------------------------- | ------------------------------- | -------------------- |
| ESP32-C3 | hello_world     | 156469                        | 75140                           | 51.98%               |
| ESP32-C3 | esp_http_client | 954000                        | 502368                          | 47.34%               |
| ESP32-C3 | wifi_prov_mgr   | 1026928                       | 512324                          | 50.11%               |

上述示例都是在 ESP-IDF Tag/v5.0 上编译的。

其中压缩率是指 `(Original_firmware_size - Compressed_firmware_size) / Original_firmware_size`，因此压缩率越大，生成的压缩固件体积越小。大部分情况下，压缩率在 40%～60%。因此，存放压缩固件的分区大小可以预留为正常固件分区的约 60%。

### 压缩更新的优势

- 节省硬件 flash 空间。压缩后的固件体积更小，因此需要的存储空间更小。例如原始固件大小为 1MB，如果使用全量更新，则必须使用两个 1MB 大小的分区。此时，你需要使用一个 4MB 大小的 flash。使用压缩 OTA 时，您可以设置一个大小为 1MB 的分区和一个大小为 640KB 的分区。现在您可以使用 2MB 大小的 flash 来完成您的项目。

    ```
        ****************               ****************
        * ota_0 (1M）  *               * ota_0 (1M)   *
        ****************               ****************
        * ota_1 (1M)   *               * ota_1 (640KB)*
        ****************               ****************
           (全量更新)                       (压缩更新)
    ```

- 节省传输时间，提高更新成功率。部署在带宽受限、网络环境差的设备上，可能因设备掉线、传输的固件数据大导致更新成功率低、传输时间长。压缩更新缩短传输固件所需的时间，提高更新成功率。
- 节省传输流量。压缩后的固件体积更小，因此传输需要的流量更小。对流量计费的设备将节省维护成本。
- 节省 OTA 功耗。测试表明，一次 OTA 过程中，传输固件数据所消耗的能源超过总能耗的 80%，因此传输的数据量越小，越节省功耗。
- 不需要新增任何 API。该方案支持 ESP-IDF 中原生的 OTA 接口，如 [esp_ota](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/ota.html#api-reference)、[esp_https_ota](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/esp_https_ota.html#api-reference)。
- 兼容 Secure Boot 和 flash 加密方案。

### 如何生成压缩固件

通过 [cmake_utilities](https://components.espressif.com/components/espressif/cmake_utilities) 组件可以制作压缩固件。添加该组件后，可运行：

```
idf.py gen_compressed_ota
```

在工程目录中运行上述命令后，会生成 `build/custom_ota_binaries/app_name.bin.xz.packed`。其中 `app_name.bin.xz.packed` 就是要传输的压缩固件。更多说明见 [Gen Compressed OTA](https://github.com/espressif/esp-iot-solution/blob/master/tools/cmake_utilities/docs/gen_compressed_ota.md)。

## 差分更新

差分更新建立在压缩更新之上，用 xz 压缩的 ddelta 补丁代替整包压缩固件。主机侧用设备当前运行的固件（base）和新固件生成补丁；设备下载补丁后，在 bootloader 阶段流式解压并打补丁，写出完整新固件。

为保证 OTA 可回滚，分区表至少要有三个 `app` 分区：前两个轮流存放正在运行的固件和打补丁得到的新固件，最后一个分区存放差分压缩包。下载仍使用 ESP-IDF 原生 OTA 接口（如 `esp_ota`、`esp_https_ota`），目标分区固定为最后一个 `app` 分区。

### 差分更新的优势

- 补丁更小。相对于应用层差分方案，同等变更下补丁体积通常约为其 1/3；若仅改动少量代码，补丁可小到约 300～400 字节，更适合 BLE、Zigbee 等低带宽链路。
- 节省传输时间、流量与功耗。传输对象是补丁而不是整包或整包压缩固件，带宽受限或按流量计费的场景收益更明显。
- 无需新增应用层 OTA API。差分解压与打补丁全部在 bootloader 中完成，应用继续使用原有 `esp_ota` / `esp_https_ota` 下载流程。
- 差分引擎只保留一份。应用层方案通常要在两份 app 里各带一套差分引擎；本方案引擎只在 bootloader 中，两份 app 都不必再编入差分逻辑，可减小固件体积。
- 保留双分区回滚。三个 `app` 分区下，打补丁失败或新固件校验失败时，可回到原先正在运行的槽，不丢掉可用固件。

### 分区表与配置

[simple_ota_example](https://github.com/espressif/esp-iot-solution/tree/master/examples/ota/simple_ota_example) 中的 [partition_for_diff_ota.csv](../../examples/ota/simple_ota_example/partition_for_diff_ota.csv) 按 4MB flash 给出默认布局：`ota_0` 和 `ota_1` 各 1408KB，`ota_2` 为 640KB。1.4MiB 不是 64KB 对齐的整数倍，1408KB 是不超过 1.4MiB 的最大对齐大小；640KB 可放下约 600KB 的压缩固件或差分压缩包。

```
# Name,   Type, SubType, Offset,   Size, Flags
phy_init, data, phy,       ,        4k
nvs,      data, nvs,       ,        28k
otadata,  data, ota,       ,        8k
ota_0,    app,  ota_0,     ,        1408k,
ota_1,    app,  ota_1,     ,        1408k,
ota_2,    app,  ota_2,     ,        640k,
```

在 `Bootloader config (Custom)` 中同时打开 `Enable compressed OTA support` 和 `Enable ddelta patch apply`。该选项不能与 legacy esp-bootloader-plus v2 同时启用。

制作差分压缩包需要 [cmake_utilities](https://components.espressif.com/components/espressif/cmake_utilities) >=1.4.1（`gen_diff_ota` 与 `esp_ddelta`）。设备重启后，bootloader 先按 v3 头里的 `base_len` 和 `base_crc32` 核对整份旧固件，不一致则直接回到原运行槽，不会擦除另一份固件；通过后再流式解压并做 ddelta，把新固件写到「不是旧固件、也不是最后一分区」的那个槽。把 `Skip full base image validation in ddelta OTA` 设为 n 后，打补丁前还会用 `esp_image_verify()` 再查一遍 base 的镜像结构和签名，文件 CRC 仍然会做。`Skip full new image validation in ddelta OTA` 默认 y：补丁写完就更新 otadata，新固件在随后的加载阶段校验，失败则启动另一份可用固件。把它设为 n 后，切换 otadata 前会用 `esp_image_verify()` 校验重建出的新固件，失败则把 otadata 恢复到原来的运行分区。开启 Flash 加密和 Secure Boot 时，这是额外一次验签，会拉长这次启动。

纯压缩更新不打开 ddelta 选项，行为与原来一致。

### 如何生成补丁文件

制作补丁需要 [cmake_utilities](https://components.espressif.com/components/espressif/cmake_utilities) >=1.4.1，并在 ESP-IDF 使用的 Python 环境中安装 `esp_ddelta`。建议用 `version.txt` 管理每次发布的版本，并保存对应的固件文件。制作补丁时，`-b`（或 `gen_diff_ota` 使用的当前工程固件）必须是设备上正在运行的那一份，不能用之后的发布版代替。

在工程的 `CMakeLists.txt` 中 `include(gen_diff_ota)` 后，可运行：

```
idf.py -DDIFF_OTA_NEW_FILE=backup/app_name.bin gen_diff_ota
```

该命令会先编译工程，把 `build/app_name.bin` 作为 base，相对 `DIFF_OTA_NEW_FILE` 指向的新固件打出补丁，输出到 `build/custom_ota_binaries/patch.xz.packed`。请先把新固件拷贝到旁路目录：命令会覆盖 `build/app_name.bin`。下发时使用 `patch.xz.packed`，不要使用未打包的 `patch.xz`。

若两份固件已经存在，也可直接调用脚本（需带 `--add_app_header`，以便走原生 OTA 接口）：

```
python3 /path/to/scripts/gen_custom_ota.py \
    -hv v3 -c xz -d ddelta -b current_app.bin -i new_app.bin --add_app_header
```

若补丁短于 1024 字节且使用 `esp_https_ota`，还需开启 `CMake Utilities -> Pad packed OTA image to 1024 bytes`，或在脚本中增加 `--pad_https_ota`。更多说明见 [gen_compressed_ota.md](https://github.com/espressif/esp-iot-solution/blob/master/tools/cmake_utilities/docs/gen_compressed_ota.md) 与 [esp_ddelta/README_CN.md](https://github.com/espressif/esp-iot-solution/blob/master/tools/cmake_utilities/scripts/esp_ddelta/README_CN.md)。

## 如何使用 bootloader support plus

为了使用压缩更新，您需要进行以下更改：

1. 请参阅 [bootloader override example](https://github.com/espressif/esp-idf/tree/master/examples/custom_bootloader/bootloader_override) 在工程中添加 `bootloader_components` 目录。

2. 在 `bootloader_components` 目录中创建 idf_component.yml 文件。

      ```yaml
      dependencies:
        espressif/bootloader_support_plus: ">=0.1.0"
      ```

3. 在 `bootloader_components/main/bootloader_start.c` 中添加头文件 `bootloader_custom_ota.h`。

   ```
   #if defined(CONFIG_BOOTLOADER_COMPRESSED_ENABLED)
   #include "bootloader_custom_ota.h"
   #endif
   ```

4. 在 `bootloader_components/main/bootloader_start.c` 中调用 `bootloader_custom_ota_main()`：

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

5. 添加 `bootloader_support_plus` 组件。你可以通过 `idf.py add-dependency` 命令添加该组件，如：

   ```shell
   idf.py add-dependency "espressif/bootloader_support_plus=*"
   ```

   可选地，你也可以在当前工程的 main 目录中创建一个 idf_component.yml 文件，然后添加下面的内容：

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

7. 在工程目录中通过 `idf.py menuconfig -> Bootloader config (Custom) -> Enable compressed OTA support` 使能压缩更新的支持。若使用差分更新，请再打开 `Enable ddelta patch apply`，并使用至少三个 `app` 分区，详见上文「差分更新」。

## 示例

在 esp-iot-solution 中有一个参考示例：[ota/simple_ota_example](https://github.com/espressif/esp-iot-solution/tree/master/examples/ota/simple_ota_example)。

您可以通过以下命令从此示例创建项目：

```
idf.py create-project-from-example "espressif/bootloader_support_plus=*:simple_ota_example"
```

## 注意

1. 当使用压缩更新时，双分区情况下不支持版本回滚。当 flash 中仅有一个存放 app 的分区和一个存放压缩固件的分区时，压缩更新无法提供版本回滚，请在下发压缩固件前验证其可用性和正确性。
2. 当启用 Secure Boot 和 flash 加密时，存放压缩固件（或差分压缩包）的分区大小须大于「压缩包大小 + 8KB」。
3. 当使用差分更新时，必须进行版本管理，保存每次下发的固件，作为下次制作补丁时的 base。
4. 在启用差分更新后，压缩更新仍默认下载到最后一个分区，并在 bootloader 中默认解压到 ota_0（ota_index = 0），和原来压缩更新一致。

### Q&A

Q1. 在使用包管理器的时候遇到了如下问题：

```
Executing action: create-project-from-example
CMakeLists.txt not found in project directory /home/username             
```

A1. 这是旧版本包管理器中的兼容性问题，请在 ESP-IDF 开发环境中运行 `pip install -U idf-component-manager` 更新组件管理器。

Q2. 已经使用 esp bootloader plus 方案的设备，是否可以使用 bootloader support plus 方案？

A2. 可以。您可以使用脚本工具 [gen_custom_ota.py](https://github.com/espressif/esp-iot-solution/blob/master/tools/cmake_utilities/scripts/gen_custom_ota.py) 执行下述命令，生成适用于 esp bootloader plus 方案的压缩固件：

```
python3 gen_custom_ota.py -hv v2 -i simple_ota.bin
```

另外，因为这两个方案采取不同的文件格式以及不同的触发更新机制，请确保已经使用 esp bootloader plus 方案的设备继续使用包含 SubType 为 0x22 的 storage 分区的分区表：

```
# Name,   Type, SubType, Offset,   Size, Flags
phy_init, data, phy, 0xf000,        4k
nvs,      data, nvs,       ,        28k
otadata,  data, ota,       ,        8k
ota_0,    app,  ota_0,     ,        1216k,
storage,  data, 0x22,      ,        640k,
```

然后在 menuconfig 配置菜单中使能 `CONFIG_ENABLE_LEGACY_ESP_BOOTLOADER_PLUS_V2_SUPPORT` 选项。

最后您只需要将压缩固件下载到 storage 分区，然后重启设备即可触发更新。

## 贡献

我们欢迎以错误报告、功能请求和拉取请求的形式对此项目做出贡献。

您可以使用 Github Issues 提交问题报告和功能请求：https://github.com/espressif/esp-iot-solution/issues。在打开新的 issue 之前，请检查该问题是否已经被上报。

您可以发起一个 Pull Request 来提交您的代码。提交代码应遵循 ESP-IDF 项目的[贡献指南](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/contribute/index.html)。
