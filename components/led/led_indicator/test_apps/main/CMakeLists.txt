idf_component_register(SRC_DIRS "."
                       INCLUDE_DIRS ".")

list(APPEND EXTRA_LINK_FLAGS "-u newlib_include_assert_impl")
target_link_libraries(${COMPONENT_LIB} INTERFACE "${EXTRA_LINK_FLAGS}")
