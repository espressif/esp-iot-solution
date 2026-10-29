# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(BMGR_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/components/gen_bmgr_codes/board_manager.defaults")
if(EXISTS "${BMGR_DEFAULTS}")
    list(FIND SDKCONFIG_DEFAULTS "${BMGR_DEFAULTS}" BMGR_DEFAULTS_INDEX)
    if(BMGR_DEFAULTS_INDEX EQUAL -1)
        if(SDKCONFIG_DEFAULTS)
            # Keep CI overrides after the board-specific defaults.
            list(INSERT SDKCONFIG_DEFAULTS 1 "${BMGR_DEFAULTS}")
        else()
            set(SDKCONFIG_DEFAULTS
                "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults;${BMGR_DEFAULTS}"
            )
        endif()
    endif()
endif()

add_compile_options(-Wno-ignored-qualifiers)
project(usb_touch_screen)
