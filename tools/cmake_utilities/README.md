# Cmake utilities

This component is aiming to provide some useful CMake utilities outside of ESP-IDF.

## Use

1. Add dependency of this component in your component or project's idf_component.yml.

    ```yml
    dependencies:
      espressif/cmake_utilities: "0.*"
    ```

2. Include the CMake file you needed in your component's CMakeLists.txt after idf_component_register.

    ```cmake
    // Note: should remove .cmake postfix when using include(), otherwise the requested file will not found
    // Note: should place this line after `idf_component_register` function
    // Include the top CMake file which will include other CMake files
    include(cmake_utilities)

    // Or, only include the one you needed.
    include(package_manager)
    ```

3. Then you can use the corresponding CMake function which is provided by the CMake file.