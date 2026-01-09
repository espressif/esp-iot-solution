# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(elf_example)

# Set other components to link to the ELF file
# e.g: set(ELF_COMPONENTS "log" "esp_wifi")

include(elf_loader)
project_elf(elf_example)
