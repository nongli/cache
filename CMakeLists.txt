cmake_minimum_required(VERSION 3.2)

# set the project name
project(cache)
enable_testing()
option(PRINT_TRACE "Print trace when possible" OFF)

set(SOURCE_DIR ${CMAKE_SOURCE_DIR}/src)
set(LIBRARY_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/build/lib)

# C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# General compiler flags
set(CXX_COMMON_FLAGS "-Wall -Wno-sign-compare -Werror -Werror=switch -Werror=return-type")
set(CXX_COMMON_FLAGS "${CXX_COMMON_FLAGS} -fno-omit-frame-pointer -ggdb")
set(CXX_COMMON_FLAGS "${CXX_COMMON_FLAGS} -march=haswell")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CXX_COMMON_FLAGS}")

# Configure build/release
if ("${CMAKE_BUILD_TYPE}" STREQUAL "RELEASE")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -DNDEBUG")
  # Messes up asserts
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=unused-variable")
  set(EXECUTABLE_OUTPUT_PREFIX "${CMAKE_SOURCE_DIR}/build/release")
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address,undefined")
  set(CMAKE_EXE_LINKER_FLAGS "-fsanitize=address,undefined")
  set(EXECUTABLE_OUTPUT_PREFIX "${CMAKE_SOURCE_DIR}/build/debug")
endif()


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
set(gflags_DIR ${CMAKE_SOURCE_DIR}/thirdparty/gflags)
add_subdirectory(thirdparty/gflags)
#add_subdirectory(thirdparty/glog)
add_subdirectory(thirdparty/gtest)
set(LIBS ${LIBS} gflags::gflags)
#set(LIBS ${LIBS} glog::glog)
set(LIBS ${LIBS} gtest_main)

#
# Includes
#
include_directories(src)
include_directories(thirdparty/gtest/googletest/include)
include_directories(thirdparty/gflags/include)
#include_directories(thirdparty/glog/src)

#
# Subdirectories
#
set(EXECUTABLE_OUTPUT_PATH ${EXECUTABLE_OUTPUT_PREFIX}/exe)
add_subdirectory(src)

set(LIBS ${LIBS} Util)

#
# Tests
#
set(EXECUTABLE_OUTPUT_PATH ${EXECUTABLE_OUTPUT_PREFIX}/tests)
add_subdirectory(test)

#
# Examples
#
set(EXECUTABLE_OUTPUT_PATH ${EXECUTABLE_OUTPUT_PREFIX}/examples)
add_executable(example examples/example.cc)
