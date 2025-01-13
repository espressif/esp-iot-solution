idf_component_register(
    SRCS "main.c" "usb_descriptors.c"
    INCLUDE_DIRS "."
    PRIV_INCLUDE_DIRS "."
)

# Determine whether tinyusb is fetched from component registry or from local path
idf_build_get_property(build_components BUILD_COMPONENTS)
if(tinyusb IN_LIST build_components)
    set(tinyusb_name tinyusb) # Local component
else()
    if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "5.0")
        set(tinyusb_name espressif__tinyusb) # Managed component
    else ()
        set(tinyusb_name leeebo__tinyusb_src) # Managed component
    endif()
endif()

# Pass tusb_config.h to the example
idf_component_get_property(tusb_lib ${tinyusb_name} COMPONENT_LIB)
target_include_directories(${tusb_lib} INTERFACE ".")
target_link_libraries(${tusb_lib} INTERFACE ${COMPONENT_LIB})

cmake_policy(SET CMP0079 NEW)
