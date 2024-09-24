CMake Utilities
===================

:link_to_translation:`en:[English]`

``cmake_utilities`` 是 ``esp-iot-solution`` 组件中常用的 CMake 工具集合。

支持的功能：

- ``project_include.cmake``: 为项目添加额外的功能，例如 ``DIAGNOSTICS_COLOR``。文件将被自动解析，详情请参考 `project-include-cmake <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/build-system.html#project-include-cmake>`__。
- ``package_manager.cmake``: 提供了管理组件版本等功能。
- ``gcc.cmake``: 通过 menuconfig 管理 GCC 编译器选项，例如 ``LTO``。
- ``relinker.cmake`` 提供了一种将 IRAM 函数移动到 Flash 以节省 RAM 空间的方法。
- ``gen_compressed_ota.cmake``: 添加了新命令 ``idf.py gen_compressed_ota`` 以生成 ``xz`` 压缩的 OTA 二进制文件。详情请参考 `xz <https://github.com/espressif/esp-iot-solution/tree/master/components/utilities/xz>`__.
- ``gen_single_bin.cmake``: 添加了新命令 ``idf.py gen_single_bin`` 以生成单个组合 bin 文件（组合 app、bootloader、分区表等）。

如何使用
----------

1. 在你的组件或项目的 ``idf_component.yml`` 中添加此组件的依赖。

   .. code:: yaml

      dependencies:
        espressif/cmake_utilities: "0.*"

2. 在你的组件或项目的 ``CMakeLists.txt`` 中的 ``idf_component_register`` 后包含 CMake 功能/脚本。

   .. code:: none

      // 注意：使用 include() 时应删除 .cmake 后缀，否则将找不到请求的文件
      // 注意：应在 `idf_component_register` 函数之后放置此行
      // 只包含你需要的功能。
      include(package_manager)

3. 然后你可以使用 CMake 脚本提供的相应功能。

详细参考
---------------------

1. `relinker <https://github.com/espressif/esp-iot-solution/blob/master/tools/cmake_utilities/docs/relinker.md>`__
2. `gen_compressed_ota <https://github.com/espressif/esp-iot-solution/blob/master/tools/cmake_utilities/docs/gen_compressed_ota.md>`__
3. `GCC Optimization <https://github.com/espressif/esp-iot-solution/blob/master/tools/cmake_utilities/docs/gcc.md>`__
