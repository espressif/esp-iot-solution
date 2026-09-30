cmake_minimum_required(VERSION 3.16)

# In IDF >= 5.3 the UART driver was split into its own component 'esp_driver_uart'.
# Older IDF versions still bundle it inside the generic 'driver' component.
if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "5.3")
    set(UART_DEP esp_driver_uart)
else()
    set(UART_DEP driver)
endif()

idf_component_register(
    SRCS
        "src/esp_xmodem_sender.c"
        "src/esp_xmodem_receiver.c"
        "src/esp_xmodem_transport.c"
        "src/esp_xmodem_common.c"
    INCLUDE_DIRS
        "include"
    PRIV_INCLUDE_DIRS
        "private_include"
    REQUIRES
        # Nothing in the public headers requires additional components beyond
        # what ESP-IDF automatically links (esp_common for esp_err.h, etc.)
    PRIV_REQUIRES
        ${UART_DEP}
)

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
