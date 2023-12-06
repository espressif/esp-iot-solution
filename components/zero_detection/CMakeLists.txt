if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "5.0")
    idf_component_register(SRCS "zero_detection.c"
                        REQUIRES driver esp_timer
                        INCLUDE_DIRS "include")
else()
    message(FATAL_ERROR "Not supported in the current version")
endif()

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
