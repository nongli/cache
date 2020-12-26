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
set(CXX_COMMON_FLAGS "-Wall -Wno-sign-compare -Werror=switch -Werror=return-type")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CXX_COMMON_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ggdb -Werror -fno-omit-frame-pointer -fsanitize=address,undefined")
set(CMAKE_EXE_LINKER_FLAGS "-fsanitize=address,undefined")

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
add_subdirectory(thirdparty/gflags)
add_subdirectory(thirdparty/gtest)
set(LIBS ${LIBS} gflags::gflags)
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

set(LIBS ${LIBS} Cache)
set(LIBS ${LIBS} Util)

#
# Tests
#
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/build/tests)
add_subdirectory(test)

