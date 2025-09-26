安全启动
*****************

安全启动是 ESP32 系列芯片的关键安全防护机制，通过在启动过程和 OTA 更新时对软件实施数字签名验证，确保设备只能执行经过授权的固件。该机制建立了从引导加载程序到应用程序的完整信任链，为设备提供完备的代码完整性保护。

安全启动的核心特点：

- **代码完整性保护**：通过密码学验证确保固件代码未被篡改或替换，防止运行未经授权的恶意程序
- **完整信任链保护**：建立从引导加载程序到应用程序的端到端验证机制，确保启动流程中的每个环节都经过验证
- **支持多种签名算法**：可选择 RSA-PSS 或 ECDSA 作为签名算法，并通过硬件加速确保高效验证
- **硬件安全存储**：将签名密钥摘要存储在受保护的 eFuse 中，提供严格的硬件级读写保护机制

更多详细信息请参考：`ESP-IDF 安全启动 v2 文档 <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/security/secure-boot-v2.html>`_

安全启动与 Flash 加密
~~~~~~~~~~~~~~~~~~~~~~~

安全启动与 Flash 加密是互补的安全功能：

- **安全启动**：保证代码完整性，防止运行未授权固件
- **Flash 加密**：保护数据机密性，防止读取 Flash 代码和数据

建议在生产环境中同时启用两项功能以获得最佳安全保护。详情参考：`安全启动与 Flash 加密 <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/security/secure-boot-v2.html#secure-boot-flash-encryption>`_

安全启动信任链
~~~~~~~~~~~~~~~~

安全启动会验证以下软件组件：

- 一级引导加载程序（ROM 代码）作为芯片固化代码，无法被更改或破坏，作为信任链的根基
- 一级引导加载程序通过数字签名验证二级引导加载程序（bootloader）的完整性和真实性
- 二级引导加载程序通过数字签名验证应用程序（app）二进制文件的完整性和真实性
- 应用程序在 OTA 升级过程中验证新固件的数字签名，确保更新的安全性

详细验证过程：`安全启动 v2 过程 <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/security/secure-boot-v2.html#secure-boot-v2-process>`_

安全启动功能支持列表
~~~~~~~~~~~~~~~~~~~~

.. list-table:: 安全启动功能支持列表
    :header-rows: 1

    * - 芯片型号
      - 支持算法（曲线/位数）
      - 签名验证时间（CPU频率）
      - 最大公钥数量
      - 备注/特殊说明
    * - ESP32 （ECO V3+）
      - RSA-3072（RSA-PSS，Secure Boot V2）<br>AES（V1）
      - 未明确给出
      - 1
      - 仅支持1把公钥，Secure Boot V2 需ECO V3及以上
    * - ESP32-S2
      - RSA-3072（RSA-PSS）
      - 未明确给出
      - 3
      - 最多3把公钥，可吊销
    * - ESP32-S3
      - RSA-3072（RSA-PSS）
      - 未明确给出
      - 3
      - 最多3把公钥，可吊销
    * - ESP32-C2
      - ECDSA-192（NISTP192）<br>ECDSA-256（NISTP256，推荐）
      - 未明确给出
      - 1
      - 仅支持ECDSA，且仅1把公钥 `ESP32-C2 Kconfig Reference <https://docs.espressif.com/projects/esp-idf/en/latest/esp32c2/api-reference/kconfig-reference.html#config-secure-boot-ecdsa-key-len-size>`_
    * - ESP32-C3 （v0.3及以上）
      - RSA-3072（RSA-PSS）
      - 未明确给出
      - 3
      - 最多3把公钥，可吊销
    * - ESP32-C5
      - RSA-3072<br>ECDSA-256（P-256）<br>ECDSA-384（P-384）
      - RSA-3072: 12.1 ms<br>ECDSA-256: 5.6 ms<br>ECDSA-384: 20.6 ms（48 MHz）
      - 3
      - 仅可选一种算法 `ESP32-C5 Secure Boot v2 <https://docs.espressif.com/projects/esp-idf/en/latest/esp32c5/security/secure-boot-v2.html#secure-boot-v2-scheme-selection>`_
    * - ESP32-C6
      - RSA-3072<br>ECDSA-256（P-256）
      - RSA-3072: 10.2 ms<br>ECDSA-256: 83.9 ms（40 MHz）
      - 3
      - 仅可选一种算法 `ESP32-C6 Secure Boot v2 <https://docs.espressif.com/projects/esp-idf/en/latest/esp32c6/security/secure-boot-v2.html#secure-boot-v2-scheme-selection>`_
    * - ESP32-H2
      - RSA-3072<br>ECDSA-256（P-256）
      - RSA-3072: 18.3 ms<br>ECDSA-256: 76.2 ms（32 MHz）
      - 3
      - 仅可选一种算法 `ESP32-H2 Secure Boot v2 <https://docs.espressif.com/projects/esp-idf/en/latest/esp32h2/security/secure-boot-v2.html#secure-boot-v2-scheme-selection>`_
    * - ESP32-P4
      - RSA-3072<br>ECDSA-256（P-256）
      - RSA-3072: 14.8 ms<br>ECDSA-256: 61.1 ms（40 MHz）
      - 3
      - 仅可选一种算法 `ESP32-P4 Secure Boot v2 <https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/security/secure-boot-v2.html#secure-boot-v2-scheme-selection>`_

