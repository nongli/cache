cmake_minimum_required(VERSION 3.2)

# set the project name
project(cache)

set(SOURCE_DIR ${CMAKE_SOURCE_DIR}/src)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/build)
set(LIBRARY_OUTPUT_PATH ${EXECUTABLE_OUTPUT_PATH})

# C++11
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Out of source builds
set(DEFAULT_OUT_OF_SOURCE_FOLDER "build")
if (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
  message(FATAL_ERROR "In-source builds not allowed. Run cmake with:
    mkdir -p ${DEFAULT_OUT_OF_SOURCE_FOLDER} && cd build && cmake -H.. -B.")
endif ()

# add the executable
add_executable(build-test src/build-test.cc)
