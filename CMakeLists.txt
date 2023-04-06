cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(piconsole C CXX ASM)

pico_sdk_init()

add_subdirectory(os)
add_subdirectory(example_program)
add_subdirectory(input_test)