**说明：**
- “最大公钥数量”指可在 eFuse 中存储的公钥数量，部分芯片支持密钥吊销。
- “签名验证时间”指 ROM 引导加载器验证 bootloader 镜像签名所需时间，实际启动时间可能略有不同。
- 若需更详细的配置和使用建议，请查阅对应芯片的官方文档。

签名与密钥管理
~~~~~~~~~~~~~~~

在信息安全中，数字签名（Digital Signature）是一种基于非对称加密的完整性验证机制。它的核心目的是：证明数据的来源真实性（Authenticity），以及确保数据在传输或存储过程中未被篡改（Integrity）。

签名过程使用的是一对密钥：

- 私钥（Private Key）：用于对数据进行签名，私钥必须严格保密，保存在安全的环境中（本地固件开发环境或远程签名服务器）。
- 公钥（Public Key）：用于验证签名的合法性，公钥可以安全地分发给任何需要验证签名的实体。

安全启动 v2 支持多种非对称加密算法，具体取决于芯片型号：

- **RSA-PSS**：RSA-3072 密钥，适用于快速启动的场景
- **ECDSA**：支持 P-256 和 P-192 曲线，密钥长度更短但验证时间较长（受限于当前 ECDSA 硬件加速能力）

签名块格式
~~~~~~~~~~~~~

签名块（Signature Block）是 ESP32-S3 Secure Boot v2 中用于验证镜像完整性和来源可信性的关键结构。它是附加在 bootloader 或应用程序镜像末尾的一个固定格式数据块，包含签名信息、公钥以及辅助验证字段。

- 签名块主要包含镜像的 SHA-256 哈希值、公钥、签名结果等
- 签名块本身占据 1216 字节，位于镜像末尾的 4KB 对齐边界，且具有 CRC32 校验
- 每个镜像可附加最多 3 个签名块，用于支持多密钥验证与撤销机制

签名块详细定义：`签名块格式 <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/security/secure-boot-v2.html#signature-block-format>`_

启用硬件安全启动
~~~~~~~~~~~~~~~~~~~~~~~

**方法一：通过配置项启用（menuconfig）**：

在 ESP-IDF 中启用 Secure Boot v2 最简单的方式是通过 ``menuconfig`` 配置项。在构建 ``bootloader`` 时，构建系统会在编译 ``bootloader`` 和 ``app`` 镜像时自动插入签名流程。首次启动时，bootloader 会自动烧写 eFuse 以启用 Secure Boot，并撤销未使用的密钥槽。这种方式适合开发阶段，流程自动化、集成度高，但由于密钥撤销不可逆，量产时需谨慎使用。

1. 打开项目配置菜单 ``Security features`` 启用 ``Enable hardware Secure Boot in bootloader``
2. 选择 ``Secure Boot v2`` 签名方案，根据不同芯片，可选的签名算法为``RSA`` 或 ``ECDSA``
3. 指定签名密钥文件路径，密钥文件（例如 RSA 3072）可通过 ``idf.py secure-generate-signing-key`` 或 ``openssl genrsa`` 指令生成
4. 使用 ``idf.py build`` 指令将直接编译、对齐填充、生成附带签名块的 ``app`` 镜像
5. 使用 ``idf.py bootloader`` 指令生成启用了安全启动且已经附带签名块的 bootloader 镜像
6. 为了安全起见，默认情况下启用了安全启动的 bootloader 需要使用 ``esptool.py write_flash`` 指令单独烧写，``idf.py flash`` 只能烧写应用程序和分区表等分区
7. 重启设备，bootloader 会在首次运行时自动烧写 eFuse 以启用 Secure Boot，并将计算出签名块的公钥摘要写入 eFuse，并撤销未使用的密钥槽
8. 之后的启动过程中，eFuse 中的公钥摘要将用于验证 bootloader 和 app 镜像的签名，确保设备仅运行对应私钥签名后的固件

