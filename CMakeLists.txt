cmake_minimum_required(VERSION 3.2)

# set the project name
project(cache)

# C++11
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# add the executable
add_executable(build-test src/build-test.cc)
