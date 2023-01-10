
if(CONFIG_SOURCE_SIMULATE)
set(PIC_DIR "pictures/320240")
set(                    embed_files
                        ${PIC_DIR}/0001.jpg
                        ${PIC_DIR}/0002.jpg
                        ${PIC_DIR}/0003.jpg
                        ${PIC_DIR}/0004.jpg
                        ${PIC_DIR}/0005.jpg
                        ${PIC_DIR}/0006.jpg
                        ${PIC_DIR}/0007.jpg
                        ${PIC_DIR}/0008.jpg
                        ${PIC_DIR}/0009.jpg
                        ${PIC_DIR}/0010.jpg
                        ${PIC_DIR}/0011.jpg
                        ${PIC_DIR}/0012.jpg
                        ${PIC_DIR}/0013.jpg
                        ${PIC_DIR}/0014.jpg
                        ${PIC_DIR}/0015.jpg)
endif()

function(pkg_check_version file_path ver_major ver_minor ver_patch)
    set(file_name "${file_path}/idf_component.yml")
    if(EXISTS ${file_name})
        file(READ ${file_name} ver)
        string(REGEX MATCH "(^|\n)version: \"?([0-9]+).([0-9]+).([0-9]+)\"?" _ ${ver})
        set(${ver_major} ${CMAKE_MATCH_2} PARENT_SCOPE)
        set(${ver_minor} ${CMAKE_MATCH_3} PARENT_SCOPE)
        set(${ver_patch} ${CMAKE_MATCH_4} PARENT_SCOPE)
    else()
        message(WARNING " ${file_name} not exist")
    endif()
endfunction()

#ESP-IDF USB component HCD level API default to private now,
#to usb_stream, related API must manually set to public.
if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "5.0")
idf_component_register(SRCS usb_stream.c descriptor.c
                    INCLUDE_DIRS "include" "include/deprecated"
                    PRIV_INCLUDE_DIRS "${IDF_PATH}/components/usb/private_include" "private_include"
                    EMBED_FILES ${embed_files}
                    REQUIRES usb esp_ringbuf)
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-address-of-packed-member")
else()
idf_component_register(SRCS usb_stream.c descriptor.c
                    INCLUDE_DIRS "include" "include/deprecated"
                    PRIV_INCLUDE_DIRS "${IDF_PATH}/components/usb/private_include" "private_include"
                    EMBED_FILES ${embed_files}
                    REQUIRES usb )
endif()

pkg_check_version(${CMAKE_CURRENT_SOURCE_DIR} ver_major ver_minor ver_patch)
message(STATUS "usb_stream: ${ver_major}.${ver_minor}.${ver_patch}")

target_compile_options(${COMPONENT_LIB} PUBLIC
    -DUSB_STREAM_VER_MAJOR=${ver_major} -DUSB_STREAM_VER_MINOR=${ver_minor} -DUSB_STREAM_VER_PATCH=${ver_patch})