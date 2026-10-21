## Description

[![Component Registry](https://components.espressif.com/components/espressif/elf_loader/badge.svg)](https://components.espressif.com/components/espressif/elf_loader)

Espressif ELF(Executable and Linkable Format) loader is a software development kit that is developed based on the ESP-IDF, mainly used to load ELF file compiled based on ESP32 series SoCs to the executable memory area, then link and execute it.

In this way, the application does not need compile into the whole firmware in advance. It is like running the compiled program through terminal input `./main.o` on the Ubuntu platform automatically, which realizes the separation of application and kernel.

This ELF loader supports following SoCs:

- ESP32
- ESP32-S2, support running ELF in PSRAM
- ESP32-S3, support running ELF in PSRAM
- ESP32-S31, support running ELF in PSRAM
- ESP32-P4, support running ELF in PSRAM
- ESP32-C5, support running ELF in PSRAM
- ESP32-C6
- ESP32-C61, support running ELF in PSRAM

> **PMP / memory protection:** Targets with PMP IDRAM split (ESP32-C5/C6/C61/P4/S31, etc.) make DRAM non-executable when `CONFIG_ESP_SYSTEM_PMP_IDRAM_SPLIT=y`. Loading ELF into internal SRAM then requires `CONFIG_ESP_SYSTEM_PMP_IDRAM_SPLIT=n`; otherwise the loader refuses executable allocations. Prefer `ELF_LOADER_LOAD_PSRAM` when SPIRAM is available (not offered on ESP32-C6). Official ESP-IDF support for ESP32-C5 starts at **v5.5.2**.

### Usage

#### Firmware

Add a dependency on this component in your component or project's idf_component.yml file.

    ```yml
    dependencies:
      espressif/elf_loader: "2.*"
    ```

Enable ELF loader in the menuconfig:

```
Component config  --->
    ESP-ELFLoader Configuration  --->
        [*] Enable Espressif ELF Loader
```

You can then optionally enable dynamic load shared object in the menuconfig:

```
Component config  --->
    ESP-ELFLoader Configuration  --->
        [*] Enable dynamic load shared object
```

Add API calls in your project as follows:

```c
#include "esp_elf.h"

    esp_elf_t elf;

    // Your Code

    esp_elf_init(&elf);
    esp_elf_relocate(&elf, elf_file_data_bytes, elf_file_size_bytes);
    esp_elf_request(&elf, 0, argc, argv);
    esp_elf_deinit(&elf);
```

The third argument to `esp_elf_relocate()` is the exact accessible length of the
ELF image. Callers using `esp_elf_open()` should pass `file.size`; embedded ELF
images can use the difference between the `_binary_*_end` and `_binary_*_start`
linker symbols. The buffer must be 4-byte aligned. This argument is required to
reject malformed input before any ELF headers, sections, or relocation entries
are accessed. The old two-argument signature is no longer supported.
For embedded ELF images, use the component's `elf_embed_aligned_binary(target
file BINARY|TEXT)` CMake helper to guarantee input alignment on released
ESP-IDF versions, where `target_add_binary_data(... ALIGN 4)` is ignored.

When dynamic shared object loading is enabled, an ELF shared library without
an entry point can be loaded and queried with `dlsym()`. Such a library cannot
be executed with `esp_elf_request()`.

If dynamic load shared object is enabled, you can use the following APIs in your project:

#### Dynamic Shared Object API

Include the dynamic loader header and use the dl* APIs to load, resolve symbols, and unload shared objects:

```c
#include "esp_dlfcn.h"
```

**Common API list**
- `void *dlopen(const char *file, int mode)` loads a shared object from a filesystem path and returns a module handle (NULL on failure).
- `void *dlsym(void *handle, const char *name)` returns the address of a symbol in the module (NULL if not found).
- `int dlclose(void *handle)` closes the module handle (0 on success, -1 on invalid handle).
- `const char *dlerror(void)` returns the last error message (NULL if no error); copy the string immediately if needed.
- `void dllist(dl_list_t type)` prints module or symbol information (`LIST_MODULE` or `LIST_SYMBOL`).

**Mode flags**
- `RTLD_LAZY`, `RTLD_NOW`, `RTLD_NOLOAD`, `RTLD_DEEPBIND` (use one of the binding modes plus optional flags).

**Typical flow**

```c
#include "esp_dlfcn.h"
#include <stdio.h>

typedef int (*lib_func2)(int);

void run_plugin(void)
{
    void *handle = dlopen("/riscv/lib.so", RTLD_NOW);
    if (!handle) {
        printf("dlopen failed: %s\n", dlerror());
        return;
    }

    lib_func2 fibonacci_function = (lib_func2)dlsym(handle, "fibonacci");
    if (!fibonacci_function) {
        printf("dlsym failed: %s\n", dlerror());
        dlclose(handle);
        return;
    }

    printf("Fibonacci number at position %d is %d\n", 10, fibonacci_function(10));
    dlclose(handle);
}
```

#### ELF APP

To use this feature to compile ELF file, including the required CMake file in your project's CMakeLists.txt file after the line project(XXXX).

```cmake
project(XXXX)

# Add
include(elf_loader)
project_elf(XXXX)
```

Build the project as an ordinary ESP-IDF project, and then the ELF file named `XXXX.app.elf` is in the build directory.

#### Shared library

To use this feature to compile Shared library file, including the required CMake file in your project's CMakeLists.txt file after the line project(XXXX).

```cmake
project(XXXX)

# Add
include(elf_loader)
project_so(XXXX)
```

Build the project as an ordinary ESP-IDF project, and then the Shared library file named `XXXX.so` is in the build directory.

### ELF APP Fast Build

Users can enable ELF fast build functionality by configuring CMAKE's generator as Unix Makefiles. The reference command is as follows:

```bash
idf.py -G 'Unix Makefiles' set-target <chip-name>
```

Then input the ELF APP build command as follows:

```
idf.py elf
```

The build system will only build ELF target components and show the following logs:

```
Building C object esp-idf/main/CMakeFiles/__idf_main.dir/main.c.obj
Linking C static library libmain.a
Build ELF: hello_world.app.elf
Built target elf
```

### Shared library Fast Build

Users can enable shared library fast build functionality by configuring CMAKE's generator as Unix Makefiles. The reference command is as follows:

```bash
idf.py -G 'Unix Makefiles' set-target <chip-name>
```

Then input the shared library build command as follows:

```
idf.py so
```

The build system will only build shared library target components and show the following logs:

```
[100%] Building C object esp-idf/main/CMakeFiles/__idf_main.dir/lib.c.obj
[100%] Linking C static library libmain.a
[100%] Build Shared Object: lib.so
Linking lib.so completed
[100%] Built target so
```

### C++ ELF startup and shutdown

An ELF application built with `project_elf()` can contain C++ static/global
objects. After relocation, `esp_elf_request()` invokes `.preinit_array` and
`.init_array` once before calling the entry point. On the legacy Xtensa layout,
`.ctors` executes in reverse order. `esp_elf_deinit()` runs `.fini_array` in
reverse order (or legacy `.dtors` in forward order) before freeing the image,
but only if initialization ran. Calling the ELF entry point repeatedly does not
repeat static construction. These callbacks run in the calling task.

**C++ runtime dependencies are not bundled into the loader.** The
`project_elf()` link uses `-nostdlib`: unresolved C++ runtime entry points
(e.g. `operator new/delete` and `__cxa_pure_virtual`) must be exported by the
host using `esp_elf_register_symbol()` or supplied by the module. A global
object with a destructor requires a definition of `__dso_handle` and a
module-local implementation of `__cxa_atexit`/finalization, or equivalent
host-provided support; the loader only invokes the finalization callbacks in
`.fini_array`, it does **not** automatically call `__cxa_finalize` itself.
See `components/elf_loader/tests/idf_cpp_lifecycle` for a freestanding module
which defines these runtime hooks and tests actual static-object construction.
C++ exceptions and RTTI are not covered by this loader API.

This lifecycle support applies to ELF applications invoked with
`esp_elf_request()`. A `.so` loaded only by `dlopen()` has no entry invocation;
its static constructors are not run automatically by `dlopen()`.

### Xtensa PLT literal pools

For shared objects containing several PLT imports, `.got.loc` holds literals
used by PC-relative `l32r` instructions. The build rules retain this section.
On Xtensa the loader places it in executable memory at the same virtual offset
from `.text` as in the ELF file, while mapping intervening `.rodata` separately
to data memory. This may use more executable memory when the two sections are
far apart. Modules with malformed or overlapping PLT literal ranges are
rejected. Host regressions cover three literal entries and relocation, and
can cross-compile a real multi-PLT shared object when the ESP32 Xtensa toolchain
is installed. ESP32-S2 MMU mappings include the full executable literal span.

Module-local weak/defined symbols are resolved from the loaded image when the
host has no matching exported symbol. This fallback is available with
`CONFIG_ELF_DYNAMIC_LOAD_SHARED_OBJECT` both enabled and disabled.

### Exporting C++ functions to ELF modules

`ESP_ELFSYM_EXPORT(name)` stringifies its argument; a C++ qualified function
name is **not** the mangled name emitted into an ELF module. Use
`ESP_ELFSYM_EXPORT_NAMED("linker_name", qualified_function)` to register the
actual symbol name. Obtain the exact name from the target-toolchain `nm`
(`riscv32-esp-elf-nm` or `xtensa-esp32s3-elf-nm`) rather than guessing. The
same macro can also give C exports custom names.

For overloaded functions, use the C++-only macro
`ESP_ELFSYM_EXPORT_CPP_OVERLOAD("linker_name", signature, qualified_function)`;
`signature` must be a function-pointer typedef or `using` alias, so the
compiler can select one overload unambiguously:

```cpp
namespace demo {
int compute(int);
int compute(double);
}
using ComputeInt = int (*)(int);
using ComputeDouble = int (*)(double);

const struct esp_elfsym application_symbols[] = {
    ESP_ELFSYM_EXPORT_CPP_OVERLOAD("_ZN4demo7computeEi", ComputeInt, demo::compute),
    ESP_ELFSYM_EXPORT_CPP_OVERLOAD("_ZN4demo7computeEd", ComputeDouble, demo::compute),
    ESP_ELFSYM_END,
};
```

Pass this null-terminated table to `esp_elf_register_symbol()` after the host
has linked the actual C++ functions into its firmware. Names and calling
conventions depend on the target compiler ABI; build host and module with a
compatible Xtensa/RISC-V toolchain and ABI. Public/static class methods with
normal function pointers are eligible, but **non-static C++ member functions,
C++ object layouts, RTTI, exceptions and cross-module ownership are not
implicitly made ABI-safe** by these export macros. Prefer an explicit stable C
ABI for interfaces that must work across toolchain versions.

### Resolving logging and other host symbols

ELF modules using `ESP_LOGx()` may need the libc formatting function `vprintf`.
It is included in the default libc export table when
`CONFIG_ELF_LOADER_LIBC_SYMBOLS=y`. Other functions imported by an ELF module
must also be present in the host's symbol resolver. Build the ELF with the
required `ELF_COMPONENTS` and generate an export table using the documented
`components/elf_loader/tool/symbols.py` workflow and
`CONFIG_ELF_LOADER_CUSTOMER_SYMBOLS=y`, or register application-owned symbols
with `esp_elf_register_symbol()`. The host firmware must actually link each
exported function: adding a symbol name cannot provide missing implementation.

Do not indiscriminately add internal FreeRTOS or ESP-IDF functions such as
`esp_crosscore_int_send_yield` to the public libc table. Internal functions can
change across ESP-IDF versions and should be exported only when the application's
specific ABI and dependencies require them.

### Adding the Component to Your Project

Please use the component manager command add-dependency to add the elf_loader component as a dependency in your project. During the CMake step, the component will be downloaded automatically.

```
idf.py add-dependency "espressif/elf_loader=*"
```

### Examples

Please use the component manager command create-project-from-example to create a project from the example template.

```
idf.py create-project-from-example "espressif/elf_loader=*:elf_loader_example"
```

This command will download the example elf_loader_example into the current folder. You can navigate into it to build and flash the example.

Alternatively, you can download examples from the esp-iot-solution repository:
1. [build_elf_file_example](https://github.com/espressif/esp-iot-solution/tree/master/examples/elf_loader/build_elf_file_example)
2. [build_shared_library_example](https://github.com/espressif/esp-iot-solution/tree/master/examples/elf_loader/build_shared_library_example)
3. [elf_embed_example](https://github.com/espressif/esp-iot-solution/tree/master/examples/elf_loader/elf_embed_example)
4. [elf_loader_example](https://github.com/espressif/esp-iot-solution/tree/master/examples/elf_loader/elf_loader_example)
5. [elf_console_example](https://github.com/espressif/esp-iot-solution/tree/master/examples/elf_loader/elf_console_example)
