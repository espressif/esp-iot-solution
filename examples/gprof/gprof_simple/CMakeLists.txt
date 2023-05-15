cmake_minimum_required(VERSION 3.10)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(EXTRA_COMPONENT_DIRS ${EXTRA_COMPONENT_DIRS}
                         $ENV{IOT_SOLUTION_PATH}/examples/gprof/gprof_simple/components/test_func)

project(gprof_simple)

include(gprof)
