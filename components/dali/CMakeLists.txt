if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "5.5")
    set(REQ esp_driver_rmt)
else()
    set(REQ driver)
endif()

idf_component_register(SRCS "src/dali.c"
                        INCLUDE_DIRS "include"
                        REQUIRES ${REQ})

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
