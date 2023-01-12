# cu_get_pkg_version
#
# @brief Get the package's version information, the package is installed by component manager.
#
# @param[in] pkg_path the package's path, normally it's ${CMAKE_CURRENT_SOURCE_DIR}.
#
# @param[out] ver_major the major version of the package
# @param[out] ver_minor the minor version of the package
# @param[out] ver_patch the patch version of the package
function(cu_get_pkg_version pkg_path ver_major ver_minor ver_patch)
    set(yml_file "${pkg_path}/idf_component.yml")
    if(EXISTS ${yml_file})
        file(READ ${yml_file} ver)
        string(REGEX MATCH "(^|\n)version: \"?([0-9]+).([0-9]+).([0-9]+)\"?" _ ${ver})
        set(${ver_major} ${CMAKE_MATCH_2} PARENT_SCOPE)
        set(${ver_minor} ${CMAKE_MATCH_3} PARENT_SCOPE)
        set(${ver_patch} ${CMAKE_MATCH_4} PARENT_SCOPE)
    else()
        message(WARNING " ${yml_file} not exist")
    endif()
endfunction()