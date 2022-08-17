# XZ Decompress Example

This example demonstrates how to use esp_xz_decompress() to decompress the specified file.  
The `esp_xz_decompress()` is based on the XZ Embedded lib, for more information about XZ Embedded, please refer to [XZ Embeded](https://tukaani.org/xz/embedded.html).  

### Generate the Compressed File
To compress the specified file, use the following command:  
```
xz --check=crc32 --lzma2=dict=8KiB -k file_name
```
Use `xz -h` to get more information about how to use xz embedded lib to compress the specified file.  

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

See the Getting Started Guide for full steps to configure and use [ESP-IDF]( https://github.com/espressif/esp-idf) to build projects.

## Example output

Here is the example's console output:

```
...
I (309) xz decry: origin file size is 393, compressed file size is 93 bytes

I (319) xz decry: *****************test buf to buf begin****************

I (329) xz decry: decompress data:
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

I (369) xz decry: ret = 0, decrypted count is 92
I (369) xz decry: *****************test buf to buf end******************
```
