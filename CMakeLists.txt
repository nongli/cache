cmake_minimum_required(VERSION 3.2)

# set the project name
project(cache)

set(SOURCE_DIR ${CMAKE_SOURCE_DIR}/src)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/build/exe)
set(LIBRARY_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/build/lib)

# C++14
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Out of source builds
set(DEFAULT_OUT_OF_SOURCE "build")
if (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
  message(FATAL_ERROR "In-source builds not allowed. Run cmake with:
    mkdir -p ${DEFAULT_OUT_OF_SOURCE} && cd ${DEFAULT_OUT_OF_SOURCE} && cmake -H.. -B.")
endif ()

include_directories(src)
add_subdirectory(src)

# add the executable
add_executable(build-test src/build-test.cc)
