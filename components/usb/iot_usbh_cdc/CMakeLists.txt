idf_component_register(SRC_DIRS "."
                       INCLUDE_DIRS "." "include"
                       PRIV_INCLUDE_DIRS "private_include"
                       PRIV_REQUIRES esp_ringbuf
                       REQUIRES usb)

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
