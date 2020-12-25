cmake_minimum_required(VERSION 3.19)
project(GifEncoder)

set(CMAKE_CXX_STANDARD 11)

include_directories(egif)

file(GLOB_RECURSE EGIF_SOURCES "egif/*.h" "egif/*.cpp")

add_library(egif STATIC ${EGIF_SOURCES})

add_executable(egif_demo "demo.cpp")
add_executable(egif_test "test.cpp")

target_link_libraries(egif_demo egif)
target_link_libraries(egif_test egif)
