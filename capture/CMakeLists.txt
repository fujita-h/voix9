cmake_minimum_required(VERSION 3.1)
project(Capture CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-O2")
set(LIBTINS_ENABLE_CXX11 1 CACHE BOOL "libtins: enable C++11")
set(LIBTINS_BUILD_SHARED 0 CACHE BOOL "libtins: build static library")
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../libtins ${CMAKE_CURRENT_BINARY_DIR}/libtins)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../redis-cpp ${CMAKE_CURRENT_BINARY_DIR}/redis-cpp)
add_subdirectory(parser)
include_directories(include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../libtins/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../redis-cpp/include)
include_directories(parser)
add_executable(capture capture.cpp)
target_link_libraries(capture parser tins redis-cpp pthread)
