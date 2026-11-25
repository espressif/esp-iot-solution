# EAF/AAF 文件检查工具

按本仓库 `src/container/eaf/container.c` 的布局检查原有 EAF/AAF 文件。

```bash
python3 tools/eaf_inspect.py path/to/file.eaf
python3 tools/eaf_inspect.py --frames path/to/file.eaf
python3 tools/eaf_inspect.py -v path/to/file.eaf
python3 tools/eaf_inspect.py --json path/to/file.eaf
python3 -m tools.eaf_inspector path/to/file.eaf
```

检查文件头、checksum、帧表、图片头、调色板、block 边界和规范 codec ID
`0x00..0x06`。不接受其他实验仓库编号；关闭 block 解码仍检查 codec ID。

RLE、Huffman、Huffman+RLE 支持 Python 解码检查。JPEG、LOSSLESS、BOUNDED、
JPEG_DELTA 只检查容器结构，完整像素验证由 Host C 测试负责。
`0x05` 是 BOUNDED，不是 RAW。

EAF/EAF4 统一摘要使用 `python3 tools/eaf_cli.py inspect path/to/file`。
