# XZ Decompress Example

This example demonstrates how to use function `xz_decompress()` to decompress the specified file.
The `xz_decompress()` is based on the `XZ Embedded` library, for more information about `XZ Embedded`, please refer to [XZ Embedded](https://tukaani.org/xz/embedded.html).

### Generate the Compressed File

> Note:
> Please install **xz** tool based on your operation system, this part will not be described here, please google it if needed.

To compress the specified file, use the following command:

```shell
xz --check=crc32 --lzma2=dict=8KiB -k file_name
```

Use `xz -h` to get more information about how to use `xz` command to compress the specified file.

In this example, we use the following command to compress `test_file/hello.txt`:

```
xz --check=crc32 --lzma2=dict=8KiB -k test_file/hello.txt
```

### Build and flash

To run the example, type the following command:

```
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the Getting Started Guide for full steps to configure and use [ESP-IDF](https://github.com/espressif/esp-idf) to build projects.

## Example output

Here is the example's console output:

```
...
I (309) xz decompress: origin file size is 393, compressed file size is 93 bytes

I (319) xz decompress: *****************test buf to buf begin****************

I (329) xz decompress: decompress data:
Hello World!Hello Everyone!
Hello World!Hello Everyone!
Hello World!Hello Everyone!
Hello World!Hello Everyone!
Hello World!Hello Everyone!
Hello World!Hello Everyone!
Hello World!Hello Everyone!
Hello World!Hello Everyone!
Hello World!Hello Everyone!
Hello World!Hello Everyone!
Hello World!Hello Everyone!
Hello World!Hello Everyone!
Hello World!Hello Everyone!
Hello World!Hello Everyone!

I (369) xz decompress: ret = 0, decompressed count is 92
I (369) xz decompress: *****************test buf to buf end******************
```
