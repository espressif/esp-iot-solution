idf_component_register(SRC_DIRS "."
                    INCLUDE_DIRS "." "include/"
                    REQUIRES "esp_mm" "esp_timer")

set_source_files_properties(
    "cv_detect.cpp"
    PROPERTIES
        COMPILE_OPTIONS
        "-Wno-deprecated-enum-enum-conversion"
    )

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
