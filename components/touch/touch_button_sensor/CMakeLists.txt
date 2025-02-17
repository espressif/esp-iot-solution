idf_component_register(SRC_DIRS "."
                        INCLUDE_DIRS "include")

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/touch_button_sensor.c")
    add_prebuilt_library(touch_button_sensor_lib "${CMAKE_CURRENT_SOURCE_DIR}/${CONFIG_IDF_TARGET}/libtouch_button_sensor.a")
    target_include_directories(${COMPONENT_LIB} INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_link_libraries(${COMPONENT_LIB} INTERFACE touch_button_sensor_lib)
else()
    include(package_manager)
    cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
    target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-type-limits)
endif()