**注意事项**：启用安全启动后，bootloader 镜像大小将增加，可能需要重新调整分区表大小。私钥必须妥善保管，如果私钥泄露，攻击者可以生成有效签名的恶意固件，如果密钥丢失，设备将无法更新固件。

**方法二：外部工具启用**：

另一种方式是使用 ``espefuse.py`` 工具在烧录前手动配置 eFuse，包括写入公钥摘要、设置密钥用途、启用 ``Secure Boot`` 标志位等。这种方式不会触发 ``bootloader`` 的自动密钥撤销逻辑，因此可以保留未使用的密钥槽，便于未来密钥轮换或 OTA 签名更新。它适合量产环境，安全性高、控制精细，但操作复杂，需要严格的密钥管理和烧录流程。此外，你可以使用 `远程签名 <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/security/secure-boot-v2.html#remote-signing-of-images>`_ 或 `外部 HSM <https://docs.espressif.com/projects/esptool/en/latest/esp32c2/espsecure/index.html#remote-signing-using-an-external-hsm>`_ 来生成签名块，进一步提升私钥安全性

1. 生成私钥（可在本地或远程环境中生成）：创建用于签名固件的私钥
2. 生成公钥摘要：对公钥进行 SHA-256 哈希计算
3. 烧录摘要：将公钥摘要写入 eFuse 特定区域
4. 启用安全启动：置位相关 eFuse 标志位
5. 撤销未用密钥槽：防止未授权密钥被添加
6. 烧录安全配置：设置其他安全相关的 eFuse 位
7. 配置项目：禁用自动签名以使用外部签名工具
8. 签名镜像：对 bootloader 和 app 进行签名
9. 烧录镜像：将签名后的固件写入设备
10. 启用安全下载：最终锁定所有安全配置

详细配置方法和指令详见：`外部启用 v2 <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/security/security-features-enablement-workflows.html#enable-secure-boot-v2-externally>`_

密钥撤销
~~~~~~~~~~~

对于已烧录多个公钥的芯片，可以通过撤销密钥防止已泄露或不再使用的密钥被利用。撤销密钥后，使用该密钥签名的固件将无法通过验证，从而保护设备免受潜在攻击。

**实施前提**：

密钥撤销功能需要满足以下前提条件：

1. 芯片必须支持多个公钥（>1）且具备密钥撤销功能
2. 设备出厂时已在 eFuse 中烧录至少两个公钥摘要（如 key #0 和 key #1）
3. 二级引导加载程序（bootloader）使用多重签名机制，已由 key #0 和 key #1 对应的私钥进行签名
4. 应用程序（app）仅使用其中一个密钥（如 key #0）进行签名

更多关于多重签名的信息，请参考：`多重签名 <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/security/secure-boot-v2.html#multiple-keys>`_

**实施步骤**：

1. 使用新密钥（如 key #1）签名的 OTA 固件通过当前应用验证后写入备用分区
2. 新应用启动后验证二级引导加载程序（bootloader）签名正常（确认 bootloader key #1 可用）
3. 调用 `esp_ota_revoke_secure_boot_public_key（） <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/ota.html#_CPPv437esp_ota_revoke_secure_boot_public_key38esp_ota_secure_boot_public_key_index_t>`__ 撤销旧密钥（key #0 撤销）
4. 旧密钥（key #0）被撤销后，使用该密钥签名的固件将无法通过验证
5. 二级引导加载程序（bootloader）和应用程序（app）均使用新密钥（key #1）签名验证

密钥撤销详情：`密钥撤销 <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/security/secure-boot-v2.html#key-revocation>`_

启用后的限制
~~~~~~~~~~~~~

**安全限制**：

