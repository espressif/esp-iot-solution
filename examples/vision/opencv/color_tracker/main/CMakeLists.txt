idf_component_register(SRC_DIRS "."
                    INCLUDE_DIRS "." "include/"
                    REQUIRES "esp_mm" "esp_timer")

set_source_files_properties(
    "cv_detect.cpp"
    PROPERTIES
        COMPILE_OPTIONS
        "-Wno-deprecated-enum-enum-conversion"
    )

set_source_files_properties(
    "lcd.cpp"
    PROPERTIES
        COMPILE_OPTIONS
        "-Wno-missing-field-initializers"
    )

idf_component_get_property(esp_video_component_lib espressif__esp_video COMPONENT_LIB)
target_compile_options(${esp_video_component_lib} PRIVATE "-Wno-deprecated-declarations")

set(MODIFY_EXE ${PROJECT_DIR}/modify_opencv.py)
set(OPENCV_CMAKE_TARGET "${PROJECT_DIR}/managed_components/espressif__opencv/CMakeLists.txt")
set(OPENCV_CMAKE_SOURCE "CMakeLists.txt.opencv")

add_custom_command(
    OUTPUT ${OPENCV_CMAKE_TARGET}
    COMMENT "Updating OpenCV CMakeLists.txt using modify_opencv.py"
    COMMAND python ${MODIFY_EXE}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEPENDS ${OPENCV_CMAKE_SOURCE}
    VERBATIM)

add_custom_target(update_opencv ALL DEPENDS ${OPENCV_CMAKE_TARGET})
add_dependencies(app update_opencv)
