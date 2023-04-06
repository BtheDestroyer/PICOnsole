set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(PICO_DEOPTIMIZED_DEBUG On)

# initalize pico_sdk from installed location
# (note this can come from environment, CMake cache etc)
#set(PICO_SDK_PATH "/home/carlk/pi/pico/pico-sdk")

#set(CMAKE_VERBOSE_MAKEFILE ON)
project(piconsole_example_program C CXX ASM)

# Add executable. Default name is the project name, version 0.1
add_executable(piconsole_example_program
        src/main.cpp
)
target_compile_options(piconsole_example_program PUBLIC -nostartfiles -O0)

target_compile_definitions(piconsole_example_program
    PUBLIC
        _DEBUG=1
        PICO_STDOUT_MUTEX=0
    )

pico_set_program_name(piconsole_example_program "piconsole_example_program")
pico_set_program_version(piconsole_example_program "0.1")

# Choose source and destination for standard input and output:
#   See 4.1. Serial input and output on Raspberry Pi Pico in Getting started with Raspberry Pi Pico (https://datasheets.raspberrypi.org/pico/getting-started-with-pico.pdf)
#   and 2.7.1. Standard Input/Output (stdio) Support in Raspberry Pi Pico C/C++ SDK (https://datasheets.raspberrypi.org/pico/raspberry-pi-pico-c-sdk.pdf):
pico_enable_stdio_uart(piconsole_example_program 1)
pico_enable_stdio_usb(piconsole_example_program 1)

#target_compile_options(piconsole_example_program PUBLIC -nostartfiles -nolibc -nostdlib)
set_target_properties(piconsole_example_program PROPERTIES PICO_TARGET_LINKER_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/../piconsole_program_memmap.ld)

pico_add_extra_outputs(piconsole_example_program)
target_link_options(piconsole_example_program PRIVATE -Wl,--no-gc-sections)

target_link_libraries(piconsole_example_program PRIVATE piconsole_os_lib)
