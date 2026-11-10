# Gen Compressed OTA

When using the compressed OTA, we need to generate the compressed app firmware. This document mainly describes how to generate the compressed app firmware.

For more information about compressed OTA, refer to [bootloader_support_plus](https://github.com/espressif/esp-iot-solution/tree/master/components/bootloader_support_plus).

## Use
In order to use this feature, you need to include the needed CMake file in your project's CMakeLists.txt after `project(XXXX)`.

```cmake
project(XXXX)

include(gen_compressed_ota)
```

Generate the compressed app firmware in an ESP-IDF "project" directory by running:

```plaintext
idf.py gen_compressed_ota
```

This command will compile your project first, then it will generate the compressed app firmware. For example, run the command under the project `simple_ota_examples` folder. If there are no errors, the `custom_ota_binaries` folder will be created and contains the following files:

```plaintext
simple_ota.bin.xz  
simple_ota.bin.xz.packed
```

The file named `simple_ota.bin.xz.packed` is the actual compressed app binary file to be transferred.

In addition, if [secure boot](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/security/secure-boot-v2.html) is enabled, the command will generate the signed compressed app binary file:

```plaintext
simple_ota.bin.xz.packed.signed
```

you can also use the script [gen_custom_ota.py](https://github.com/espressif/esp-iot-solution/blob/master/tools/cmake_utilities/scripts/gen_custom_ota.py) to compress the specified app:

```plaintext
python3 gen_custom_ota.py -hv v3 -i simple_ota.bin --add_app_header
```

## How to generate a diff patch

`gen_diff_ota` and `esp_ddelta` require cmake_utilities >=1.4.0. The bootloader that applies the patch is bootloader_support_plus >=0.7.0.

Keep `version.txt` as the version of each release, and keep that release's app binary. `-b` (and the base `gen_diff_ota` builds) must be the firmware running on the device. A newer build cannot stand in for it.

`include(gen_diff_ota)` adds `idf.py gen_diff_ota`. The target compiles the project and uses `build/<project>.bin` as the base. `DIFF_OTA_NEW_FILE` is the firmware the patch should produce. The path is relative to the project directory unless absolute. Copy the new firmware aside before running the command: the build overwrites `build/<project>.bin`.

```plaintext
idf.py -DDIFF_OTA_NEW_FILE=backup/simple_ota.bin gen_diff_ota
```

Install `esp_ddelta` into the same Python that `idf.py` uses, once per machine. See [esp_ddelta/README.md](../scripts/esp_ddelta/README.md).

The command writes `build/custom_ota_binaries/patch.xz.packed`. Serve that packed file, not `patch.xz`. That directory is recreated on each run, so a later `gen_compressed_ota` replaces these files.

`esp_https_ota` reads 1024 bytes of image header. A short patch fails that read. Padding is off unless you enable `CMake Utilities -> Pad packed OTA image to 1024 bytes` (`CONFIG_CU_OTA_PAD_HTTPS`) before `idf.py gen_diff_ota`, or pass `--pad_https_ota` to the script. The extra bytes are zeros after the payload and are not covered by the header length or MD5.

Two existing binaries, without an ESP-IDF project, are still packed by the script. Run it from the directory that holds both files, with the interpreter that has `esp_ddelta` installed:

```plaintext
python3 /path/to/scripts/gen_custom_ota.py \
    -hv v3 -c xz -d ddelta -b current_app.bin -i new_app.bin --add_app_header --pad_https_ota
```

`--add_app_header` is required for the same package that `idf.py gen_diff_ota` writes. That target always passes it, so `esp_https_ota` can download the file with the native OTA APIs. Without it, `patch.xz.packed` has only the v3 custom header.

`--pad_https_ota` pads a packed file shorter than 1024 bytes. Omit it to leave a short package unpadded. `gen_diff_ota` adds it only when `CMake Utilities -> Pad packed OTA image to 1024 bytes` is enabled.

Check the uncompressed patch before using the packed file:

```plaintext
python3 -m esp_ddelta decode current_app.bin custom_ota_binaries/patch restored.bin
cmp new_app.bin restored.bin
```

## Notes

1. Command tool [gen_custom_ota.py](https://github.com/espressif/esp-iot-solution/tree/master/tools/cmake_utilities/scripts/gen_custom_ota.py) can also be used to generate the compressed firmware required by  [esp bootloader plus](https://github.com/espressif/esp-bootloader-plus):

```plaintext
python3 gen_custom_ota.py -i simple_ota.bin
```

Use `python3 gen_custom_ota.py -h` to see a summary of all available command line options.

`esp_ddelta` is maintained as source. Windows, Ubuntu, and macOS compile it locally. A successful `import esp_ddelta._core` means that machine can pack ddelta images. See [esp_ddelta/README.md](../scripts/esp_ddelta/README.md).