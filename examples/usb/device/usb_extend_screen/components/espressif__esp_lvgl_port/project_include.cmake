# lvgl_port_create_c_image
#
# Create a C array of image for using with LVGL
function(lvgl_port_create_c_image image_path output_path color_format compression)

    #Get Python
    idf_build_get_property(python PYTHON)

    #Get LVGL version
    idf_build_get_property(build_components BUILD_COMPONENTS)
    if(lvgl IN_LIST build_components)
        set(lvgl_name lvgl) # Local component
        set(lvgl_ver $ENV{LVGL_VERSION}) # Get the version from env variable (set from LVGL v9.2)
    else()
        set(lvgl_name lvgl__lvgl) # Managed component
        idf_component_get_property(lvgl_ver ${lvgl_name} COMPONENT_VERSION) # Get the version from esp-idf build system
    endif()

    if("${lvgl_ver}" STREQUAL "")
        message("Could not determine LVGL version, assuming v9.x")
        set(lvgl_ver "9.0.0")
    endif()

    get_filename_component(image_full_path ${image_path} ABSOLUTE)
    get_filename_component(output_full_path ${output_path} ABSOLUTE)
    if(NOT EXISTS ${image_full_path})
        message(FATAL_ERROR "Input image (${image_full_path}) not exists!")
    endif()

    message(STATUS "Generating C array image: ${image_path}")

    #Create C array image by LVGL version
    if(lvgl_ver VERSION_LESS "9.0.0")

        if(CONFIG_LV_COLOR_16_SWAP)
            set(color_format "RGB565SWAP")
        else()
            set(color_format "RGB565")
        endif()

        execute_process(COMMAND git clone https://github.com/W-Mai/lvgl_image_converter.git "${CMAKE_BINARY_DIR}/lvgl_image_converter")
        execute_process(COMMAND git checkout 9174634e9dcc1b21a63668969406897aad650f35 WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/lvgl_image_converter" OUTPUT_QUIET)
        execute_process(COMMAND ${python} -m pip install --upgrade pip)
        execute_process(COMMAND ${python} -m pip install pillow==10.3.0)
        #execute_process(COMMAND python -m pip install -r "${CMAKE_BINARY_DIR}/lvgl_image_converter/requirements.txt")
        execute_process(COMMAND ${python} "${CMAKE_BINARY_DIR}/lvgl_image_converter/lv_img_conv.py"
                -ff C
                -f true_color_alpha
                -cf ${color_format}
                -o ${output_full_path}
                ${image_full_path})
    else()
        idf_component_get_property(lvgl_dir ${lvgl_name} COMPONENT_DIR)
        get_filename_component(script_path ${lvgl_dir}/scripts/LVGLImage.py ABSOLUTE)
        set(lvglimage_py ${python} ${script_path})

        #Install dependencies
        execute_process(COMMAND ${python} -m pip install pypng lz4 OUTPUT_QUIET)

        execute_process(COMMAND ${lvglimage_py}
                --ofmt=C
                --cf=${color_format}
                --compress=${compression}
                -o ${output_full_path}
                ${image_full_path})
    endif()

endfunction()

# lvgl_port_add_images
#
# Add all images to build
function(lvgl_port_add_images component output_path)
    #Add images to sources
    file(GLOB_RECURSE IMAGE_SOURCES ${output_path}*.c)
    target_sources(${component} PRIVATE ${IMAGE_SOURCES})
    idf_build_set_property(COMPILE_OPTIONS "-DLV_LVGL_H_INCLUDE_SIMPLE=1" APPEND)
endfunction()
