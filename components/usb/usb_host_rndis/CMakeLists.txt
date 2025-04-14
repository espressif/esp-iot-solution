idf_component_register(SRCS "usb_host_rndis.c" "usb_host_rndis_descriptor.c"
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "priv_include"
                       REQUIRES esp_netif esp_ringbuf
                       )

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
