## Build ELF file Example

This example shows how to build ELF file.

## How to Use Example

Before project configuration and build, be sure to set the correct chip target using `idf.py set-target <chip_name>`.

* Note: Only ESP32, ESP32-S2, ESP32-S3, ESP32-C6, ESP32-C61 and ESP32-P4 are supported

### Build and Flash

To reduce the compilation time, please run the following command to force the use of Makefile: 

```bash
idf.py -G 'Unix Makefiles' set-target <chip-name>
```

Then input the ELF APP build command as follows:

```bash
idf.py elf
```

After compilation, you can find `hello_world.app.elf` in the `build` directory.
