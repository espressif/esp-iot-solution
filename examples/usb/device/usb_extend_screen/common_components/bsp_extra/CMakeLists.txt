file(GLOB_RECURSE SRCS src/*.c)

set(requires driver)
set(priv_requires "esp32_s3_lcd_ev_board")
if(${IDF_VERSION_MAJOR} GREATER_EQUAL 6)
    list(APPEND requires esp_driver_gpio esp_driver_i2s esp_driver_rmt esp_driver_spi)
endif()

idf_component_register(
    SRCS ${SRCS}
    INCLUDE_DIRS "include"
    REQUIRES ${requires}
    PRIV_REQUIRES ${priv_requires})
