set(srcs
    src/power_measure_api.c
    src/power_measure_bl0937.c
    src/power_measure_ina236.c
    src/power_measure_bl0942.c
    drivers/ina236/ina236.c
    drivers/bl0937/bl0937.c
    drivers/bl0942/bl0942.c
)

set(incs "include")

set(priv_incs
    "drivers/bl0937"
    "drivers/ina236"
    "drivers/bl0942"
)

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${incs}
                       PRIV_INCLUDE_DIRS ${priv_incs}
                       REQUIRES esp_timer driver i2c_bus
                       )

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
