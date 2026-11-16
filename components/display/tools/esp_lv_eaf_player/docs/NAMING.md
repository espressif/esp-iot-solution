# EAF 命名规范 — 冻结版 Revision 1

冻结日期：2026-11-11。本规范约束代码、CLI、日志、文档与新生成的测试报告。
名称规范版本与文件头中的二进制版本独立；本轮不改变任何已有编号、字节序和载荷布局。
历史实测报告可保留采样时的标签，数字与测量口径不因改名而重写。

## 1. 命名空间

| 层级 | C 类型 | C 符号前缀 | Python 前缀 | 日志字段 |
| --- | --- | --- | --- | --- |
| Codec 算法类别 | `esp_eaf_codec_id_t` | `ESP_EAF_CODEC_` | `CODEC_` | `codec` |
| EAF4 帧载荷布局 | `eaf4_frame_mode_t` | `EAF4_FRAME_` | `FRAME_` | `frame_mode` |
| Bounded 块原语 | `eaf_bounded_block_t` | `BOUNDED_BLOCK_` | `BOUNDED_BLOCK_` | `block_mode` |

三个编号空间独立。经典 EAF block 首字节保存 codec ID；EAF4 索引字节 16
保存 frame mode；Bounded 解压后的块模式表保存 block opcode。不得仅凭一个
未标明命名空间的整数选择算法，不接受其他实验仓库编号映射。

### C 实现与文件命名

- 公共 API 和公共类型保留 `esp_eaf_` 前缀；EAF4 公共接口沿用 `eaf4_`。
- 跨文件内部符号使用 `eaf_`，仅属于 EAF4 的实现使用 `eaf4_`。
  例如 `eaf_lossless_decode`、`eaf4_lossless_decode`、`eaf_codec_ops_t`。
- 文件内 `static` 辅助函数不重复组件前缀，以职责命名。
- 内部文件由目录说明归属，例如 `codec/codec_registry.c`、
  `codec/eaf4/lossless.c`、`port/host/jpeg.c`；公共头文件名称保持不变。
- 内部实现名称不属于码流规范，不为旧内部名称保留别名或映射。

## 2. Codec ID

| ID | C 枚举 | 规范字符串 / 日志名称 | 展示名称 | 公共 CLI |
| ---: | --- | --- | --- | --- |
| `0x00` | `ESP_EAF_CODEC_RLE` | `RLE` | RLE | `—` |
| `0x01` | `ESP_EAF_CODEC_HUFFMAN_RLE` | `HUFFMAN_RLE` | Huffman + RLE | `—` |
| `0x02` | `ESP_EAF_CODEC_JPEG` | `JPEG` | JPEG | `—` |
| `0x03` | `ESP_EAF_CODEC_HUFFMAN` | `HUFFMAN` | Huffman | `—` |
| `0x04` | `ESP_EAF_CODEC_LOSSLESS` | `LOSSLESS` | EAF Lossless | `lossless` |
| `0x05` | `ESP_EAF_CODEC_BOUNDED` | `BOUNDED` | EAF Bounded | `bounded` |
| `0x06` | `ESP_EAF_CODEC_JPEG_DELTA` | `JPEG_DELTA` | EAF JPEG Delta | `jpeg-delta` |

`HUFFMAN_RLE` 表示先解 Huffman、再解 RLE。RLE/Huffman 不限定为调色板数据；
帧色深决定解压后数据的解释方式。`JPEG` 不把关键帧角色写进算法名。
`BOUNDED` 的最大误差由参数控制，默认值 8 不属于模式名称，也不是所有配置的固定阈值。

EAF4 头的 `codec_id` 目前允许 `LOSSLESS`、`BOUNDED` 或 0（AUTO）。
这里的 AUTO=0 是容器字段的特殊值，不表示该文件使用 RLE。

## 3. EAF4 Frame Mode

