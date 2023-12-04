#ESP-IDF USB component HCD level API default to private now,
#to usb_stream, related API must manually set to public.
if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "5.0")
idf_component_register(SRCS usb_stream.c descriptor.c usb_host_helpers.c
                    INCLUDE_DIRS "include"
                    PRIV_INCLUDE_DIRS "${IDF_PATH}/components/usb/private_include" "private_include"
                    REQUIRES usb esp_ringbuf)
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-address-of-packed-member")
else()
idf_component_register(SRCS usb_stream.c descriptor.c usb_host_helpers.c
                    INCLUDE_DIRS "include"
                    PRIV_INCLUDE_DIRS "${IDF_PATH}/components/usb/private_include" "private_include"
                    REQUIRES usb)
endif()

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
