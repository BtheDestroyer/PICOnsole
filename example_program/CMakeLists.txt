# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

# initalize pico_sdk from installed location
# (note this can come from environment, CMake cache etc)
#set(PICO_SDK_PATH "/home/carlk/pi/pico/pico-sdk")

# Pull in Raspberry Pi Pico SDK (must be before project)
include("../pico_sdk_import.cmake")

project(piconsole_example_program C CXX ASM)
#set(CMAKE_VERBOSE_MAKEFILE ON)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1
add_executable(piconsole_example_program
        src/main.cpp
)
target_compile_options(piconsole_example_program PUBLIC -nostartfiles -fPIC -pie -fPIC -Os)
target_include_directories(piconsole_example_program
    PUBLIC
        ../inc
)

target_compile_definitions(piconsole_example_program
    PUBLIC
        _DEBUG=1

        LCD_MODEL=PicoLCD_1_8
    )

pico_set_program_name(piconsole_example_program "piconsole_example_program")
pico_set_program_version(piconsole_example_program "0.1")

# Choose source and destination for standard input and output:
#   See 4.1. Serial input and output on Raspberry Pi Pico in Getting started with Raspberry Pi Pico (https://datasheets.raspberrypi.org/pico/getting-started-with-pico.pdf)
#   and 2.7.1. Standard Input/Output (stdio) Support in Raspberry Pi Pico C/C++ SDK (https://datasheets.raspberrypi.org/pico/raspberry-pi-pico-c-sdk.pdf):
pico_enable_stdio_uart(piconsole_example_program 1)
pico_enable_stdio_usb(piconsole_example_program 1)

target_link_libraries(piconsole_example_program pico_stdlib hardware_pwm hardware_spi hardware_dma FatFs_SPI)