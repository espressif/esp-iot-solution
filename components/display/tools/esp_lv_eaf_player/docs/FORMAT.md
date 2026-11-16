# EAF4 统一容器与帧模式（0..11）格式规范

`EAF4` 是支持 `BGRA8888`、`RGB565` 与 `RGB565A8` 的统一带索引、带 CRC 动画容器。
这里的 `mode 0..11` 是 EAF4 索引中的**帧模式**，不是统一编码表的
顶层 codec ID：帧模式 `0..5, 7, 8, 10, 11` 属于 `0x04 LOSSLESS`，帧模式 `6, 9` 属于
`0x05 BOUNDED`。

支持全四通道 8-bit (`B, G, R, A`) 与 16-bit (`RGB565` / `RGB565A8`) 无损与有界误差压缩容器。Alpha 为
**Straight Alpha（非预乘）**：RGB 不预乘 A，且 `A=0` 下的隐藏 RGB 仍属于
帧数据。宽和高必须是 4 的倍数，且各自不超过 1024。文件小于 32 MiB，
帧数不超过 10000。输出格式与透明度接口见 `include/esp_eaf_player.h`。

## 1. 文件布局

```
[64-byte header][24-byte index × N][zlib payload × N]
```

多字节整数均为小端（Little-Endian）。

### Header（64 字节）

| 偏移 | 类型 | 字段 | 约束 |
| ---: | --- | --- | --- |
| 0 | 4s | magic | `EAF4` |
| 4 | u16 | version | `1` |
| 6 | u16 | header_size | `64` |
| 8 | u16 | width | 4 的倍数 |
| 10 | u16 | height | 4 的倍数 |
| 12 | u32 | frames | 1..10000 |
| 16 | u32 | period_ms | 10..10000 |
| 20 | u32 | frame_bytes | `width * height * bpp` (`bpp` = 4 for BGRA8888, 3 for RGB565A8, 2 for RGB565) |
| 24 | u32 | index_offset | `64` |
| 28 | u32 | data_offset | `64 + frames * 24` |
| 32 | u32 | file_size | 整个文件长度 |
| 36 | u16 | tile_w | `4` |
| 38 | u16 | tile_h | `4` |
| 40 | u32 | header_crc | 本字段置 0 后，对 64 字节头做 IEEE CRC-32 |
| 44 | u32 | index_crc | 索引区 CRC-32 |
| 48 | u32 | flags | `1` |
| 52 | u8 | pixel_format | `0 = BGRA8888`（v1 默认兼容），`1 = RGB565`，`2 = RGB565A8` |
| 53 | u8 | codec_id | `0 = 由帧索引决定（v1 兼容）`，或 `0x04 (LOSSLESS)` / `0x05 (BOUNDED)` |
| 54 | 10s | reserved | 全 0 |

### Index Entry（24 字节）

| 偏移 | 类型 | 字段 |
| ---: | --- | --- |
| 0 | u32 | payload_offset |
| 4 | u32 | payload_length |
| 8 | u32 | decoded_size（zlib 解压后的字节数） |
| 12 | u32 | frame_crc32（按 pixel_format 重建的整帧字节 CRC32；RGB565A8 为颜色平面后接 A8） |
| 16 | u8 | mode（`0..11`） |
| 17 | u8 | 0 |
| 18 | u16 | 0 |
| 20 | u32 | payload_crc32（压缩字节流 CRC32） |

---

## 2. 全部帧模式 (`mode = 0..11`)

每一帧先构造对应模式的未压缩字节流，再用 zlib level 6 压缩。C 容器解析器（`components/esp_lv_eaf_player/src/container/eaf4/container.c`）通过统一 codec registry 调用对应后端；Python 参考解码器位于 `encoder/eaf4.py`：

