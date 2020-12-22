cmake_minimum_required(VERSION 3.2)

# set the project name
project(cache)
enable_testing()

set(SOURCE_DIR ${CMAKE_SOURCE_DIR}/src)
set(LIBRARY_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/build/lib)

# C++14
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# General compiler flags
set(CXX_COMMON_FLAGS "-Wall")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CXX_COMMON_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ggdb -Werror")

#
# Out of source builds
#
set(DEFAULT_OUT_OF_SOURCE "build")
if (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
  message(FATAL_ERROR "In-source builds not allowed. Run cmake with:
    mkdir -p ${DEFAULT_OUT_OF_SOURCE} && cd ${DEFAULT_OUT_OF_SOURCE} && cmake -H.. -B.")
endif ()

#
# Third party dependencies
#
add_subdirectory(thirdparty)
set(LIBS ${LIBS} gflags_nothreads_static)
set(LIBS ${LIBS} gtest_main)

#
# Includes
#
include_directories(src)
include_directories(thirdparty/gtest/googletest/include)
include_directories(thirdparty/gflags/include)

#
# Subdirectories
#
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/build/exe)
add_subdirectory(src)

#
# Tests
#
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/build/tests)
add_subdirectory(test)

