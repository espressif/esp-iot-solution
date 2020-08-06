
if(NOT CONFIG_UGFX_GUI_ENABLE)
    set(srcs "")
    set(inc_dirs "")
    message(STATUS "Building empty ugfx component due to configuration")
else()
    set(srcs "mk.c"
                "gos_freertos_priv.c")

    set(inc_dirs "."
                "include"
                "ugfx"
                "ugfx/src"
                "ugfx/src/gadc"
                "ugfx/src/gaudio"
                "ugfx/src/gdisp"
                "ugfx/src/gdriver"
                "ugfx/src/gevent"
                "ugfx/src/gfile"
                "ugfx/src/ginput"
                "ugfx/src/gmisc"
                "ugfx/src/gos"
                "ugfx/src/gqueue"
                "ugfx/src/gtimer"
                "ugfx/src/gtrans"
                "ugfx/src/gwin")


    set(priv_incdirs ${COMPONENT_PRIV_INCLUDEDIRS} ${CMAKE_SOURCE_DIR}/${CONFIG_UGFX_PROJ_RESOURCE_PATH})

endif()


idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "${inc_dirs}"
                    PRIV_INCLUDE_DIRS "${priv_incdirs}"
                    REQUIRES gdrivers param)

# In order to reduce the warning, turn off all warnings of UGFX
target_compile_options(${COMPONENT_LIB} INTERFACE -w)