# Cmake utilities

This component is aiming to provide some useful CMake utilities outside of ESP-IDF.

## Use

1. Add dependency of this component in your component or project's idf_component.yml.

    ```yml
    dependencies:
      espressif/cmake_utilities: "0.*"
    ```

2. Include the CMake file you needed in your component's CMakeLists.txt.

    ```cmake
    idf_component_get_property(CMAKE_UTILITIES_DIR espressif__cmake_utilities COMPONENT_DIR)

    // Include the top CMake file which will include other CMake files
    include(${CMAKE_UTILITIES_DIR}/cmake_utilities.cmake)

    // Or, only include the one you needed.
    include(${CMAKE_UTILITIES_DIR}/package_manager.cmake)
    ```

3. Then you can use the corresponding CMake function which is provided by the CMake file.