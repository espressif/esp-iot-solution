# ESP Local Controller

面向 IDF [`esp_local_ctrl`](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-reference/protocols/esp_local_ctrl.html) 的**控制器侧**客户端。

| 组件 | 角色 |
|------|------|
| **esp_local_ctrl** | 设备端 **Server** |
| **protocomm_ext** | 控制器 **管道**（传输 + Sec0/1/2） |
| **esp_local_controller**（本组件） | 控制器 **业务**（`control` protobuf） |

## 能力矩阵

| 传输 | Sec0 | Sec1 | Sec2 |
|------|:----:|:----:|:----:|
| HTTP | ✓ | ✓ | ✓ |
| BLE（NimBLE central） | ✓ | ✓ | ✓ |
| UART Console | ✓（调试） | ✓ | ✓ |

## 典型流程

推荐按对端 version JSON 运行时识别安全等级：

```c
esp_local_controller_t *ctrl = esp_local_controller_create(pc);
esp_local_controller_fetch_version(ctrl, NULL);
/* protocomm_ext_set_security() 按 sec_ver 配置 PoP / Sec2 */
esp_local_controller_establish_security(ctrl);
esp_local_controller_get_property_count(ctrl, &count);
esp_local_controller_stop_session(ctrl);
esp_local_controller_delete(ctrl);
```

若对端安全方案**已知**，可先 `protocomm_ext_set_security()`，再调用
`esp_local_controller_start_session()`（内部为 fetch_version + establish_security）。

**线程安全：** 同一实例的 API 非线程安全，请在单任务中串行调用。

## 依赖

本组件依赖同仓库中的 **`protocomm_ext`**（`components/protocomm_ext`，
`idf_component.yml` 中通过 `override_path` 指向）。

```yaml
dependencies:
  espressif/esp_local_controller: "^1.0.0"
```

两者均发布到 Component Registry 后，可按清单中的版本约束解析，无需本地 override。

## 测试 / 示例

- 测试：`components/esp_local_controller/test_apps`
- 示例：`examples/esp_local_controller`（HTTP / BLE / Console）

**支持目标：** ESP32 / ESP32-S3 / ESP32-C2 / ESP32-C3 / ESP32-C5 / ESP32-C6 / ESP32-C61 / ESP32-H2 / ESP32-P4

## 许可证

Apache-2.0，见 `license.txt`。
