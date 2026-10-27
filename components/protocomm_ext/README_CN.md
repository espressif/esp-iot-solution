# Protocomm 扩展方案

ESP-IDF [`protocomm`](https://docs.espressif.com/projects/esp-idf/en/latest/api-reference/provisioning/protocomm.html) 的**客户端扩展**。

| 组件 | 角色 |
|------|------|
| **protocomm**（ESP-IDF） | 设备端 **Server**：承接配网 / 本地控制会话 |
| **protocomm_ext**（本组件） | 控制器端 **Client**：主动建连、完成安全握手、收发加解密载荷 |

本组件只做**协议管道**，**不**编解码 Wi-Fi 配网或 `esp_local_ctrl` 业务 protobuf。上层应把编好的端点载荷交给 `protocomm_ext_send_data()`（如 `prov-config`、`esp_local_ctrl/control`）。

## 功能

| | Security 0 | Security 1 | Security 2 |
|--|:----------:|:----------:|:----------:|
| HTTP / HTTPS | ✓ | ✓ | ✓ |
| BLE（NimBLE Central） | ✓ | ✓ | ✓ |
| UART Console | ✓ | ✓ | ✓ |

- **Security 0**：明文会话
- **Security 1**：Curve25519 + AES-256-CTR（可选 PoP）
- **Security 2**：SRP-6a（NG_3072 / SHA512）+ AES-256-GCM（用户名 / 密码）
- 可通过 Kconfig 裁剪未使用的安全版本

**支持目标：** ESP32 / ESP32-S3 / ESP32-C2 / ESP32-C3 / ESP32-C5 / ESP32-C6 / ESP32-C61 / ESP32-P4

## 典型流程

```c
#include "protocomm_ext.h"
#include "protocomm_ext_http.h"
#include "protocomm_ext_security.h"

esp_http_client_config_t http_cfg = {
    .url = "http://192.168.4.1:80",
    .timeout_ms = 5000,
};

protocomm_ext_security1_params_t pop = {
    .data = (const uint8_t *)"abcd1234",
    .len = 8,
};

protocomm_ext_config_data_t cfg = {
    .transport_method = PROTOCOMM_EXT_TRANSPORT_METHOD_HTTP,
    .security_method = PROTOCOMM_EXT_SECURITY_METHOD_SECURITY_1,
    .transport_data = &http_cfg,
    .security_data = &pop,
};

protocomm_ext_t *pc = protocomm_ext_new(&cfg);
protocomm_ext_open_session(pc, NULL);

uint8_t *ver = NULL;
size_t ver_len = 0;
protocomm_ext_get_version_capabilities(pc, "proto-ver", &ver, &ver_len);
free(ver);

protocomm_ext_establish_security(pc, "prov-session");

uint8_t *resp = NULL;
size_t resp_len = 0;
protocomm_ext_send_data(pc, "prov-config", req, req_len, &resp, &resp_len);
free(resp);

protocomm_ext_close_session(pc);
protocomm_ext_delete(pc);
```

### 传输说明

- **HTTP**：`transport_data` 传 `esp_http_client_config_t *`；自动保存 SoftAP `protocomm_httpd` 的 session Cookie。
- **BLE**：需开启 `CONFIG_BT_NIMBLE_ENABLED`；通过 `esp_protocomm_ext_nimble_start_scan()` / `open_session(pc, &ble_addr)` 连接。
- **Console**：`transport_data` 传 `protocomm_ext_console_config_t *`，协议与 IDF `protocomm_console` 一致（`<ep> <sid> <hex>\r`）。

### 安全说明

- Sec1：`protocomm_ext_security1_params_t`（PoP），对端不需要 PoP 时可传 `NULL`。
- Sec2：`protocomm_ext_security2_params_t`（username/password），需与设备端 salt/verifier 对应口令一致。

## 添加依赖

```yaml
dependencies:
  espressif/protocomm_ext: "^1.0.0"
```

## 测试

```bash
cd components/protocomm_ext/test_apps
idf.py set-target esp32
idf.py build
pytest --target esp32
```

## 与后续组件的关系

配网 / 本地控制的**业务客户端**不在本组件范围内，将在独立组件中基于本管道实现。

## License

Apache-2.0，见 `license.txt`。
