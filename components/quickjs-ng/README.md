## Description

[![Component Registry](https://components.espressif.com/components/espressif/quickjs-ng/badge.svg)](https://components.espressif.com/components/espressif/quickjs-ng)

QuickJS-NG is a small JavaScript engine. This component provides an ESP-IDF port of QuickJS-NG so applications can create a JavaScript runtime, evaluate scripts, and use the QuickJS C API from ESP-IDF projects.

The component vendors QuickJS-NG v0.14.0 and adds an ESP-IDF port layer for newlib compatibility and unsupported host operating system APIs.

### Add Component to Your Project

Please use the component manager command `add-dependency` to add `quickjs-ng` to your project. During the CMake step, the component will be downloaded automatically.

```bash
idf.py add-dependency "espressif/quickjs-ng=*"
```

Then include QuickJS headers from your application component:

```c
#include "quickjs.h"
#include "quickjs-libc.h"
```

### Basic Usage

Create a runtime and context, evaluate JavaScript, and release QuickJS resources when finished:

```c
JSRuntime *rt = JS_NewRuntime();
JSContext *ctx = JS_NewContext(rt);

JSValue ret = JS_Eval(ctx, "1 + 2", 5, "<input>", JS_EVAL_TYPE_GLOBAL);
if (JS_IsException(ret)) {
    js_std_dump_error(ctx);
}
JS_FreeValue(ctx, ret);

JS_FreeContext(ctx);
JS_FreeRuntime(rt);
```

For ES modules and the `std` / `os` modules, install the module loader and register the modules before evaluating the script:

```c
js_std_init_handlers(rt);
JS_SetModuleLoaderFunc2(rt, NULL, js_module_loader, js_module_check_attributes, NULL);

js_init_module_std(ctx, "std");
js_init_module_os(ctx, "os");

JSValue ret = JS_Eval(ctx, script, script_len, "script.js", JS_EVAL_TYPE_MODULE);
```

### Examples

Please use the component manager command `create-project-from-example` to create a project from the example template:

```bash
idf.py create-project-from-example "espressif/quickjs-ng=*:quickjs-ng"
```

The example embeds a JavaScript file with `EMBED_TXTFILES`, evaluates it as an ES module, imports `std` and `os`, and drains pending QuickJS jobs. After the example is created, build and flash it as a normal ESP-IDF project.

### Notes and Limitations

- APIs that depend on unsupported host OS features are unavailable or return errors on ESP targets.
- Dynamic loading and process-control APIs such as `dlopen`, `popen`, and `exec` are not supported.
- `os.Worker` is disabled on ESP targets because the upstream worker wakeup path depends on `pipe()`.
- Some filesystem-related `os` APIs depend on the filesystem mounted by the application.

### Upstream Project

This component vendors the QuickJS-NG source from:

https://github.com/quickjs-ng/quickjs

The vendored upstream source is kept under `quickjs-ng/`.

### License

The vendored QuickJS-NG source is licensed under the MIT license. The ESP-IDF port files are licensed under Apache-2.0.
