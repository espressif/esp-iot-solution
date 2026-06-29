set(srcs "main.c")
set(include_dirs ".")

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS ${include_dirs}
    REQUIRES hw_init
    PRIV_REQUIRES esp_pm esp_timer
)

set(_esp_sleep_header "${IDF_PATH}/components/esp_hw_support/include/esp_sleep.h")
if(EXISTS "${_esp_sleep_header}")
    file(READ "${_esp_sleep_header}" _esp_sleep_content)
    string(FIND "${_esp_sleep_content}" "esp_sleep_get_wakeup_causes" _wakeup_causes_pos)
    if(NOT _wakeup_causes_pos EQUAL -1)
        target_compile_definitions(${COMPONENT_LIB} PRIVATE EXAMPLE_HAS_ESP_SLEEP_GET_WAKEUP_CAUSES=1)
    endif()
endif()
