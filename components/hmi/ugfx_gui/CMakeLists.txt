
if(NOT CONFIG_UGFX_GUI_ENABLE)
    set(COMPONENT_SRCS "")
    set(COMPONENT_ADD_INCLUDEDIRS "")
    message(STATUS "Building empty ugfx component due to configuration")
else()
    set(COMPONENT_SRCS "mk.c"
                        "gos_freertos_priv.c")

    set(COMPONENT_ADD_INCLUDEDIRS "."
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

    set(COMPONENT_PRIV_INCLUDEDIRS "."
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

    # if(CONFIG_UGFX_USE_CUSTOM_DRIVER)    
    #     set(COMPONENT_REQUIRES $(call dequote,$(CONFIG_UGFX_CUSTOM_DRIVER_COMPONENT_NAME)))
    # else()
    #     set(COMPONENT_REQUIRES gdrivers)
    # endif()

    set(COMPONENT_PRIV_INCLUDEDIRS ${COMPONENT_PRIV_INCLUDEDIRS} ${PROJECT_PATH}/${CONFIG_UGFX_PROJ_RESOURCE_PATH})
    
endif()

# The values of COMPONENT_REQUIRES and COMPONENT_PRIV_REQUIRES should not 
# depend on any configuration choices (CONFIG_xxx macros). 
# This is because requirements are expanded before configuration is loaded. 
# Other component variables (like include paths or source files) can depend on configuration choices.
set(COMPONENT_REQUIRES gdrivers param)

register_component()
