if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "5.5")
    set(REQ esp_driver_rmt)
else()
    set(REQ driver)
endif()

# Part 101 (dali_system_components.c) is always compiled.
set(DALI_SRCS "src/dali_system_components.c")

if(CONFIG_DALI_PART102_ENABLED)
    list(APPEND DALI_SRCS "src/dali_control_gear.c")
endif()

if(CONFIG_DALI_PART209_ENABLED)
    list(APPEND DALI_SRCS "src/dali_color_control_dt8.c")
endif()

if(CONFIG_DALI_PART103_ENABLED)
    list(APPEND DALI_SRCS "src/dali_control_device.c")
endif()

if(CONFIG_DALI_PART303_304_ENABLED)
    list(APPEND DALI_SRCS "src/dali_device_sensors.c")
endif()

idf_component_register(SRCS ${DALI_SRCS}
                        INCLUDE_DIRS "include"
                        REQUIRES ${REQ})

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
