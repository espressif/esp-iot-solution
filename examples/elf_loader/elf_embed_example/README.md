## Embedded ELF Example

This example shows how to use ELF loader to run an ELF file embedded in the main project file.

## How to Use Example

Before project configuration and build, be sure to set the correct chip target using `idf.py set-target <chip_name>`.

* Note: Only ESP32, ESP32-S2, ESP32-S3, ESP32-C6, ESP32-C61 and ESP32-P4 are supported

### Hardware Required

* A development board based on espressif ESP32/ESP32-S2/ESP32-S3/ESP32-C6/ESP32-C61/ESP32-P4 SoC
* A USB cable for power supply and programming

### Configure the Project

Open the project configuration menu (`idf.py menuconfig`).

In the `Example Configuration` menu:

* Enable ELF input arguments in the `Support arguments for ELF main` option if necessary.
* Set the arguments in the `Arguments of ELF main` option.

### Adjust the ELF project configuration

1. Enter the `elf_loader_example` directory

    ```shell
    cd ./esp-iot-solution/examples/elf_loader/elf_loader_example
    ```
2. Modify the `main/CMakeLists.txt`
    * Set the following parameters for the ELF application, e.g:

    ```cmake
    set(elf_app_name "elf_example")
    set(elf_app_path "${CMAKE_CURRENT_LIST_DIR}/elf_example_project")
    set(elf_bin_name "test_elf")
    ```
    * `elf_app_name`: ELF application project name.
    * `elf_app_path`: Relative path of the ELF application project.
    * `elf_bin_name`: Filename of the generated ELF binary firmware

3. Modify the main project files accordingly

    * Modify the starting address passed to the ELF application firmware, e.g: `test_elf_start`

    ```c
    ...
    int app_main(void)
    {
        ...
        const uint8_t *buffer = test_elf_start;
        ...
    }
    ...
    ```

    * Update the ELF application symbol table array, e.g: `g_esp_elf_example_elfsyms`

    ```c
    ...
    extern esp_elf_symbol_table_t *g_esp_elf_example_elfsyms[];
    ...
    esp_elf_register_symbol(g_esp_elf_example_elfsyms);
    ...
    esp_elf_unregister_symbol(g_esp_elf_example_elfsyms);
    ...
   ```

    * Note: This symbol array can also be found in the file: `build/generated/elf_example/esp_elf_example.c`

4. Support for multiple embedded ELF projects

    * To support multiple ELF applications, you need to define unique configurations for each in `main/CMakeLists.txt` and update the firmware addresses in the main project.

    (1). Modify `main/CMakeLists.txt`: Define different sets of `elf_app_name`, `elf_app_path`, and `elf_bin_name` for each application, then register them using `elf_embed_binary`, e.g:

    ```cmake
    set(elf_app_name "elf_example")

    # Set the address of the compiled project.
    # If it is an external project, for example: <project_root>/esp-iot-solution/examples/elf_loader/build_elf_file_example
    # Configure elf_app_path to /path/to/build_elf_file_example: "${CMAKE_CURRENT_LIST_DIR}/../../build_elf_file_example"

    set(elf_app_path "${CMAKE_CURRENT_LIST_DIR}/elf_example_project")
    set(elf_bin_name "test_elf")

    set(elf_app_name2 "hello_world")
    set(elf_app_path2 "${CMAKE_CURRENT_LIST_DIR}/../../build_elf_file_example")
    set(elf_bin_name2 "test_elf2")

    idf_component_register(SRCS "elf_embed_example_main.c")

    elf_embed_binary(${elf_app_name} ${elf_app_path} ${elf_bin_name})
    elf_embed_binary(${elf_app_name2} ${elf_app_path2} ${elf_bin_name2})

    ```
    (2). Modify the main project files: Update the source code to reference the corresponding firmware addresses for each embedded ELF, e.g:
    ```c
    ...
    extern const uint8_t test_elf2_start[] asm("_binary_test_elf2_bin_start");
    extern const uint8_t test_elf2_end[]   asm("_binary_test_elf2_bin_end");
    extern esp_elf_symbol_table_t g_esp_hello_world_elfsyms[];
    ...
    const uint8_t *buffer2 = test_elf2_start;
    ...
    esp_elf_register_symbol(g_esp_hello_world_elfsyms);
    ...
    esp_elf_unregister_symbol(g_esp_hello_world_elfsyms);
    ...
   ```


### Build and Flash

Run `idf.py -p PORT flash monitor` to build, flash and monitor the project.

(To exit the serial monitor, type `Ctrl-]`.)

## Example Output

As you run the example, you will see the following log:

```
I (291) ELF: elf->entry=0x4081244a
I (291) elf_loader: Start to run ELF file
hello world test 0
hello world test 1
hello world test 2
hello world test 3
hello world test 4
hello world test 5
hello world test 6
hello world test 7
hello world test 8
hello world test 9
I (10401) elf_loader: Unregister symbol table
I (10401) elf_loader: Success to exit from ELF file
```