| 值 | C 枚举 | 载荷含义 |
| ---: | --- | --- |
| 0 | `EAF4_FRAME_FULL` | 完整绝对像素帧（仍经 zlib 压缩） |
| 1 | `EAF4_FRAME_XOR_BGRA8888` | 整帧交错 BGRA8888 XOR |
| 2 | `EAF4_FRAME_XOR_PLANAR_BGRA8888` | 整帧 B/G/R/A 分通道 XOR |
| 3 | `EAF4_FRAME_TILE_XOR_PLANAR_BGRA8888` | 4×4 变化块位图 + 分通道 XOR |
| 4 | `EAF4_FRAME_TILE_XOR_PACKED_BGRA8888` | 4×4 变化块位图 + 交错像素 XOR |
| 5 | `EAF4_FRAME_TILE_JOINT_DELTA_BGRA8888` | 4×4 变化块 + 联合小差分、逃逸值 |
| 6 | `EAF4_FRAME_TILE_BOUNDED_BGRA8888` | 4×4 有界误差色胞、跳过块和原始块逃逸 |
| 7 | `EAF4_FRAME_TILE_DELTA_RGB565` | RGB565 颜色 XOR + 可选 A8 加法差分 |
| 8 | `EAF4_FRAME_TILE_JOINT_DELTA_RGB565` | RGB565 联合小差分 + 可选 A8、逃逸值 |
| 9 | `EAF4_FRAME_TILE_BOUNDED_RGB565` | RGB565 有界误差色胞、跳过块和原始块逃逸 |
| 10 | `EAF4_FRAME_HSUB_BGRA8888` | 整帧水平字节算术差分（支持独立关键帧） |
| 11 | `EAF4_FRAME_TILE_SUB_PACKED_BGRA8888` | 4×4 变化块位图 + 交错像素字节算术差分 |

规范字符串是去掉 `EAF4_FRAME_` 后的完整大写名称；Python 符号使用 `FRAME_` 前缀。
所有帧载荷仍有外层 zlib 压缩。`FULL` 是绝对像素帧，不能被理解为未压缩文件。
模式 7/8 的 RGB565 表示颜色布局，是否携带 A8 由 pixel_format 明确决定。
模式 6/9 归属 BOUNDED；其余归属 LOSSLESS，FULL 也允许作为 BOUNDED 独立关键帧。

## 4. Bounded Block Opcode

| 值 | C / Python 符号 | 含义 |
| ---: | --- | --- |
| 0 | `BOUNDED_BLOCK_SKIP` | 保留上一重建块 |
| 1 | `BOUNDED_BLOCK_SOLID` | 整块单色 |
| 2 | `BOUNDED_BLOCK_PALETTE2` | 两色端点 + 1-bit 索引 |
| 3 | `BOUNDED_BLOCK_PALETTE4` | 四色插值 + 2-bit 索引 |
| 4 | `BOUNDED_BLOCK_LITERAL` | 原始块像素；不做块内近似 |

## 5. 像素格式与 CLI

| 规范名称 | CLI `--format` | 字节布局 |
| --- | --- | --- |
| `RGB565` | `rgb565` | 小端 RGB565 颜色平面 |
| `RGB565A8` | `rgb565a8` | RGB565 颜色平面后接完整 A8 平面 |
| `BGRA8888` | `bgra8888` | 交错 B、G、R、A 四字节 |

Alpha 为 straight alpha。RGBA 仅可用于描述实际 RGBA 输入，不作为 BGRA8888 输出别名。
公共输出说明不得用 565/8888 简写替代精确像素格式。

CLI codec 仅接受 `lossless`、`bounded`、`jpeg-delta`。
最大误差选项仅使用 `--max-error`。不保留 `video`、`btc-safe`、`quad-jpeg`、
`--btc-max-error` 或旧像素格式简写作为兼容入口。

| Codec | BGRA8888 | RGB565 | RGB565A8 |
| --- | --- | --- | --- |
| Lossless / EAF4 | 支持 | 支持 | 支持 |
| Bounded / EAF4 | 支持连续 Alpha 的有界误差 | 支持 | 不支持 |
| JPEG Delta / EAF | 不支持 | 支持 | 不支持 |

经典 EAF Lossless 编码入口使用 RGB565A8。

## 6. 固定规则与验证

- 既有 ID、规范符号和载荷语义不得重新分配；新算法/新布局分配新值。
- 枚举或函数名称本身不写入文件，源代码改名不需要改变二进制版本。
- 编码策略/profile、质量参数、硬件后端不是 codec ID 或 frame mode，不混入模式名。
- C 头文件定义协议值；Python 规范表集中在 `encoder/names.py`，不提供旧符号别名。
- `tests/test_naming.py` 核对固定值、C/Python/日志名称、CLI 拒绝旧拼写及精确像素格式。
- `make host-test` 与 `make host-sanitize` 验证重命名后的解码和生命周期行为。
