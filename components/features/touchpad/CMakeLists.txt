set(srcs "touchpad_obj.cpp" "touchpad.c")
set(incs "include")

if(CONFIG_DATA_SCOPE_DEBUG)
    set(srcs "${srcs}" "scope_debug/touch_tune_tool.c")
    set(incs "${incs}" "scope_debug")
endif()
idf_component_register(SRCS "${srcs}"
                        INCLUDE_DIRS "${incs}"
                        PRIV_REQUIRES nvs_flash)