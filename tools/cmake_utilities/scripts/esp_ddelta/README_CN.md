# esp_ddelta

* [English Version](README.md)

把 [ddelta](https://github.com/julian-klode/ddelta) 的差分编码 / 差分解码做成 Python 扩展，供本机调试，以及给 `gen_custom_ota.py` 制作差分压缩包。

本包只处理**未压缩**的 `DDELTA40` 差分流。xz 压缩和 custom OTA 头由上一级目录中的 `gen_custom_ota.py` 负责。

Python 绑定属于 esp-iot-solution，许可证是 Apache-2.0。打包进来的 ddelta 仍是 BSD，libdivsufsort 仍是 MIT。全文在 [NOTICE](NOTICE)，安装后位于 `esp_ddelta-*.dist-info/licenses/NOTICE`。

| 函数 | 输入 | 输出 |
| --- | --- | --- |
| 编码 `encode` | 旧固件 + 新固件 | 未压缩差分包 `patch` |
| 解码 `decode` | 旧固件 + `patch` | 还原出的新固件 |

## 安装

本包只维护源码，不发布预编译包。Windows、Ubuntu、macOS 都用下面的步骤在本机编译。需要 Python 3.8+，以及本机 C 编译器（Linux/macOS 用 gcc 或 clang，Windows 用 MSVC 或 MinGW）。`pip install` 时编译扩展，不需要单独安装 libdivsufsort。编译并成功导入即表示该系统可用。

Debian/Ubuntu 的系统 Python 往往是 externally-managed，直接 `pip install` 会失败。用虚拟环境：

```bash
cd tools/cmake_utilities/scripts/esp_ddelta
python3 -m venv .venv
.venv/bin/python -m pip install -e .
```

之后命令都用这个虚拟环境里的解释器：

```bash
.venv/bin/python -m esp_ddelta -h
```

不使用虚拟环境、且允许往当前环境安装时：

```bash
python3 -m pip install -e tools/cmake_utilities/scripts/esp_ddelta
```

Windows 把 `.venv/bin/python` 换成 `.venv\Scripts\python.exe`。macOS 与 Linux 相同。

编译成功的标志是能导入扩展模块，并打印 `ok`：

```bash
.venv/bin/python -c "import esp_ddelta._core; print('ok')"
```

Windows 使用 `.venv\Scripts\python.exe -c "import esp_ddelta._core; print('ok')"`。macOS 与 Linux 相同。

## 命令行

在已安装该包的解释器上执行。

编码（旧固件 + 新固件 → 未压缩差分包）：

```bash
python3 -m esp_ddelta encode old.bin new.bin patch
```

解码（旧固件 + 差分包 → 新固件），并和原新固件比对：

```bash
python3 -m esp_ddelta decode old.bin patch restored.bin
cmp new.bin restored.bin
```

`cmp` 无输出且退出码为 0 表示还原正确。差分包文件头为 ASCII `DDELTA40`。

## Python 接口

```python
import esp_ddelta

patch = esp_ddelta.encode(old_bytes, new_bytes)
restored = esp_ddelta.decode(old_bytes, patch)
assert restored == new_bytes

esp_ddelta.encode_file("old.bin", "new.bin", "patch")
esp_ddelta.decode_file("old.bin", "patch", "restored.bin")
```

`encode` / `decode` 处理内存中的 `bytes`。`encode_file` / `decode_file` 直接读写路径，适合体积较大的固件。

## 和 gen_custom_ota.py 一起用

制作可下发的差分压缩包（差分编码 → xz → v3 头）时，用同一套已安装 `esp_ddelta` 的 Python：

```bash
python3 ../gen_custom_ota.py -hv v3 -c xz -d ddelta \
    -b old.bin -i new.bin --add_app_header
```

| 参数 | 含义 |
| --- | --- |
| `-hv v3` | 差分信息写在 v3 头的保留字段里，必须是 v3 |
| `-c xz` | 对未压缩差分包做 xz。默认就是 xz |
| `-d ddelta` | 先做 ddelta 编码。默认 `none`，即只压缩整包固件 |
| `-b` | 设备上正在运行的旧固件 |
| `-i` | 目标新固件 |
| `--add_app_header` | 在包前加上 app 镜像头。`idf.py gen_diff_ota` 总会带上这个参数，这样 `esp_https_ota` 才能走原生 OTA 接口 |
| `--pad_https_ota` | 可选。最终文件不足 1024 字节时补零，供 `esp_https_ota` 读完镜像头。默认不补 |

当前目录会生成 `custom_ota_binaries/`：

| 文件 | 内容 |
| --- | --- |
| `patch` | 未压缩差分包，可单独拿来 `esp_ddelta decode` |
| `patch.xz` | xz 压缩后的差分包 |
| `patch.xz.packed` | 加上 v3 头，并在带了 `--add_app_header` 时再加上 app 镜像头。这是实际要传输的差分压缩包 |

建议先核对未压缩差分包，再看 packed 文件：

```bash
python3 -m esp_ddelta decode old.bin custom_ota_binaries/patch restored.bin
cmp new.bin restored.bin
```

纯压缩（不做差分）的用法不变，且不要加 `-d`：

```bash
python3 ../gen_custom_ota.py -hv v3 -i new.bin
```

在 ESP-IDF 工程里，`include(gen_diff_ota)` 之后可以直接：

```bash
idf.py -DDIFF_OTA_NEW_FILE=backup/simple_ota.bin gen_diff_ota
```

这条命令会先编译当前工程，把 `build/<project>.bin` 当作 base。`-DDIFF_OTA_NEW_FILE` 是打补丁后要得到的固件，路径相对工程目录。new 固件要事先拷到别处，因为编译会覆盖 `build/<project>.bin`。`esp_ddelta` 要装进 `idf.py` 使用的那套 Python。产物在 `build/custom_ota_binaries/patch.xz.packed`。

## v3 头里和差分相关的字段

头总长仍是 40 字节。纯压缩时类型字节和保留字段与原来一致。

类型字节（偏移 5）：

- 低 4 位：压缩类型，`0` = none，`1` = xz
- 高 4 位：差分类型，`0` = none，`1` = ddelta
- 差分 + xz 时该字节为 `0x11`

紧接着的 10 字节，仅在 `-d ddelta` 时使用：

| 偏移 | 长度 | 内容 |
| --- | --- | --- |
| 6 | 4 | `base_len`，小端。旧固件参与 CRC 的长度，一般为 4096 |
| 10 | 4 | `base_crc32`，小端。旧固件前 `base_len` 字节的 CRC32 |
| 14 | 2 | 0 |

纯压缩时这 10 字节仍是历史填充 `0x30` 后跟 9 个 `0x00`，已有 v3 压缩包不用改。
