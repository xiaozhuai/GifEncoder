cmake_minimum_required(VERSION 3.19)
project(GifEncoder)

set(CMAKE_CXX_STANDARD 11)

file(GLOB_RECURSE EGIF_SOURCES "gif/*.h" "gif/*.cpp")

add_library(egif STATIC ${EGIF_SOURCES})

add_executable(egif_demo "main.cpp" "stb_image.h")

target_link_libraries(egif_demo egif)
