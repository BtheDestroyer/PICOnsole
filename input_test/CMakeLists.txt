set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(PICO_DEOPTIMIZED_DEBUG On)

# initalize pico_sdk from installed location
# (note this can come from environment, CMake cache etc)
#set(PICO_SDK_PATH "/home/carlk/pi/pico/pico-sdk")

#set(CMAKE_VERBOSE_MAKEFILE ON)
project(input_test C CXX ASM)

# Add executable. Default name is the project name, version 0.1
add_executable(input_test
        src/main.cpp
)
target_compile_options(input_test PUBLIC -nostartfiles -O0)

target_compile_definitions(input_test
    PUBLIC
        _DEBUG=1
        PICO_STDOUT_MUTEX=0
    )

pico_set_program_name(input_test "input_test")
pico_set_program_version(input_test "1.0")

# Choose source and destination for standard input and output:
#   See 4.1. Serial input and output on Raspberry Pi Pico in Getting started with Raspberry Pi Pico (https://datasheets.raspberrypi.org/pico/getting-started-with-pico.pdf)
#   and 2.7.1. Standard Input/Output (stdio) Support in Raspberry Pi Pico C/C++ SDK (https://datasheets.raspberrypi.org/pico/raspberry-pi-pico-c-sdk.pdf):
pico_enable_stdio_uart(input_test 1)
pico_enable_stdio_usb(input_test 1)

#target_compile_options(input_test PUBLIC -nostartfiles -nolibc -nostdlib)
set_target_properties(input_test PROPERTIES PICO_TARGET_LINKER_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/../piconsole_program_memmap.ld)

pico_add_extra_outputs(input_test)
target_link_options(input_test PRIVATE -Wl,--no-gc-sections)

target_link_libraries(input_test PRIVATE piconsole_os_lib)
