# QuickJS-NG Example

This example demonstrates how to run [QuickJS-NG](https://github.com/quickjs-ng/quickjs) on ESP-IDF.

The embedded JavaScript file is evaluated as an ES module. It imports the QuickJS `std` and `os` modules, checks that `os.platform` reports `esp`, calls selected `os` APIs, and runs the final print from a Promise job so the example also validates pending job handling.

## Hardware Required

Any ESP development board supported by the `quickjs-ng` component can be used. The example has been verified on `esp32c5`.

## Build and Flash

Set up the ESP-IDF environment first:

```bash
. $IDF_PATH/export.sh
```

Set the target chip, build the project, flash it to the board, and open the serial monitor from the example project directory:

```bash
idf.py set-target esp32c5
idf.py build
idf.py -p PORT flash monitor
```

Replace `esp32c5` with your target chip and `PORT` with your serial port, for example `/dev/ttyUSB0`.

To exit the serial monitor, type `Ctrl-]`.

## Example Output

After boot, the serial monitor should include output similar to:

```text
hello from esp: cwd=/, realpath=unavailable
I (...) qjs_example: QuickJS hello finished
```

`realpath=unavailable` is acceptable on targets where `os.realpath('.')` is not available for the current filesystem path.

## How It Works

The example embeds `main/hello_world.js` with `EMBED_TXTFILES` and evaluates it from `main/qjs_main.c`.

At runtime, the example:

- Creates a QuickJS runtime and context.
- Installs the module loader and registers the `std` and `os` modules.
- Evaluates the embedded script as an ES module.
- Drains pending QuickJS jobs and reports async failures through a promise rejection tracker.

## Troubleshooting

If the application hits a stack overflow, increase `CONFIG_ESP_MAIN_TASK_STACK_SIZE` in the project configuration.

If flashing fails, check that the board is connected, the serial port is correct, and the current user has permission to access the port.
