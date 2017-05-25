project(v4l2-test)
cmake_minimum_required(VERSION 2.8)
aux_source_directory(. DIR_SRCS)
add_executable(v4l2-test ${DIR_SRCS})

set (SRC_VERSION_MAJOR 1)
set (SRC_VERSION_MINOR 0)
set (SRC_VERSION_PATCH 1)
set (SRC_VERSION ${SRC_VERSION_MAJOR}.${SRC_VERSION_MINOR}.${SRC_VERSION_PATCH})
set (PROJECT_VERSION "${SRC_VERSION}")

add_definitions(-DPC)
#add_definitions(-DRPI3)
#add_definitions(-DIMX6)
add_definitions(-DDEBUG)

install(TARGETS v4l2-test DESTINATION bin)
