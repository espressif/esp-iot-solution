file(GLOB_RECURSE SRCS src/*.c)

set(priv_requires "esp32_s3_lcd_ev_board")

idf_component_register(
    SRCS ${SRCS}
    INCLUDE_DIRS "include"
    PRIV_REQUIRES ${priv_requires})
