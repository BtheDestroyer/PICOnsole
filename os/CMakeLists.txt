set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(PICO_DEOPTIMIZED_DEBUG On)

project(piconsole_os C CXX ASM)

add_subdirectory(libs/FatFS_SD/FatFs_SPI)

# set(CMAKE_VERBOSE_MAKEFILE ON)

add_library(piconsole_os_lib
    "src/main.cpp"
    "src/OS.cpp"
    "src/program.cpp"
    "src/PICOnsole.cpp"
    "src/gfx/sprite.cpp"
    "src/gfx/text.cpp"
    "src/gfx/typefaces/ascii_5px.cpp"
    "src/interfaces/Input.cpp"
    "src/interfaces/LCD.cpp"
    "src/interfaces/SD.cpp"
    "src/interfaces/Speaker.cpp"
    "src/interfaces/Vibrator.cpp"
)
pico_generate_pio_header(piconsole_os_lib ${CMAKE_CURRENT_LIST_DIR}/src/i2s.pio)
target_include_directories(piconsole_os_lib
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/inc
        ${CMAKE_CURRENT_LIST_DIR}/build/os
)
target_compile_definitions(piconsole_os_lib
    PUBLIC
        LCD_MODEL=PicoLCD_1_8
        SD_SPI=spi0
    
    PRIVATE
        _DEBUG=1
        _PICONSOLE_OS=1
        _PICONSOLE_OS_LIB=1

        LCD_SPI=spi1
        
        LCD_CS=9
        LCD_CS_ACTIVE_POLARITY=0
        LCD_RST=12
        LCD_SCK=10
        LCD_MOSI=11
        LCD_DC=8
        LCD_BACKLIGHT=13

        SD_CS=5
        SD_CS_ACTIVE_POLARITY=0
        SD_SCK=2
        SD_MOSI=3
        SD_MISO=4

        "BUTTON_CATHODES=16,17,18"
        "BUTTON_ANODES=19,20,21"

        I2S_CLK=14
        I2S_LRC=15
        I2S_DATA=22

        VIBRATION_PIN=6
    
    INTERFACE
        _DEBUG=1
        _PICONSOLE_PROGRAM=1
    )
#target_compile_options(piconsole_os_lib PUBLIC -Ofast)
target_compile_options(piconsole_os_lib PRIVATE -Og)

pico_enable_stdio_usb(piconsole_os_lib 0)
pico_enable_stdio_uart(piconsole_os_lib 1)

set_target_properties(piconsole_os_lib PROPERTIES PICO_TARGET_LINKER_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/piconsole_os_memmap.ld)

target_compile_options(piconsole_os_lib PUBLIC -Wno-psabi)
target_link_options(piconsole_os_lib PUBLIC -Wl,--print-memory-usage)

target_link_libraries(piconsole_os_lib
    PUBLIC
        pico_stdlib pico_multicore
        hardware_pwm hardware_spi hardware_dma
        hardware_flash hardware_resets hardware_structs
        hardware_pio
        cmsis_core
        FatFs_SPI)

add_executable(piconsole_os)
pico_enable_stdio_usb(piconsole_os 0)
pico_enable_stdio_uart(piconsole_os 1)
set_target_properties(piconsole_os PROPERTIES PICO_TARGET_LINKER_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/piconsole_os_memmap.ld)
pico_add_extra_outputs(piconsole_os)
target_compile_definitions(piconsole_os_lib
    PRIVATE
        _PICONSOLE_OS=1
        _PICONSOLE_PROGRAM=0
    )
target_link_libraries(piconsole_os PRIVATE piconsole_os_lib)
