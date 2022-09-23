
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

#ESP-IDF USB component HCD level API default to private now,
#to use uvc_stream, related API must manually set to public.
idf_component_register(SRCS uvc_stream.c descriptor.c
                    INCLUDE_DIRS "include"
                    PRIV_INCLUDE_DIRS "${IDF_PATH}/components/usb/private_include"
                    EMBED_FILES ${embed_files}
                    REQUIRES usb)
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")
