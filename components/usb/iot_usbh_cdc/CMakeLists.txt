idf_component_register(SRC_DIRS "."
                       INCLUDE_DIRS "include"
                       REQUIRES iot_usbh
                       PRIV_REQUIRES esp_ringbuf)

include(cmake_utilities)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})