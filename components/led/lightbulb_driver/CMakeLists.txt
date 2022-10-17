set(srcs "src/hal_driver.c"
        "src/lightbulb.c"
        "src/lighting_test.c"
        )

set(priv_req nvs_flash)

set(incs "include")

if(CONFIG_ENABLE_PWM_DRIVER)
    list(APPEND srcs "drivers/pwm/pwm.c")
    list(APPEND incs "drivers/pwm")
endif()

if(CONFIG_ENABLE_SM2135E_DRIVER)
    list(APPEND srcs "drivers/sm2135e/sm2135e.c")
    list(APPEND incs "drivers/sm2135e")
    list(APPEND srcs "drivers/common/iic/iic.c")
    list(APPEND incs "drivers/common/iic/")
endif()

if(CONFIG_ENABLE_SM2135EH_DRIVER)
    list(APPEND srcs "drivers/sm2135eh/sm2135eh.c")
    list(APPEND incs "drivers/sm2135eh")
    list(APPEND srcs "drivers/common/iic/iic.c")
    list(APPEND incs "drivers/common/iic/")
endif()

if(CONFIG_ENABLE_SM2x35EGH_DRIVER)
    list(APPEND srcs "drivers/sm2x35egh/sm2x35egh.c")
    list(APPEND incs "drivers/sm2x35egh")
    list(APPEND srcs "drivers/common/iic/iic.c")
    list(APPEND incs "drivers/common/iic/")
endif()

if(CONFIG_ENABLE_BP5758D_DRIVER)
    list(APPEND srcs "drivers/bp5758d/bp5758d.c")
    list(APPEND incs "drivers/bp5758d")
    list(APPEND srcs "drivers/common/iic/iic.c")
    list(APPEND incs "drivers/common/iic/")
endif()

if(CONFIG_ENABLE_WS2812_DRIVER)
    list(APPEND srcs "drivers/ws2812/ws2812.c")
    list(APPEND incs "drivers/ws2812")
endif()

idf_component_register(SRCS ${srcs}
                        INCLUDE_DIRS ${incs}
                        PRIV_INCLUDE_DIRS "src/priv_include"
                        PRIV_REQUIRES ${priv_req}
                        )