| 值 | Frame mode（前缀 `EAF4_FRAME_`） | 像素格式 | 载荷含义 |
| ---: | --- | --- | --- |
| 0 | `FULL` | RGB565 / RGB565A8 / BGRA8888 | 完整绝对像素帧（仍经 zlib 压缩） |
| 1 | `XOR_BGRA8888` | BGRA8888 | 整帧交错 BGRA8888 XOR |
| 2 | `XOR_PLANAR_BGRA8888` | BGRA8888 | 整帧 B/G/R/A 分通道 XOR |
| 3 | `TILE_XOR_PLANAR_BGRA8888` | BGRA8888 | 4×4 变化块位图 + 分通道 XOR |
| 4 | `TILE_XOR_PACKED_BGRA8888` | BGRA8888 | 4×4 变化块位图 + 交错像素 XOR |
| 5 | `TILE_JOINT_DELTA_BGRA8888` | BGRA8888 | 4×4 变化块 + 联合小差分、逃逸值 |
| 6 | `TILE_BOUNDED_BGRA8888` | BGRA8888 | 4×4 有界误差色胞、跳过块和原始块逃逸 |
| 7 | `TILE_DELTA_RGB565` | RGB565 / RGB565A8 | RGB565 颜色 XOR + 可选 A8 加法差分 |
| 8 | `TILE_JOINT_DELTA_RGB565` | RGB565 / RGB565A8 | RGB565 联合小差分 + 可选 A8、逃逸值 |
| 9 | `TILE_BOUNDED_RGB565` | RGB565 | RGB565 有界误差色胞、跳过块和原始块逃逸 |
| 10 | `HSUB_BGRA8888` | BGRA8888 | 整帧水平字节算术差分（支持独立关键帧，直接解压到帧缓冲） |
| 11 | `TILE_SUB_PACKED_BGRA8888` | BGRA8888 | 4×4 变化块位图 + 交错像素字节算术差分 |

---

## 3. 编码策略

- Lossless 使用统一自适应策略：关键帧从 `FULL / HSUB` 选择（HSUB 至少节省 256 字节）；帧间比较 `FULL / HSUB / TILE_XOR_PACKED / TILE_SUB_PACKED`；`JOINT_DELTA` 需额外节省至少 256 字节和 8% 才采用。
- Bounded 使用 `Mode 6`；`--max-error`（默认 8）控制单通道最大绝对误差上限。

## 命名约束

名称以 [NAMING.md](NAMING.md) 为准。`FULL` 的帧模式值仍是 0；所有 ID、
文件版本、字节序与载荷布局保持原定义。帧模式不使用 codec ID 的命名空间。
`codec_id=0` 在 EAF4 头中表示 AUTO，与 EAF block 的 `CODEC_RLE=0` 语义不同。

## 4. 新增 SUB 模式与可选预处理

`HSUB_BGRA8888` 每行首像素保存原值，随后每个 BGRA 通道保存
`(current[x] - current[x-1]) mod 256`；每行独立累加重建，不依赖上一帧。
`TILE_SUB_PACKED_BGRA8888` 沿用 mode 4 的变化块位图和 tile/行/像素/通道顺序，
保存 `(current - previous_reconstructed) mod 256`，解码逐通道相加，通道间不进位。
两种模式都是严格无损的载荷表示，旧模式 0..9 的编号和语义保持不变。

EAF4 BGRA8888 的 Lossless／Bounded 默认将 A=0 的 RGB 清零，不影响 straight-alpha 显示；
使用 `--no-clean-alpha` 可保留隐藏 RGB 字节。
`--temporal-deadzone N` 仅支持 EAF4 BGRA8888 lossless，N 为 0..255，默认 0。
非关键帧中，四通道与上一重建像素差值均不超过 N 时，可沿用旧像素；仅在压缩
载荷更小时采用。关键帧不应用时间死区，误差每帧对当前输入重新检查。
这属于编码端有损预处理，后续载荷仍由无损模式解码。
summary 显式记录两个预处理参数；任一有损选项启用时 `roundtrip_exact=false`。
