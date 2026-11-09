# esp_ddelta

* [中文版](README_CN.md)

A Python extension that encodes and decodes [ddelta](https://github.com/julian-klode/ddelta) patches. Use it locally, and from `gen_custom_ota.py` when packing a differential compressed OTA image.

This package handles only the **uncompressed** `DDELTA40` stream. xz compression and the custom OTA header are applied by `gen_custom_ota.py` in the parent directory.

The Python binding is part of esp-iot-solution and is Apache-2.0. The bundled ddelta sources remain BSD, and libdivsufsort remains MIT. The full text is in [NOTICE](NOTICE). After installation it is at `esp_ddelta-*.dist-info/licenses/NOTICE`.

| Function | Input | Output |
| --- | --- | --- |
| Encode `encode` | old firmware + new firmware | uncompressed patch |
| Decode `decode` | old firmware + patch | restored new firmware |

## Install

Only the source is maintained. There is no prebuilt package. Compile it locally on Windows, Ubuntu, and macOS. You need Python 3.8+ and a C compiler (gcc or clang on Linux/macOS, MSVC or MinGW on Windows). `pip install` compiles the extension. libdivsufsort does not need a separate install. A successful import means this machine can use the tool.

Debian/Ubuntu marks the system Python as externally managed, so a direct `pip install` fails. Use a virtual environment:

```bash
cd tools/cmake_utilities/scripts/esp_ddelta
python3 -m venv .venv
.venv/bin/python -m pip install -e .
```

Run later commands with that interpreter:

```bash
.venv/bin/python -m esp_ddelta -h
```

To install into the current environment instead:

```bash
python3 -m pip install -e tools/cmake_utilities/scripts/esp_ddelta
```

On Windows, replace `.venv/bin/python` with `.venv\Scripts\python.exe`. macOS uses the same commands as Linux.

The build is successful when importing the extension prints `ok`:

```bash
.venv/bin/python -c "import esp_ddelta._core; print('ok')"
```

On Windows:

```bash
.venv\Scripts\python.exe -c "import esp_ddelta._core; print('ok')"
```

## Command line

Run these with the interpreter that has the package installed.

Encode (old firmware + new firmware → uncompressed patch):

```bash
python3 -m esp_ddelta encode old.bin new.bin patch
```

Decode (old firmware + patch → new firmware) and compare with the original new firmware:

```bash
python3 -m esp_ddelta decode old.bin patch restored.bin
cmp new.bin restored.bin
```

`cmp` prints nothing and exits 0 when the restore matches. The patch starts with the ASCII magic `DDELTA40`.

## Python API

```python
import esp_ddelta

patch = esp_ddelta.encode(old_bytes, new_bytes)
restored = esp_ddelta.decode(old_bytes, patch)
assert restored == new_bytes

esp_ddelta.encode_file("old.bin", "new.bin", "patch")
esp_ddelta.decode_file("old.bin", "patch", "restored.bin")
```

`encode` / `decode` take `bytes` in memory. `encode_file` / `decode_file` read and write paths, which suits larger firmware images.

## Use with gen_custom_ota.py

To build a downloadable differential package (ddelta encode → xz → v3 header), use the same Python that has `esp_ddelta` installed:

```bash
python3 ../gen_custom_ota.py -hv v3 -c xz -d ddelta \
    -b old.bin -i new.bin --add_app_header
```

| Argument | Meaning |
| --- | --- |
| `-hv v3` | Differential fields are stored in the v3 header. v3 is required |
| `-c xz` | xz-compress the uncompressed patch. xz is the default |
| `-d ddelta` | Encode a ddelta patch first. The default is `none`, which compresses the whole firmware |
| `-b` | Old firmware currently running on the device |
| `-i` | Target new firmware |
| `--add_app_header` | Prepend an app image header. `idf.py gen_diff_ota` always passes this, so `esp_https_ota` can use the native OTA APIs |
| `--pad_https_ota` | Optional. Pad the final file with zeros when it is shorter than 1024 bytes, so `esp_https_ota` can finish reading the image header. Off by default |

The current directory then contains `custom_ota_binaries/`:

| File | Contents |
| --- | --- |
| `patch` | Uncompressed patch. It can be checked with `esp_ddelta decode` |
| `patch.xz` | xz-compressed patch |
| `patch.xz.packed` | Patch plus the v3 header and, with `--add_app_header`, the app image header. This is the file to transfer |

Check the uncompressed patch before using the packed file:

```bash
python3 -m esp_ddelta decode old.bin custom_ota_binaries/patch restored.bin
cmp new.bin restored.bin
```

A pure compressed update is unchanged. Do not pass `-d`:

```bash
python3 ../gen_custom_ota.py -hv v3 -i new.bin
```

Inside an ESP-IDF project, after `include(gen_diff_ota)`:

```bash
idf.py -DDIFF_OTA_NEW_FILE=backup/simple_ota.bin gen_diff_ota
```

The command builds the project and uses `build/<project>.bin` as the base. `-DDIFF_OTA_NEW_FILE` is the firmware the patch should produce. The path is relative to the project directory. Copy the new firmware aside first: the build overwrites `build/<project>.bin`. Install `esp_ddelta` into the Python that `idf.py` uses. The output is `build/custom_ota_binaries/patch.xz.packed`.

## Differential fields in the v3 header

The header is still 40 bytes. For a pure compressed image, the type byte and the reserved field stay as they were.

Type byte (offset 5):

- Low 4 bits: compression type. `0` = none, `1` = xz
- High 4 bits: delta type. `0` = none, `1` = ddelta
- ddelta plus xz sets this byte to `0x11`

The next 10 bytes are used only with `-d ddelta`:

| Offset | Length | Contents |
| --- | --- | --- |
| 6 | 4 | `base_len`, little-endian. Length of the old firmware covered by the CRC, usually 4096 |
| 10 | 4 | `base_crc32`, little-endian. CRC32 of the first `base_len` bytes of the old firmware |
| 14 | 2 | 0 |

A pure compressed image still fills these 10 bytes with the historical `0x30` followed by nine `0x00` bytes. Existing v3 compressed packages do not need to change.
