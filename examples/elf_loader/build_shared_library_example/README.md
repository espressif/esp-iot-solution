## Build shared library Example

This example shows how to build shared library.

## How to Use Example

Before project configuration and build, be sure to set the correct chip target using `idf.py -G 'Unix Makefiles' set-target <chip_name>`.

* Note: Only ESP32, ESP32-S2, ESP32-S3, ESP32-C6, ESP32-C61 and ESP32-P4 are supported

### Build and Flash

To reduce the compilation time, please run the following command to force the use of Makefile: 

```bash
idf.py -G 'Unix Makefiles' set-target <chip_name>
```

Then input the shared library build command as follows:

```bash
idf.py so
```

After compilation, you can find `lib.so` in the `build` directory.
