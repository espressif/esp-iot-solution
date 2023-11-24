idf_component_register(SRC_DIRS "."
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "${IDF_PATH}/components/usb/private_include"
                       REQUIRES usb)

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
