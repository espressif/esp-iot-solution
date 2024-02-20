# XZ decompress utility

[![Component Registry](https://components.espressif.com/components/espressif/xz/badge.svg)](https://components.espressif.com/components/espressif/xz)

This component provides the XZ decompress utility based on [XZ Embedded](https://tukaani.org/xz/embedded.html). API `xz_decompress` is public to use, for more info about this API, please refer to [link](https://github.com/espressif/esp-iot-solution/tree/master/components/utilities/xz/include/xz_decompress.h).

## Add to project

You can add this component to your project via `idf.py add-dependancy`, e.g.

```shell
idf.py add-dependency xz==1.0.0
```

Alternatively, you can create the manifest idf_component.yml in the project's main component directory.

```yml
dependencies:
  espressif/xz: "^1.0.0"
```

More is in [Espressif's documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html).

## Example

- [examples/utilities/xz_decompress_file](https://github.com/espressif/esp-iot-solution/tree/master/examples/utilities/xz_decompress_file): This example is based on this component.

You can create a project from this example by the following command:

```shell
idf.py create-project-from-example "espressif/xz^1.0.0:xz_decompress_file"
```
