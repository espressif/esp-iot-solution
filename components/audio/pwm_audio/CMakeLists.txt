idf_component_register(SRCS "pwm_audio.c"
                       INCLUDE_DIRS "include"
                       REQUIRES driver)

include(cmake_utilities)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})   