- 启用硬件安全启动后，无禁用方法
- 更新的引导加载程序或应用程序必须使用匹配的密钥签名
- 禁用 USB-OTG USB 栈，不允许通过串行仿真或 DFU 更新
- 禁用进一步的 eFuse 读保护，防止攻击者读保护安全启动公钥摘要

**调试接口**：

- JTAG 接口默认被禁用
- UART 下载模式切换到安全模式

详细限制说明：`安全启动启用后的限制 <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/security/secure-boot-v2.html#restrictions-after-secure-boot-is-enabled>`_

软件签名验证
~~~~~~~~~~~~~~~

软件签名验证提供了一种轻量级的签名验证机制，和硬件安全启动使用相同的签名方案，但仅在 OTA 更新时验证新镜像，适用于对启动速度敏感或物理安全要求较低的场景，但不具备完整的启动链保护能力。

**软件签名 vs 硬件安全启动**：

.. list-table:: 软件签名 vs 硬件安全启动
    :header-rows: 1
    :widths: 20 40 40

    * - 项目
      - 🛡️ Secure Boot v2（硬件）
      - 🔓 软件签名验证机制（无 Secure Boot）
    * - 启动时验证
      - ✅ 验证 bootloader 和 app 镜像签名
      - ❌ 不验证当前 app，假定其可信
    * - OTA 更新验证
      - ✅ 使用 eFuse 中密钥验证新 app 签名
      - ✅ 使用当前 app 的签名块公钥验证新 app
    * - 安全根（Root of Trust）
      - eFuse 中烧录的公钥摘要
      - 当前运行 app 的签名块中的公钥
    * - eFuse 配置要求
      - 必须烧录 SECURE_BOOT_EN 和 KEY_DIGESTx
      - 无需烧录 Secure Boot 相关 eFuse
    * - 密钥撤销机制
      - ✅ 支持 KEY_REVOKE_x 和激进撤销策略
      - ❌ 不支持密钥撤销
    * - 签名块支持数量
      - ✅ 最多 3 个签名块，支持多密钥验证
      - ❌ 仅使用第一个签名块，忽略其他
    * - 防篡改能力
      - ✅ 防止 Flash 被替换或注入恶意代码
      - ❌ 无法防止物理攻击或 Flash 替换
    * - 启动性能
      - ❌ 启动时有签名验证开销
      - ✅ 启动更快，无验证延迟
    * - 开发便利性
      - ❌ 启用 eFuse 后不可逆，需谨慎操作
      - ✅ 无不可逆操作，适合开发调试
    * - 适用场景
      - 量产部署、高安全性要求设备
      - 开发测试、启动速度敏感、物理安全可控环境
    * - 密钥管理灵活性
      - ✅ 支持多密钥轮换与撤销
      - ❌ 仅依赖当前 app 的公钥，无法轮换
    * - 推荐使用
      - ✅ 官方推荐用于正式产品
      - ⚠️ 仅在明确威胁模型下使用，需谨慎评估

**配置方法**：

启用 ``CONFIG_SECURE_SIGNED_APPS_NO_SECURE_BOOT`` 选项。

详情参考：`无硬件安全启动的签名应用程序验证 <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/security/secure-boot-v2.html#signed-app-verification-without-hardware-secure-boot>`_

示例代码
~~~~~~~~~~~

完整的安全启动使用示例请参考：

- `ESP-IDF 安全启动示例 <https://github.com/espressif/esp-idf/tree/master/tools/test_apps/security/secure_boot>`_
- `安全功能综合示例 <https://github.com/espressif/esp-idf/tree/master/examples/security/security_features_app>`_

这些示例展示了：

- 安全启动状态检查
- 签名密钥生成和管理
- 多密钥签名和撤销
- 与 Flash 加密的配合使用

最佳实践
~~~~~~~~~~~

1. **使用高质量熵源生成签名密钥**
2. **始终保持签名密钥私密**
3. **避免第三方观察密钥生成或签名过程**
4. **启用所有安全启动配置选项**
5. **结合 Flash 加密使用**
6. **使用多个密钥减少单点故障**
7. **制定密钥轮换策略**

更多最佳实践：`安全启动最佳实践 <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/security/secure-boot-v2.html#secure-boot-best-practices>`_

常见问题 (FAQ)
~~~~~~~~~~~~~~~~~

* 请参考：`ESP-FAQ 安全部分 <https://docs.espressif.com/projects/esp-faq/zh_CN/latest/software-framework/security.html>`_
