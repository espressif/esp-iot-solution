# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

# Overwrite the IDF_PATH to the esp-idf path in submodule.
set(ENV{IDF_PATH} "$ENV{IOT_SOLUTION_PATH}/submodule/esp-idf/")

include($ENV{IOT_SOLUTION_PATH}/components/CMakeLists.txt)