cmake_minimum_required(VERSION 3.16)
# ESP-IDF <5.5 invokes Ninja directly and ignores IDF_PY_BUILD_JOBS. Limit
# the memory-heavy TFLite Micro C++ compilations on those releases as well.
set_property(GLOBAL PROPERTY JOB_POOLS imu_compile_pool=2)
set(CMAKE_JOB_POOL_COMPILE imu_compile_pool)

set(EXTRA_COMPONENT_DIRS
    ../..
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(imu_gesture_inference_example)
