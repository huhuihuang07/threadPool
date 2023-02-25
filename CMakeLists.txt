cmake_minimum_required(VERSION 3.26)

project(threadPool)

set(CMAKE_BUILD_TYPE "Debug")

include_directories(inc)

aux_source_directory(src SRCS_LISTS)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")

add_executable(${PROJECT_NAME} main.c ${SRCS_LISTS})